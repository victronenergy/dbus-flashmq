#include "vendor/flashmq_plugin.h"
#include <dbus-1.0/dbus/dbus.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <mutex>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <fstream>
#include <optional>

#include "state.h"
#include "utils.h"

// https://dbus.freedesktop.org/doc/api/html/index.html

#define DBUS_MQTT_INTEGRATIONS_USERNAME "dbus-mqtt-integrations"

using namespace dbus_flashmq;

int flashmq_plugin_version()
{
    return FLASHMQ_PLUGIN_VERSION;
}

void flashmq_plugin_allocate_thread_memory(void **thread_data, std::unordered_map<std::string, std::string> &plugin_opts)
{
    State *state = new State();
    *thread_data = state;
}

void flashmq_plugin_deallocate_thread_memory(void *thread_data, std::unordered_map<std::string, std::string> &plugin_opts)
{
    State *state = static_cast<State*>(thread_data);
    delete state;
}

void flashmq_plugin_main_init(std::unordered_map<std::string, std::string> &plugin_opts)
{
    dbus_threads_init_default();
}

void flashmq_plugin_init(void *thread_data, std::unordered_map<std::string, std::string> &plugin_opts, bool reloading)
{
    State *state = static_cast<State*>(thread_data);

    if (reloading)
        return;

    state->get_unique_id();
    state->init_home_assistant_discovery();

    // Venus never skips it, but the Docker development env does.
    auto skip_broker_reg_pos = plugin_opts.find("skip_broker_registration");
    if (skip_broker_reg_pos != plugin_opts.end() && skip_broker_reg_pos->second == "true")
    {
        state->do_online_registration = false;
    }

    state->initiate_broker_registration(0);

    state->open();
    state->scan_all_dbus_services();

    // Indicate that the new keepalive mechanism is supported
    std::ostringstream keepalive_topic;
    keepalive_topic << "N/" << state->unique_vrm_id << "/keepalive";
    flashmq_publish_message(keepalive_topic.str(), 0, false, "1");

    state->start_one_second_timer();
    state->start_one_minute_timer();
}

void flashmq_plugin_deinit(void *thread_data, std::unordered_map<std::string, std::string> &plugin_opts, bool reloading)
{
    // As of yet, we don't do reload actions.
    if (reloading)
        return;

    if (!thread_data)
        return;

    State *state = static_cast<State*>(thread_data);

    state->ha_discovery.clearAll();

    /*
     *  These are async calls and because we don't have an event loop anymore at this point, it may be that the
     *  call is never sent, when dbus buffers are full for instance. It's a rare occurance though, and not
     *  easily fixed.
     */
    state->write_bridge_connection_state(BRIDGE_DBUS, std::optional<bool>(), BRIDGE_DEACTIVATED_STRING);
    state->write_bridge_connection_state(BRIDGE_RPC, std::optional<bool>(), BRIDGE_DEACTIVATED_STRING);
}

AuthResult auth_success_or_delayed_fail(
        State *state, const std::weak_ptr<Client> &client,
        const std::string &username, const std::string &clientid, AuthResult result)
{
    if (result == AuthResult::success)
    {
        state->register_user_and_clientid(username, clientid);
        return AuthResult::success;
    }

    auto f = [client, result]() {
        flashmq_continue_async_authentication(client, result, "", "");
    };
    const uint32_t delay = get_random<uint32_t>() % 5000 + 1000;
    flashmq_add_task(f, delay);
    flashmq_logf(LOG_NOTICE, "Sending delayed deny for login with '%s'", username.c_str());
    return AuthResult::async;
}

AuthResult do_vnc_auth(const std::string &password)
{
    const static std::string vnc_password_file_path = "/data/conf/vncpassword.txt";

    try
    {
        if (!std::filesystem::exists(vnc_password_file_path))
            return AuthResult::login_denied;

        if (std::filesystem::file_size(vnc_password_file_path) == 0)
            return AuthResult::success;

        std::fstream vnc_password_file(vnc_password_file_path, std::ios::in);

        if (!vnc_password_file)
        {
            std::string error_str(strerror(errno));
            throw std::runtime_error(error_str);
        }

        std::string vnc_password_crypt;

        if (!getline(vnc_password_file, vnc_password_crypt))
        {
            std::string error_str(strerror(errno));
            throw std::runtime_error(error_str);
        }

        trim(vnc_password_crypt);

        // The file is normally 0 bytes, but disabling it again makes it 1 byte, with a newline. This is also approved.
        if (vnc_password_crypt.empty())
            return AuthResult::success;

        if (crypt_match(password, vnc_password_crypt))
            return AuthResult::success;
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, "Error in do_vnc_auth using '%s': %s", vnc_password_file_path.c_str(), ex.what());
    }

    return AuthResult::login_denied;
}

