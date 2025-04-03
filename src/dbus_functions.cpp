#include "dbus_functions.h"
#include "vendor/flashmq_plugin.h"
#include "state.h"
#include "utils.h"
#include "dbusmessageguard.h"

#include "dbusutils.h"
#include "dbuserrorguard.h"
#include "dbusmessageitersignature.h"

using namespace dbus_flashmq;

void dbus_flashmq::dbus_dispatch_status_function(DBusConnection *connection, DBusDispatchStatus new_status, void *data)
{
    State *state = static_cast<State*>(data);

    if (new_status == DBusDispatchStatus::DBUS_DISPATCH_DATA_REMAINS)
    {
        state->setDispatchable();
    }
}

dbus_bool_t dbus_flashmq::dbus_add_watch_function(DBusWatch *watch, void *data)
{
    State *state = static_cast<State*>(data);

    const int fd = dbus_watch_get_unix_fd(watch);
    std::shared_ptr<Watch> &w = state->watches[fd];

    if (!w)
        w = std::make_shared<Watch>();

    w->fd = fd;
    w->add_watch(watch);

    try
    {
        int epoll_events = w->get_combined_epoll_flags();
        flashmq_poll_add_fd(fd, epoll_events, w);
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, ex.what());
        return false;
    }

    return true;
}

void dbus_flashmq::dbus_remove_watch_function(DBusWatch *watch, void *data)
{
    State *state = static_cast<State*>(data);
    const int fd = dbus_watch_get_unix_fd(watch);

    auto pos = state->watches.find(fd);
    if (pos == state->watches.end())
        return;

    std::shared_ptr<Watch> &w = pos->second;

    if (!w)
        return;

    w->remove_watch(watch);

    try
    {
        if (w->empty())
            flashmq_poll_remove_fd(fd);
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, ex.what());
    }
}

void dbus_flashmq::dbus_toggle_watch_function(DBusWatch *watch, void *data)
{
    // I'm not fully sure what this callback wants me to do, but I'm guessing changing the EPOLL events I'm watching for,
    // which in FlashMQ is the same function.
    dbus_add_watch_function(watch, data);
}

void dbus_flashmq::dbus_timeout_do_handle(DBusTimeout *timeout)
{
    dbus_bool_t result = dbus_timeout_handle(timeout);

    // FlashMQ's tasks are one-shot, so dbus's suggestion to let the timeout fire again needs to be explicitly queued.
    if (!result)
    {
        auto f = std::bind(&dbus_timeout_do_handle, timeout);
        int interval = dbus_timeout_get_interval(timeout);

        flashmq_add_task(f, interval);
    }
}

dbus_bool_t dbus_flashmq::dbus_add_timeout_function(DBusTimeout *timeout, void *data)
{
    auto f = std::bind(&dbus_timeout_do_handle, timeout);
    int interval = dbus_timeout_get_interval(timeout);

    try
    {
        // Just storing the id as address, because it saves allocation.
        uint32_t id = flashmq_add_task(f, interval);
        int *id2 = reinterpret_cast<int*>(id);
        dbus_timeout_set_data(timeout, id2, nullptr);
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, ex.what());
        return false;
    }

    return true;
}

void dbus_flashmq::dbus_remove_timeout_function(DBusTimeout *timeout, void *data)
{
    try
    {
        int *id2 = static_cast<int*>(dbus_timeout_get_data(timeout));
        uint32_t id = reinterpret_cast<intptr_t>(id2);
        flashmq_remove_task(id);
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, ex.what());
    }
}

void dbus_flashmq::dbus_toggle_timeout_function(DBusTimeout *timeout, void *data)
{
    // Say whaaaat?!
    //flashmq_logf(LOG_ERR, "What do I do here?");
}

/**
 * @brief dbus_pending_call_notify is called by dbus when a reply is received, or when it times out.
 * @param pending
 * @param data
 *
 * When the timeout happens, the handler is still called. The message type is 'DBUS_MESSAGE_TYPE_ERROR'
 * with 'org.freedesktop.DBus.Error.NoReply', so it will have to know how to handle that.
 *
 * Called from dbus internals, so we can't throw exceptions.
 */
