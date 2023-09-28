#include "vendor/flashmq_plugin.h"
#include <dbus-1.0/dbus/dbus.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <mutex>
#include <unistd.h>
#include <string.h>
#include <thread>

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
}

void flashmq_plugin_deinit(void *thread_data, std::unordered_map<std::string, std::string> &plugin_opts, bool reloading)
{

}

AuthResult flashmq_plugin_login_check(void *thread_data, const std::string &clientid, const std::string &username, const std::string &password,
                                      const std::vector<std::pair<std::string, std::string>> *userProperties, const std::weak_ptr<Client> &client)
{
    return AuthResult::success;
}

bool flashmq_plugin_alter_publish(void *thread_data, const std::string &clientid, std::string &topic, const std::vector<std::string> &subtopics,
                                  std::string_view payload, uint8_t &qos, bool &retain, std::vector<std::pair<std::string, std::string>> *userProperties)
{
    if (retain)
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

        if (access == AclAccess::write)
        {
            const std::string &action = subtopics.at(0);

            // Wo only work on strings like R/<portalid>/system/0/Serial.
            if (action == "W" || action == "R")
            {
                const std::string &system_id = subtopics.at(1);

                if (system_id != state->unique_vrm_id)
                {
                    flashmq_logf(LOG_ERR, "We received a request for '%s', but that's not us (%s)", system_id.c_str(), state->unique_vrm_id.c_str());
                    return AuthResult::success;
                }

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
                // Deal with bridge notifications like $SYS/broker/bridge/GXdbus/connected

                const std::string &bridgeName = subtopics.at(3);
                const std::string payload_str(payload);

                if ((bridgeName == "GXdbus" || bridgeName == "GXrpc"))
                {
                    if (payload_str == "1")
                    {
                        state->bridge_connected = true;
                    }
                    else if (state->bridge_connected && payload_str == "0" && state->register_pending_id == 0)
                    {
                        state->bridge_connected = false;

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
            flashmq_logf(LOG_WARNING, "dbus_watch_handle() returns false, so is out of memory.");
        }
    }
}