/**
 * These are tokens set for pairing, like:
 *
 *   curl --insecure --user "remoteconsole:securityprofilepassword" -X POST \
 *     -d "role=evcharger&device_id=mydevice123" 'https://venus.local/auth/generate-token/'
 *
 * Answer: {"token_name":"token/evcharger/mydevice123","password":"hfNWPw7UmCKg4CiSZ1CKnnkjlBbArStO"}
 *
 * Example tokens file:
 *
 * [
 *    {
 *       "password_hash": "$2y$10$SIQlG707GEsg2J6ek.FH/Od9DEr8NeBC/1xdNmdz3So7ilwKbOUw6",
 *       "token_name": "token/evcharger/mydevice123"
 *    }
 * ]
 *
 */
std::optional<AuthResult> do_token_auth(const std::string &username, const std::string &password)
{
    try
    {
        const static std::string token_users_file = "/data/conf/tokens.json";

        if (!std::filesystem::exists(token_users_file))
            return {};

        std::ifstream infile(token_users_file);

        if (!infile.is_open())
        {
            throw std::runtime_error("Error opening " + token_users_file);
        }

        std::stringstream buffer;
        buffer << infile.rdbuf();
        std::string json_text = buffer.str();

        const nlohmann::json j = nlohmann::json::parse(json_text);

        if (!j.is_array())
            throw std::runtime_error("The file '" + token_users_file + "' is not valid json (array)");

        for (auto &row : j)
        {
            const std::string &token_name = row.at("token_name");
            const std::string &password_hash = row.at("password_hash");

            if (username == token_name)
            {
                const bool match = crypt_match(password, password_hash);
                return match ? AuthResult::success : AuthResult::acl_denied;
            }
        }

        return {};
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, "Error in do_token_auth: %s", ex.what());
    }

    return {};
}

AuthResult flashmq_plugin_login_check(
    void *thread_data, const std::string &clientid, const std::string &username, const std::string &password,
    const std::vector<std::pair<std::string, std::string>> *userProperties, const std::weak_ptr<Client> &client)
{
    if (!thread_data)
        return AuthResult::error;

    State *state = static_cast<State*>(thread_data);
    const bool localhost_login = state->localhost_client(client);

    /*
     * The local_username of the bridge does not go through flashmq_plugin_login_check(), so seeing this username
     * is always an imposter.
     */
    if (username_is_bridge(username))
    {
        return AuthResult::acl_denied;
    }

    if (username == DBUS_MQTT_INTEGRATIONS_USERNAME)
    {
        // We assume elsewhere that this user is localhost, so we must force it.
        AuthResult r = localhost_login ? AuthResult::success : AuthResult::login_denied;
        return auth_success_or_delayed_fail(state, client, username, clientid, r);
    }

    if (localhost_login)
    {
        // Localhost is / can be a connection that is authenticated by Nginx, meaning GUIv2.
        state->privileged_network_clients.insert(client);

        return auth_success_or_delayed_fail(state, client, username, clientid, AuthResult::success);
    }

    // Tokens are not subject to rate-limiting. We control their entropy, and rate-limiting is not necessary.
    const std::optional<AuthResult> token_auth_result = do_token_auth(username, password);
    if (token_auth_result)
    {
        return auth_success_or_delayed_fail(state, client, username, clientid, token_auth_result.value());
    }

    /*
     * The normal security profile password has no specific username. So, we block any integration token that does
     * not appear in our user database (the do_token_auth check above this).
     *
     * This also means that in 'unsecured' mode, tokens still require a password, even though the normal
     * mqtt access doesn't.
     */
    if (username.rfind("token/", 0) == 0)
    {
        return auth_success_or_delayed_fail(state, client, username, clientid, AuthResult::login_denied);
    }

    if (state->loginTokensShortTerm <= 0 || state->loginTokensLongTerm <= 0)
    {
        flashmq_logf(LOG_WARNING, "Login rate-limited: short-term-left=%d long-term-left=%d", state->loginTokensShortTerm, state->loginTokensLongTerm);
        return auth_success_or_delayed_fail(state, client, username, clientid, AuthResult::login_denied);
    }

    if (do_vnc_auth(password) == AuthResult::success)
    {
        // The VNC password is the security profile password, so it's privileged.
        state->privileged_network_clients.insert(client);

        return auth_success_or_delayed_fail(state, client, username, clientid, AuthResult::success);
    }

    /*
     * We only count unseen attempts. When some integration uses a wrong/changed password and keeps trying, we
     * should not trip the rate limiter.
     */
    if (state->passwordHistory.count(password) == 0)
    {
        state->decrement_login_tokens();

        if (state->passwordHistory.size() < LOGIN_TOKENS_LONG_TERM) // protect memory use
            state->passwordHistory.insert(password);
    }

    return auth_success_or_delayed_fail(state, client, username, clientid, AuthResult::login_denied);
}