void dbus_flashmq::dbus_pending_call_notify(DBusPendingCall *pending, void *data) noexcept
{
    State *state = static_cast<State*>(data);

    if (!dbus_pending_call_get_completed(pending))
    {
        flashmq_logf(LOG_ERR, "We were notified about an incomplete pending reply?");
        return;
    }

    const DBusMessageGuard msg = dbus_pending_call_steal_reply(pending);
    const dbus_uint32_t reply_to = dbus_message_get_reply_serial(msg.d);

    if (reply_to == 0)
    {
        flashmq_logf(LOG_ERR, "Dbus reply could not be matched to antyhing.");
        return;
    }

    const int msg_type = dbus_message_get_type(msg.d);

    if (!(msg_type == DBUS_MESSAGE_TYPE_METHOD_RETURN || msg_type == DBUS_MESSAGE_TYPE_ERROR))
    {
        flashmq_logf(LOG_ERR, "Pending call notification is not a method return or error. Weird?");
        return;
    }

    auto fpos = state->async_handlers.find(reply_to);

    if (fpos != state->async_handlers.end())
    {
        try
        {
            auto f = fpos->second;
            state->async_handlers.erase(fpos);
            f(msg.d);
        }
        catch (std::exception &ex)
        {
            flashmq_logf(LOG_ERR, ex.what());
        }
    }
}

DBusHandlerResult dbus_flashmq::dbus_handle_message(DBusConnection *connection, DBusMessage *message, void *user_data)
{
    const char *_signal_name = dbus_message_get_member(message);
    const std::string signal_name(_signal_name ? _signal_name : "");

    try
    {
        State *state = static_cast<State*>(user_data);
        int msg_type = dbus_message_get_type(message);

        const char *_sender = dbus_message_get_sender(message);
        std::string sender(_sender ? _sender : "");

        if (msg_type == DBUS_MESSAGE_TYPE_SIGNAL)
        {
            state->attempt_to_process_delayed_changes();

            if (signal_name == "NameAcquired")
            {
                const char *_name = nullptr;
                DBusErrorGuard err;
                dbus_message_get_args(message, err.get(), DBUS_TYPE_STRING, &_name, DBUS_TYPE_INVALID);
                err.throw_error();

                std::string name(_name);

                flashmq_logf(LOG_DEBUG, "Signal: '%s' by '%s'. Name: '%s'", signal_name.c_str(), sender.c_str(), name.c_str());
                return DBusHandlerResult::DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            }
            else if (signal_name == "NameOwnerChanged")
            {
                const char *_name = nullptr;
                const char *_oldowner = nullptr;
                const char *_newowner = nullptr;

                DBusErrorGuard err;
                dbus_message_get_args(message, err.get(), DBUS_TYPE_STRING, &_name, DBUS_TYPE_STRING, &_oldowner, DBUS_TYPE_STRING, &_newowner, DBUS_TYPE_INVALID);
                err.throw_error();

                std::string name(_name);
                std::string oldowner(_oldowner);
                std::string newowner(_newowner);

                if (name.find("com.victronenergy.") == std::string::npos)
                    return DBusHandlerResult::DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

                if (!newowner.empty())
                {
                    flashmq_logf(LOG_INFO, "[OwnerChange] Service appeared: '%s' with owner '%s'", name.c_str(), newowner.c_str());
                    state->set_new_id_to_owner(newowner, name);
                    state->scan_dbus_service(name);
                }
                else if (!oldowner.empty())
                {
                    flashmq_logf(LOG_INFO, "[OwnerChange] Service disappeared: '%s' with owner '%s'", name.c_str(), oldowner.c_str());
                    state->remove_id_to_owner(oldowner);
                    state->remove_dbus_service(name);
                }

                return DBusHandlerResult::DBUS_HANDLER_RESULT_HANDLED;
            }

            sender = state->get_named_owner(sender);
            //flashmq_logf(LOG_DEBUG, "Received signal: '%s' by '%s'", signal_name.c_str(), sender.c_str());

            if (sender.find("com.victronenergy") != std::string::npos)
            {
                // The preferred signal, containing multiple items. The format is used by both ItemsChanged and the method call GetItems.
                if (signal_name == "ItemsChanged")
                {
                    std::unordered_map<std::string, Item> changed_items = get_from_dict_with_dict_with_text_and_value(message);
                    state->add_dbus_to_mqtt_mapping(sender, changed_items, true);

                    return DBusHandlerResult::DBUS_HANDLER_RESULT_HANDLED;
                }

                // Will contain the update for only one item.
                if (signal_name == "PropertiesChanged")
                {
                    std::unordered_map<std::string, Item> changed_items = get_from_properties_changed(message);
                    state->add_dbus_to_mqtt_mapping(sender, changed_items, true);

                    return DBusHandlerResult::DBUS_HANDLER_RESULT_HANDLED;
                }
            }

            flashmq_logf(LOG_INFO, "Unhandled signal: '%s' by '%s'", signal_name.c_str(), sender.c_str());
        }
    }
    catch (std::exception &ex)
    {
        flashmq_logf(LOG_ERR, "On signal '%s' in dbus_handle_message: %s", signal_name.c_str(), ex.what());
        return DBusHandlerResult::DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBusHandlerResult::DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}














