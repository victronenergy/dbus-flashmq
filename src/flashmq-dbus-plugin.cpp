#include "vendor/flashmq_plugin.h"
#include <dbus-1.0/dbus/dbus.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <mutex>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <fstream>

#include "state.h"
#include "utils.h"

// https://dbus.freedesktop.org/doc/api/html/index.html

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
}

void flashmq_plugin_deinit(void *thread_data, std::unordered_map<std::string, std::string> &plugin_opts, bool reloading)
{

}

AuthResult flashmq_plugin_login_check(void *thread_data, const std::string &clientid, const std::string &username, const std::string &password,
                                      const std::vector<std::pair<std::string, std::string>> *userProperties, const std::weak_ptr<Client> &client)
{
    State *state = static_cast<State*>(thread_data);

    FlashMQSockAddr addr;
    memset(&addr, 0, sizeof(FlashMQSockAddr));
    flashmq_get_client_address(client, nullptr, &addr);

    if (state->match_local_net(addr.getAddr()))
    {
        return AuthResult::success;
    }

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
        flashmq_logf(LOG_ERR, "Error in trying to read '%s': %s", vnc_password_file_path.c_str(), ex.what());
    }

    return AuthResult::login_denied;
}

bool flashmq_plugin_alter_publish(void *thread_data, const std::string &clientid, std::string &topic, const std::vector<std::string> &subtopics,
                                  std::string_view payload, uint8_t &qos, bool &retain, std::vector<std::pair<std::string, std::string>> *userProperties)
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

/**
 * @brief using ACL hook as 'on_message' handler.
 * @return We always return 'success', otherwise the message is blocked, and you wouldn't see it with any connecting subscriber.
 */
AuthResult flashmq_plugin_acl_check(void *thread_data, const AclAccess access, const std::string &clientid, const std::string &username,
                                    const std::string &topic, const std::vector<std::string> &subtopics, std::string_view payload,
                                    const uint8_t qos, const bool retain, const std::vector<std::pair<std::string, std::string>> *userProperties)
{
    try
    {
        State *state = static_cast<State*>(thread_data);

        if (subtopics.size() < 2)
            return AuthResult::success;

        const std::string &action = subtopics.at(0);

        if (access == AclAccess::write)
        {
            // Wo only work on strings like R/<portalid>/system/0/Serial.
            if (action == "W" || action == "R")
            {
                const std::string &system_id = subtopics.at(1);

                if (system_id != state->unique_vrm_id)
                {
                    flashmq_logf(LOG_ERR, "We received a request for '%s', but that's not us (%s)", system_id.c_str(), state->unique_vrm_id.c_str());
                    return AuthResult::success;
                }

                /*
                 * Because we also need to respond to reads and writes from remote clients when the system is not alive, we need
                 * to consider any AclAccess::write activity as interest.
                 */
                if (client_id_is_bridge(clientid))
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
                 * Deal with bridge notifications like $SYS/broker/bridge/GXdbus/connected
                 *
                 * Disconnection is a good time to re-register ourselves on VRM, because there is a small chance our
                 * registration has been reset by the user at some point in the past, which would only be seen when
                 * making a new connection.
                 */

                const std::string &bridgeName = subtopics.at(3);
                const std::string payload_str(payload);

                if ((bridgeName == "GXdbus" || bridgeName == "GXrpc"))
                {
                    bool &connected = state->bridges_connected[bridgeName];

                    if (payload_str == "1")
                    {
                        connected = true;
                    }
                    else if (connected && payload_str == "0" && state->register_pending_id == 0)
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
                }
            }
        }
        else if (access == AclAccess::read)
        {
            // We only limit our own N (notifications), to avoid accidentally denying other things.
            if (action != "N")
                return AuthResult::success;

            // We still allow normal cross-client behavior when it's all on LAN.
            if (!client_id_is_bridge(clientid))
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

    // I choose to let this function spin in case we are out of memory (i.e. not do anything). Otherwise I run the risk of
    // taking the fd out of epoll and not adding it again. And the DBusWatch logic is weird enough to deal with.

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