bool flashmq_plugin_alter_publish(void *thread_data, const std::string &clientid, std::string &topic, const std::vector<std::string> &subtopics,
                                  std::string_view payload, uint8_t &qos, bool &retain, const std::optional<std::string> &correlationData,
                                  const std::optional<std::string> &responseTopic, std::vector<std::pair<std::string, std::string>> *userProperties)
{
    State *state = static_cast<State*>(thread_data);

    if (!state)
        return false;

    if (subtopics.size() < 2)
        return false;

    /*
     * This matches the 'publish' lines in /data/conf/flashmq.d/vrm_bridge.conf. It should/can also never happen
     * in practice on Venus. However, we're making sure that:
     *
     * 1) The internet MQTT servers are never given a retained message, even if we receive a stray retained message
     *    from a local client that matches one of our own topic paths.
     * 2) We don't touch other topics that don't involve us, so the MQTT server on Venus can be used for non-Venus
     *    things in a network.
     */
    if (retain && (subtopics.at(0) == "N" || subtopics.at(0) == "P" ) && subtopics.at(1) == state->unique_vrm_id)
    {
        retain = false;
        return true;
    }

    return false;
}

void handle_venus_actions(
    State *state, const std::string &action, const std::string &system_id, const std::string &username,
    const std::string &topic, const std::vector<std::string> &subtopics, std::string_view payload)
{
    // Wo only work on strings like R/<portalid>/system/0/Serial.
    if (action == "W" || action == "R")
    {
        /*
         * Because we also need to respond to reads and writes from remote clients when the system is not alive, we need
         * to consider any AclAccess::write activity as interest.
         */
        if (username_is_bridge(username))
            state->vrmBridgeInterestTime = std::chrono::steady_clock::now();

        // There's also 'P' for mqtt-rpc, but we should ignore that, and not report it.
        if (action == "W")
        {
            std::string payload_str(payload);
            state->write_to_dbus(topic, payload_str);
        }
        else if (action == "R")
        {
            std::string payload_str(payload);
            const std::string path = splitToVector(topic, '/', 2).at(2);
            if (path == "system/0/Serial" || path == "keepalive")
            {
                state->handle_keepalive(payload_str);
            }

            if (path != "keepalive")
            {
                state->handle_read(topic);
            }
        }
    }
    else if (action == "$SYS" && subtopics.size() >= 5)
    {
        /*
         * Deal with bridge notifications like:
         *
         * * $SYS/broker/bridge/GXdbus/connected
         * * $SYS/broker/bridge/GXdbus/connection_status
         *
         * Disconnection is a good time to re-register ourselves on VRM, because there is a small chance our
         * registration has been reset by the user at some point in the past, which would only be seen when
         * making a new connection.
         */

        const std::string &bridgeName = subtopics.at(3);
        const std::string &which = subtopics.at(4);
        const std::string payload_str(payload);

        if (bridgeName == BRIDGE_DBUS || bridgeName == BRIDGE_RPC)
        {
            if (which == "connected")
            {
                bool &connected = state->bridges_connected[bridgeName];
                const bool connected_now = payload_str == "1";

                if (connected_now)
                {
                    connected = true;
                }
                else if (connected && !connected_now && state->register_pending_id == 0)
                {
                    connected = false;

                    /*
                     * Note that for the majority of users, this re-registration is not needed and it will just reconnect. We need to do it
                     * just in case (for installations that may have had their tokens reset), and we can allow some random wait time
                     * to prevent DDOS on the registration server.
                     */
                    const uint32_t delay = get_random<uint32_t>() % 600000;

                    flashmq_logf(LOG_NOTICE, "Bridge '%s' disconnect detected. We will initiate a re-registration in %d ms.", bridgeName.c_str(), delay);

                    state->initiate_broker_registration(delay);
                }

                state->bridge_connection_states[bridgeName].connected = connected_now;
            }
            else if (which == "connection_status")
            {
                state->bridge_connection_states[bridgeName].msg = payload_str;
            }

            state->write_all_bridge_connection_states_debounced();
        }
    }
}

/**
 * Handles authorization checks for MQTT-integrated devices. As dbus-flashmq, we don't actually do anything with it. We only
 * authorize.
 *
 * The authorization is implicit based on topic names and the username (which was approved on login). So, no permission
 * databases need to be checked.
 *
 * Examples of pairing:
 * - I/local/in/evcharger/HQ2401ABCDE/vregset
 * - I/local/out/evcharger/HQ2401ABCDE/vregset
 * - I/local/in/evcharger/HQ2401ABCDE/vregnotify
 *
 * Username example from the connecting client: token/evcharger/HQ2501ABCDE
 */
AuthResult handle_paired_integration_client_auth(
    State *state, const AclAccess access, const std::string &clientid, const std::string &username,
    const std::string &topic, const std::vector<std::string> &subtopics)
{
    if (access == AclAccess::subscribe)
        return AuthResult::success;

    if (subtopics.size() < 5)
        return AuthResult::acl_denied;

    const std::string &direction = subtopics.at(2);

    if (username == DBUS_MQTT_INTEGRATIONS_USERNAME)
    {
        if ((access == AclAccess::write && direction == "out") || (access == AclAccess::read && direction == "in" ))
            return AuthResult::success;

        return AuthResult::acl_denied;
    }

    if (access == AclAccess::write && direction == "out")
        return AuthResult::acl_denied;

    if (access == AclAccess::read && direction == "in")
        return AuthResult::acl_denied;

    if (username.rfind("token/", 0) != 0)
        return AuthResult::acl_denied;

    const std::vector<std::string> user_fields = splitToVector(username, '/');

    if (user_fields.size() != 3)
        return AuthResult::acl_denied;

    const std::string &unique_id_from_user = user_fields.at(2);
    const std::string &unique_id_from_topic = subtopics.at(4);
    const std::string &role_from_user = user_fields.at(1);
    const std::string &role_from_topic = subtopics.at(3);

    if (unique_id_from_user.empty() || unique_id_from_topic.empty())
        return AuthResult::acl_denied;

    if (unique_id_from_user == unique_id_from_topic && role_from_user == role_from_topic)
        return AuthResult::success;

    return AuthResult::acl_denied;
}

/**
 * @brief using ACL hook as 'on_message' handler.
 */
AuthResult flashmq_plugin_acl_check(void *thread_data, const AclAccess access, const std::string &clientid, const std::string &username,
                                    const std::string &topic, const std::vector<std::string> &subtopics, const std::string &shareName,
                                    std::string_view payload, const uint8_t qos, const bool retain,
                                    const std::optional<std::string> &correlationData, const std::optional<std::string> &responseTopic,
                                    const std::vector<std::pair<std::string, std::string>> *userProperties)
{
    if (access == AclAccess::subscribe || access == AclAccess::register_will)
        return AuthResult::success;

    try
    {
        State *state = static_cast<State*>(thread_data);

        if (subtopics.size() < 2)
            return AuthResult::success;

        const std::string &action = subtopics.at(0);
        const std::string &system_id = subtopics.at(1);

        /*
         * Can be like:
         *
         * - I/local/in/evcharger/HQ2401ABCDE/vregset
         * - I/<portalid>/in/ev/<vin>/heartbeat (which is only done over VRM, not LAN).
         *
         * We block integration access to users logged in using the normal security profile password on LAN. This is
         * to avoid integrators using that password mistakenly.
         *
         * Allowing localhost still allows diagnostic use with a local client.
         *
         * Obtaining whether a client is localhost takes a bit of work, so we only do it conditionally, even though
         * to the eye, it may make more sense to do it earlier.
         */
        if (action == "I")
        {
            /*
             * This block will always return an answer. Unlike the rest of I; see below.
             */
            if (system_id == "local")
            {
                AuthResult preliminary_result = handle_paired_integration_client_auth(state, access, clientid, username, topic, subtopics);

                if (preliminary_result == AuthResult::acl_denied)
                {
                    const IsPrivilegedUser priv = state->is_privileged_user(clientid, username);

                    if (state->localhost_client(priv.client))
                    {
                        preliminary_result = AuthResult::success;
                    }
                }

                return preliminary_result;
            }

            /*
             * This block only has the chance to deny, not allow. The reason is that the rest of this function will
             * perform the proper checks, like VRM read-only mode, or receiving messages for another portal ID.
             */
            if (!(username == DBUS_MQTT_INTEGRATIONS_USERNAME || username_is_bridge(username)))
            {
                const IsPrivilegedUser priv = state->is_privileged_user(clientid, username);

                if (!state->localhost_client(priv.client))
                {
                    return AuthResult::acl_denied;
                }
            }
        }

        if (access == AclAccess::write)
        {
            if (action == "W" && subtopics.size() >= 3 && subtopics.at(2) == "platform" && topic.find("/Security/Api") != std::string::npos)
            {
                if (!state->is_privileged_user(clientid, username))
                {
                    flashmq_logf(LOG_WARNING, "Attempt to invoke platform security API by non-privileged user.");
                    return AuthResult::acl_denied;
                }
            }

            if ((action == "W" || action == "R" || action == "P" || action == "I") && system_id != state->unique_vrm_id)
            {
                flashmq_logf(LOG_ERR, "We received a '%s' request for '%s', but that's not us (but %s)",
                             action.c_str(), system_id.c_str(), state->unique_vrm_id.c_str());

                /*
                 * With W and R we are normally the one acting on that, so we can still relay them to any
                 * subscribers. But for P and I, we are ensuring nobody gets those to avoid other Venus services
                 * mistakingly acting on them.
                 */
                if (action == "W" || action == "R")
                    return AuthResult::success;
                else
                    return AuthResult::acl_denied;
            }

            /*
             * We also block P/<portalid>/in for safety; that is currently also covered by not having an RPC bridge
             * connection in read-only mode, but that may not be a separate connection in the future anymore.
             */
            if (action == "W" || ((action == "P" || action == "I") && subtopics.size() >= 3 && subtopics.at(2) == "in"))
            {
                if (username_is_bridge(username) && state->vrm_portal_mode != VrmPortalMode::Full)
                    return AuthResult::acl_denied;
            }

            /*
             * Only allow ourselves to write N messages. This avoids people's own integrations from publishing
             * values they are not supposed to, which can be old, wrong, etc.
             */
            if (action == "N" && !(clientid.empty() && username.empty()) && state->unique_vrm_id == system_id)
            {
                if (!state->warningAboutNTopicsLogged)
                {
                    state->warningAboutNTopicsLogged = true;
                    flashmq_logf(LOG_WARNING,
                        "Received external publish on N topic: '%s'. "
                        "This is unexpected and probably a misconfigured integration. Blocking this and later ones.",
                        topic.c_str());
                }

                return AuthResult::acl_denied;
            }

            // The rest is not auth as such, but take actions based on the messages.
            handle_venus_actions(state, action, system_id, username, topic, subtopics, payload);
        }
        else if (access == AclAccess::read)
        {
            const char action_char = action.length() == 1 ? action[0] : 0;

            /*
             * Just means other MQTT clients can't see it. Doesn't affect ourselves or dbus communications.
             *
             * Denying all W reads is not really possible anymore. Our own apps don't use those, but custom
             * integrations might.
             */
            if (action_char == 'W')
            {
                if (subtopics.size() >= 3 && subtopics.at(2) == "platform" && topic.find("/Security/Api") != std::string::npos)
                    return AuthResult::acl_denied;

                if (subtopics.size() >= 7 && subtopics.at(2) == "settings" && subtopics.at(6) == "AccessPointPassword")
                    return AuthResult::acl_denied;

                std::string lower_topic = topic;
                str_make_lower(lower_topic);

                if (lower_topic.find("password") != std::string::npos)
                    return AuthResult::acl_denied;
            }

            /*
             * The if-statements below stop traffic over the bridge if there is no VRM interest. However, we only
             * limit our own N (notifications), to avoid accidentally denying other things.
             */
            if (action_char != 'N')
                return AuthResult::success;

            // We still allow normal cross-client behavior when it's all on LAN.
            if (!username_is_bridge(username))
                return AuthResult::success;

            if (std::chrono::steady_clock::now() > state->vrmBridgeInterestTime + std::chrono::seconds(VRM_INTEREST_TIMEOUT_SECONDS))
                return AuthResult::acl_denied;

            return AuthResult::success;
        }
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, "Error in flashmq_plugin_acl_check when handling '%s': %s", topic.c_str(), ex.what());
    }

    return AuthResult::success;
}

void flashmq_plugin_poll_event_received(void *thread_data, int fd, uint32_t events, const std::weak_ptr<void> &p)
{
    //flashmq_logf(LOG_DEBUG, "flashmq_plugin_poll_event_received. Epoll flags: %d", events);

    State *state = static_cast<State*>(thread_data);

    if (fd == state->dispatch_event_fd)
    {
        uint64_t eventfd_value = 0;
        if (read(fd, &eventfd_value, sizeof(uint64_t)) > 0)
        {
            DBusDispatchStatus dispatch_status = DBusDispatchStatus::DBUS_DISPATCH_DATA_REMAINS;
            while((dispatch_status = dbus_connection_get_dispatch_status(state->con)) == DBUS_DISPATCH_DATA_REMAINS)
            {
                dbus_connection_dispatch(state->con);
            }

            // This will make us spin, but it's a method that doesn't allocate memory.
            if (dispatch_status == DBusDispatchStatus::DBUS_DISPATCH_NEED_MEMORY)
            {
                uint64_t one = 1;
                write(state->dispatch_event_fd, &one, sizeof(uint64_t));
            }
        }
        else
        {
            const char *err = strerror(errno);
            flashmq_logf(LOG_ERR, err);
        }

        return;
    }

    std::shared_ptr<Watch> w = std::static_pointer_cast<Watch>(p.lock());

    if (!w || w->empty())
        return;

    // Since the process is supervised, just exit if there are major problems like running
    // out of memory.

    /*
     * See the 'Watch' object class doc for the reasoning behind this.
     */
    for (DBusWatch *watch : w->get_watches())
    {
        if (!dbus_watch_get_enabled(watch))
            continue;

        // Adding the implicit error flags to flags_of_watch.
        int flags_of_watch = dbus_watch_get_flags(watch) | (DBusWatchFlags::DBUS_WATCH_ERROR | DBusWatchFlags::DBUS_WATCH_HANGUP);
        int readiness_in_dbus_flags = epoll_flags_to_dbus_watch_flags(events);
        int match_flags = flags_of_watch & readiness_in_dbus_flags;

        if (!dbus_watch_handle(watch, match_flags))
        {
            // If we don't exit here, the logs are spammed with errors and we likely won't recover.
            flashmq_logf(LOG_WARNING, "dbus_watch_handle() returns false, so is out of memory. Exiting, because there's nothing else to do.");

            // The FlashMQ log writer is an async thread and we have no control or info over its commit status. Because we do want to see stuff
            // in the log, sleeping a bit...
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Not exit(), because we don't want to call destructors and stuff.
            abort();
        }
    }
}

void flashmq_plugin_periodic_event(void *thread_data)
{
    if (!thread_data)
        return;

    State *state = static_cast<State*>(thread_data);

    state->clear_expired_privileged_clients();
}







