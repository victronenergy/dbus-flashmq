#include "state.h"

#include <cstring>
#include <unistd.h>
#include <sys/epoll.h>
#include <functional>
#include <fstream>
#include <sstream>
#include <cassert>
#include <sys/types.h>
#include <signal.h>

#include "dbus_functions.h"
#include "dbusutils.h"
#include "vendor/flashmq_plugin.h"
#include "dbusmessageguard.h"
#include "utils.h"
#include "types.h"
#include "dbuserrorguard.h"
#include "exceptions.h"
#include "dbusmessageitersignature.h"
#include "dbusmessageiteropencontainerguard.h"
#include "dbuspendingmessagecallguard.h"
#include "exceptions.h"

using namespace std;

std::atomic_int State::instance_counter = 0;

Watch::~Watch()
{
    fd = -1;
}

State::State()
{
    /*
     * FlashMQ's threading model is spreading clients over threads. We only have one, so threads only make things difficult, mainly with
     * dbus. Plus, everything is async, so we don't need threads.
     */
    if (++instance_counter > 1)
    {
        throw std::runtime_error("Set thread_count to 1 in FlashMQ's config.");
    }

    dispatch_event_fd = eventfd(0, EFD_NONBLOCK);
    flashmq_poll_add_fd(dispatch_event_fd, EPOLLIN, std::weak_ptr<void>());
}

State::~State()
{

}

void State::get_unique_id()
{
    fstream file;

    file.open("/data/venus/unique-id", ios::in);
    getline(file, this->unique_vrm_id);
    trim(this->unique_vrm_id);

    if (this->unique_vrm_id.empty())
        throw std::runtime_error("Failed to obtain unique VRM identifier.");
}

/**
 * @brief State::add_dbus_to_mqtt_mapping
 * @param service
 * @param items
 * @param instance_must_be_known When items is a set of items from a signal, the /DeviceInstance is not among them. But when the items
 *        are from a call to GetValue on /, it is. Set this bool to make sure you don't make the wrong assumptions.
 */
void State::add_dbus_to_mqtt_mapping(const std::string &service, std::unordered_map<std::string, Item> &items, bool instance_must_be_known, bool force_publish)
{
    if (instance_must_be_known)
    {
        auto pos = dbus_service_items.find(service);
        if (pos == dbus_service_items.end())
        {
            /*
             * We may already get ItemsChanged when a service appears before we fully know the device instance and short name (like :1.33).
             * In those cases, we have to queue them up to process later.
             */

            for (auto &p : items)
            {
                Item &i = p.second;

                i.set_partial_mapping_details(service);

                flashmq_logf(LOG_DEBUG, "Queueing changed values for '%s' '%s' with value '%s' until we fully know the service.",
                             service.c_str(), i.get_path().c_str(), i.get_value().value.as_text().c_str());

                delayed_changed_values.emplace_back(i);
            }

            return;
        }
    }

    ServiceIdentifier device_instance = store_and_get_instance_from_service(service, items, instance_must_be_known);

    ShortServiceName s(service, device_instance);
    this->service_type_and_instance_to_full_service[s] = service;

    for (auto &p : items)
    {
        Item &item = p.second;
        add_dbus_to_mqtt_mapping(service, device_instance, item, force_publish);
    }

    attempt_to_process_delayed_changes();
}

/**
 * @brief Like '_add_item()' in the Python version.
 * @param service Like 'com.victronenergy.system'.
 * @param instance The instance number.
 * @param item.
 */
void State::add_dbus_to_mqtt_mapping(const std::string &service, ServiceIdentifier instance, Item &item, bool force_publish)
{
    item.set_mapping_details(unique_vrm_id, service, instance);
    Item &fully_mapped_item = dbus_service_items[service][item.get_path()];
    fully_mapped_item = item;

    if (this->alive || fully_mapped_item.should_be_retained() || force_publish)
        fully_mapped_item.publish();
}

/**
 * @brief State::find_item_by_mqtt_path get item by value based on topic.
 * @param topic
 */
const Item &State::find_item_by_mqtt_path(const std::string &topic) const
{
    // Example topic: N/48e7da87942f/solarcharger/258/Link

    std::vector<std::string> parts = splitToVector(topic, '/', 4);

    const std::string &vrm_id = parts.at(1);

    if (vrm_id != this->unique_vrm_id)
        throw std::runtime_error("Second subpath should match local VRM id. It doesn't.");

    const std::string &short_service = parts.at(2);
    const std::string &instance_str = parts.at(3);
    const std::string &dbus_like_path = "/" + parts.at(4);
    ServiceIdentifier instance(instance_str);
    ShortServiceName short_service_name(short_service, instance);

    auto pos_service = this->service_type_and_instance_to_full_service.find(short_service_name);

    if (pos_service == this->service_type_and_instance_to_full_service.end())
    {
        throw std::runtime_error("Can't find dbus service for " + topic);
    }

    const std::string &full_service = pos_service->second;

    auto pos = dbus_service_items.find(full_service);
    if (pos == dbus_service_items.end())
    {
        throw std::runtime_error("Can't find service for " + full_service);
    }

    const std::unordered_map<std::string, Item> &items = pos->second;

    auto pos_item = items.find(dbus_like_path);
    if (pos_item == items.end())
    {
        throw ItemNotFound("Can't find item for " + dbus_like_path, full_service, dbus_like_path);
    }

    return pos_item->second;
}

Item &State::find_matching_active_item(const Item &item)
{
    return find_by_service_and_dbus_path(item.get_service_name(), item.get_path());
}

Item &State::find_by_service_and_dbus_path(const std::string &service, const std::string &dbus_path)
{
    auto pos = dbus_service_items.find(service);

    if (pos == dbus_service_items.end())
        throw std::runtime_error("Can't find service: " + service);

    std::unordered_map<std::string, Item> &items = pos->second;

    auto pos_item = items.find(dbus_path);

    if (pos_item == items.end())
        throw std::runtime_error("Can't find item with path: " + dbus_path);

    return pos_item->second;
}

void State::attempt_to_process_delayed_changes()
{
    if (this->delayed_changed_values.empty())
        return;

    std::vector<QueuedChangedItem> changed_values = std::move(this->delayed_changed_values);
    this->delayed_changed_values.clear();

    for (QueuedChangedItem &i : changed_values)
    {
        if (i.age() > std::chrono::seconds(30))
        {
            flashmq_logf(LOG_DEBUG, "Giving up on orphaned PropertiesChanged for '%s' '%s' with value '%s'.",
                         i.item.get_service_name().c_str(), i.item.get_path().c_str(), i.item.get_value().value.as_text().c_str());
            continue;
        }

        auto pos = dbus_service_items.find(i.item.get_service_name());
        if (pos == dbus_service_items.end())
        {
            delayed_changed_values.push_back(std::move(i));
            continue;
        }

        flashmq_logf(LOG_DEBUG, "Sending queued changes for '%s' '%s' with value '%s'.",
                     i.item.get_service_name().c_str(), i.item.get_path().c_str(), i.item.get_value().value.as_text().c_str());

        Item &item = find_matching_active_item(i.item);
        item.set_value(i.item.get_value());

        if (this->alive)
            item.publish();
    }
}

void State::open()
{
    DBusErrorGuard err;
    con = dbus_bus_get(DBusBusType::DBUS_BUS_SYSTEM, err.get());
    err.throw_error();

    dbus_connection_set_dispatch_status_function(con, dbus_dispatch_status_function, this, nullptr);
    dbus_connection_set_watch_functions(con, dbus_add_watch_function, dbus_remove_watch_function, dbus_toggle_watch_function, this, nullptr);
    dbus_connection_set_timeout_functions(con, dbus_add_timeout_function, dbus_remove_timeout_function, dbus_toggle_timeout_function, this, nullptr);
    dbus_connection_add_filter(con, dbus_handle_message, this, nullptr);

    dbus_bus_add_match(con, "type='signal',interface='com.victronenergy.BusItem'", nullptr);
    dbus_bus_add_match(con, "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged'", nullptr);

    this->setDispatchable();
}

dbus_uint32_t State::call_method(const std::string &service, const std::string &path, const std::string &interface, const std::string &method,
                                 const std::vector<VeVariant> &args, bool wrap_arguments_in_variant)
{
    if (!dbus_validate_path(path.c_str(), nullptr))
    {
        throw std::runtime_error("Path '" + path + "' is not valid for method call.");
    }

    DBusMessageGuard msg = dbus_message_new_method_call(service.c_str(), path.c_str(), interface.c_str(), method.c_str());

    if (!msg.d)
    {
        throw std::runtime_error("No DBusMessage received from dbus_message_new_method_call. Out of memory?");
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.d, &iter);
    for (const VeVariant &arg : args)
    {
        if (wrap_arguments_in_variant)
        {
            DBusMessageIterOpenContainerGuard variant_iter(&iter, DBUS_TYPE_VARIANT, arg.get_dbus_type_as_string_recursive().c_str());
            arg.append_args_to_dbus_message(variant_iter.get_array_iter());
        }
        else
        {
            arg.append_args_to_dbus_message(&iter);
        }

    }

    DBusPendingMessageCallGuard pendingCall;
    dbus_bool_t send_reply_result = dbus_connection_send_with_reply(con, msg.d, &pendingCall.d, -1);

    if (!pendingCall.d || !send_reply_result)
        throw std::runtime_error("Tried method call but failed: DBusPendingCall is null or result was false.");

    if (!dbus_pending_call_set_notify(pendingCall.d, dbus_pending_call_notify, this, nullptr))
    {
        throw std::runtime_error("dbus_pending_call_set_notify returned false.");
    }

    dbus_uint32_t serial = dbus_message_get_serial(msg.d);
    return serial;
}

void State::write_to_dbus(const std::string &topic, const std::string &payload)
{
    flashmq_logf(LOG_DEBUG, "[Write] Writing '%s' to '%s'", payload.c_str(), topic.c_str());

    const nlohmann::json j = nlohmann::json::parse(payload);

    auto jpos = j.find("value");
    if (jpos == j.end())
        throw ValueError("Can't find 'value' in json.");

    nlohmann::json::value_type json_value = *jpos;

    const Item &item = find_item_by_mqtt_path(topic);

    VeVariant new_value(json_value);

    flashmq_logf(LOG_DEBUG, "[Write] Determined dbus type of '%s' as '%s'", json_value.dump().c_str(), new_value.get_dbus_type_as_string_recursive().c_str());

    std::vector<VeVariant> args;
    args.push_back(new_value);
    dbus_uint32_t serial = call_method(item.get_service_name(), item.get_path(), "com.victronenergy.BusItem", "SetValue", args, true);

    auto set_value_handler = [](State *state, const std::string &topic, DBusMessage *msg) {
        const int msg_type = dbus_message_get_type(msg);

        if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
        {
            std::string error = dbus_message_get_error_name_safe(msg);
            flashmq_logf(LOG_ERR, "Error on 'SetValue' on %s: %s", topic.c_str(), error.c_str());
            return;
        }

        flashmq_logf(LOG_DEBUG, "SetValue on '%s' successful.", topic.c_str());
    };

    auto handler = std::bind(set_value_handler, this, topic, std::placeholders::_1);
    this->async_handlers[serial] = handler;
}

ServiceIdentifier State::store_and_get_instance_from_service(const std::string &service, const std::unordered_map<std::string, Item> &items, bool instance_must_be_known)
{
    ServiceIdentifier device_instance;
    auto pos = this->service_names_to_instance.find(service);
    if (pos == this->service_names_to_instance.end())
    {
        if (instance_must_be_known)
            throw std::runtime_error("Programming error: you're assuming we know the instance already.");

        device_instance = get_instance_from_items(items);
        this->service_names_to_instance[service] = device_instance;
    }
    else
        device_instance = pos->second;

    return device_instance;
}

/**
 * @brief Keeps the installation actively publishing changes to MQTT.
 * @param payload options, like: { "keepalive-options" : ["suppress-republish"] }
 *
 * The payload was previsouly used for selecting only certain topics. We are probably not going to support that functionality. But
 * Note that that format was an array of topics, not a dict with keys. That kind of limited supporting other things with it. That's why
 * we're using a json object now, that you can give keys.
 *
 * Suppressing the publication of all topics can be done by those clients that understand we no longer use retained messages. By defaulting
 * to publishing all on getting a keep-alive, you can be sure you receive all topics, whether you are the first, second, third, watcher on
 * an already alive installation.
 */
void State::handle_keepalive(const std::string &payload)
{
    // Cheating: I don't actually need to parse the json.
    bool suppress_publish_of_all = payload.find("suppress-republish") != std::string::npos;

    // Always do it the first time, whether or not suppression is set.
    if (!suppress_publish_of_all)
        publish_all();

    this->alive = true;
    flashmq_remove_task(this->keep_alive_reset_task_id);
    auto f = std::bind(&State::unset_keepalive, this);
    this->keep_alive_reset_task_id = flashmq_add_task(f, 60000);

    if (!heartbeat_task_id)
        heartbeat();
}

void State::unset_keepalive()
{
    this->alive = false;
    this->keep_alive_reset_task_id = 0;
}

void State::heartbeat()
{
    if (!alive)
    {
        heartbeat_task_id = 0;
        return;
    }

    std::ostringstream heartbeat_topic;
    heartbeat_topic << "N/" << unique_vrm_id << "/heartbeat";

    const int64_t unix_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    nlohmann::json j { {"value", unix_time} };
    std::string payload = j.dump();

    flashmq_publish_message(heartbeat_topic.str(), 0, false, payload);

    auto f = std::bind(&State::heartbeat, this);
    heartbeat_task_id = flashmq_add_task(f, 3000);
}

void State::publish_all()
{
    for (auto &p : dbus_service_items)
    {
        for (auto &p2 : p.second)
        {
            Item &i = p2.second;
            i.publish();
        }
    }

    std::ostringstream done_topic;
    done_topic << "N/" << unique_vrm_id << "/full_publish_completed";

    const int64_t unix_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    nlohmann::json j { {"value", unix_time} };
    std::string payload = j.dump();

    flashmq_publish_message(done_topic.str(), 0, false, payload);
}

/**
 * @brief State::set_new_id_to_owner
 * @param owner Like 1:31.
 * @param name Like com.victronenergy.vecan.can0
 */
void State::set_new_id_to_owner(const std::string &owner, const std::string &name)
{
    assert(owner.find(":") != std::string::npos);
    this->service_id_to_names[owner] = name;
}

/**
 * @brief State::get_named_owner
 * @param sender Like 1:31
 * @return
 */
std::string State::get_named_owner(std::string sender) const
{
    if (sender.find("com.victronenergy") == std::string::npos)
    {
        auto pos = service_id_to_names.find(sender);
        if (pos != service_id_to_names.end())
            sender = pos->second;
    }

    return sender;
}

/**
 * @brief State::remove_id_to_owner
 * @param owner Like 1:31
 */
void State::remove_id_to_owner(const std::string &owner)
{
    assert(owner.find(":") != std::string::npos);
    service_id_to_names.erase(owner);
}

/**
 * @brief State::handle_read
 * @param topic like 'R/48e7da87942f/system/0/Ac/Grid/L2/Power'
 *
 * Read a fresh value and make sure item is added. This is because a path may not always send
 * PropertiesChanged (eg /vebus/Hub4/L1/AcPowerSetpoint) but can nevertheless be read.
 */
void State::handle_read(const std::string &topic)
{
    try
    {
        const Item &item = find_item_by_mqtt_path(topic);
        dbus_uint32_t serial = call_method(item.get_service_name(), item.get_path(), "com.victronenergy.BusItem", "GetValue");

        auto get_value_handler = [](State *state, const Item &item, DBusMessage *msg) {
            const int msg_type = dbus_message_get_type(msg);

            if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
            {
                std::string error = dbus_message_get_error_name_safe(msg);
                flashmq_logf(LOG_ERR, "Error on 'GetValue' from %s: %s", item.get_path().c_str(), error.c_str());
                return;
            }

            DBusMessageIter iter;
            dbus_message_iter_init(msg, &iter);
            VeVariant answer(&iter);

            ValueMinMax val;
            val.value = std::move(answer);

            Item &real_item = state->find_matching_active_item(item);
            real_item.set_value(val);
            real_item.publish();
        };

        auto handler = std::bind(get_value_handler, this, item, std::placeholders::_1);
        this->async_handlers[serial] = handler;
    }
    catch (ItemNotFound &info)
    {
        get_value(info.service, info.dbus_like_path, true);
    }
}

/**
 * @brief State::initiate_broker_registration calls a dbus method to have Venus Platform call mosquitto_bridge_registrator.py. Contrary to
 * the previous dbus-mqtt, we are not root anymore, so we can't do it directly.
 * @param delay
 */
void State::initiate_broker_registration(uint32_t delay)
{
    if (!do_online_registration)
        return;

    if (register_pending_id > 0)
    {
        flashmq_logf(LOG_WARNING, "Trying to register at VRM while a dbus method request for it is still pending.");
        return;
    }

    auto register_at_vrm_handler = [](State *state, DBusMessage *msg) {
        state->register_pending_id = 0;

        const int msg_type = dbus_message_get_type(msg);

        if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
        {
            std::string error = dbus_message_get_error_name_safe(msg);
            flashmq_logf(LOG_ERR, "Error on SetValue on /Mqtt/RegisterOnVrm: %s", error.c_str());
            return;
        }

        flashmq_logf(LOG_NOTICE, "SetValue on /Mqtt/RegisterOnVrm seemingly successful.");
    };

    auto register_f = [register_at_vrm_handler](State *state) {
        flashmq_logf(LOG_NOTICE, "Initiating bridge registration.");

        dbus_uint32_t serial = state->call_method("com.victronenergy.platform", "/Mqtt/RegisterOnVrm", "com.victronenergy.platform", "SetValue", {{"1"}}, true);

        auto handler_f = std::bind(register_at_vrm_handler, state, std::placeholders::_1);
        state->async_handlers[serial] = handler_f;
    };

    auto f = std::bind(register_f, this);
    register_pending_id = flashmq_add_task(f, delay);
}

void State::scan_all_dbus_services()
{
    auto list_names_handler = [](State *state, DBusMessage *msg) {
        const std::vector<std::string> services = get_array_from_reply(msg);

        for(const std::string &service : services)
        {
            if (service.find("com.victronenergy") == std::string::npos)
                continue;

            state->scan_dbus_service(service);
        }
    };

    dbus_uint32_t serial = call_method("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    auto bla = std::bind(list_names_handler, this, std::placeholders::_1);
    async_handlers[serial] = bla;
}

void State::get_value(const std::string &service, const std::string &path, bool force_publish)
{
    auto get_value_handler = [](State *state, const std::string &service, const std::string &path_prefix, bool force_publish, DBusMessage *msg) {
        const int msg_type = dbus_message_get_type(msg);

        if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
        {
            std::string error = dbus_message_get_error_name_safe(msg);
            flashmq_logf(LOG_ERR, "Error on 'GetValue' from %s: %s", service.c_str(), error.c_str());
            return;
        }

        std::unordered_map<std::string, Item> items = get_from_get_value_on_root(msg, path_prefix);
        state->add_dbus_to_mqtt_mapping(service, items, false, force_publish);
    };

    dbus_uint32_t serial = this->call_method(service, path, "com.victronenergy.BusItem", "GetValue");
    auto handler = std::bind(get_value_handler, this, service, path, force_publish, std::placeholders::_1);
    this->async_handlers[serial] = handler;
}

void State::scan_dbus_service(const std::string &service)
{
    auto get_items_handler = [](State *state, const std::string &service, DBusMessage *msg) {
        const int msg_type = dbus_message_get_type(msg);

        if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
        {
            std::string error = dbus_message_get_error_name_safe(msg);

            if (error == "org.freedesktop.DBus.Error.UnknownMethod")
            {
                /*
                 * The current preferred way of getting values is GetItems, which uses async IO in Python. But, but not
                 * all services support that. So, if we error here with (org.freedesktop.DBus.Error.UnknownObject
                 * or) org.freedesktop.DBus.Error.UnknownMethod, we have to use the traditional GetValue on /.
                 */

                // TODO: and if this fails, introspect it? For now, we decided to not do this. QWACS is the only thing so far that seems to need it.

                state->get_value(service, "/");
                return;
            }

            flashmq_logf(LOG_ERR, "Error on 'GetItems' from %s: %s", service.c_str(), error.c_str());
            return;
        }

        std::unordered_map<std::string, Item> items = get_from_dict_with_dict_with_text_and_value(msg);
        state->add_dbus_to_mqtt_mapping(service, items, false);
    };

    auto get_name_owner_handler = [get_items_handler](State *state, const std::string &service, DBusMessage *msg) {
        const int msg_type = dbus_message_get_type(msg);
        if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
        {
            std::string error = dbus_message_get_error_name_safe(msg);
            flashmq_logf(LOG_ERR, error.c_str());
            return;
        }

        const std::string name_owner = get_string_from_reply(msg);
        state->service_id_to_names[name_owner] = service;

        dbus_uint32_t serial = state->call_method(service, "/", "com.victronenergy.BusItem", "GetItems");
        auto handler = std::bind(get_items_handler, state, service, std::placeholders::_1);
        state->async_handlers[serial] = handler;
    };

    // We have to know the :1.66 like name for com.victronenergy.system and such, because in signals, we only have :1.66 as sender.
    std::vector<VeVariant> args {service};
    dbus_uint32_t s = call_method("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner", args);
    auto handler = std::bind(get_name_owner_handler, this, service, std::placeholders::_1);
    this->async_handlers[s] = handler;
}

void State::remove_dbus_service(const std::string &service)
{
    {
        auto pos = dbus_service_items.find(service);
        if (pos != dbus_service_items.end())
        {
            std::unordered_map<std::string, Item> &items = pos->second;

            for (auto &p : items)
            {
                Item &item = p.second;
                item.publish(true);
            }
        }
    }

    dbus_service_items.erase(service);
    service_names_to_instance.erase(service);

    {
        // Looping over values because it's the best way to guarantee we find it.
        auto pos = service_type_and_instance_to_full_service.begin();
        while (pos != service_type_and_instance_to_full_service.end())
        {
            auto pos_use = pos++;
            if (pos_use->second == service)
            {
                service_type_and_instance_to_full_service.erase(pos_use);
                break;
            }
        }
    }

    {
        // This shouldn't be necessry because we did it already, but just making sure.
        // Looping over values because it's the best way to guarantee we find it.
        auto pos = service_id_to_names.begin();
        while (pos != service_id_to_names.end())
        {
            auto pos_use = pos++;
            if (pos_use->second == service)
            {
                service_id_to_names.erase(pos_use);
                break;
            }
        }
    }
}

void State::setDispatchable()
{
    uint64_t one = 1;
    if (write(dispatch_event_fd, &one, sizeof(uint64_t)) < 0)
    {
        const char *err = strerror(errno);
        flashmq_logf(LOG_ERR, err);
    }
}

void Watch::add_watch(DBusWatch *watch)
{
    if (std::find(watches.begin(), watches.end(), watch) != watches.end())
        return;

    watches.push_back(watch);
}

void Watch::remove_watch(DBusWatch *watch)
{
    auto pos = std::find(watches.begin(), watches.end(), watch);

    if (pos != watches.end())
        watches.erase(pos);
}

const std::vector<DBusWatch*> &Watch::get_watches() const
{
    return watches;
}

int Watch::get_combined_epoll_flags()
{
    int result = 0;

    for (DBusWatch *watch : watches)
    {
        if (!dbus_watch_get_enabled(watch))
            continue;

        int dbus_flags = dbus_watch_get_flags(watch);
        int epoll_flags = dbus_watch_flags_to_epoll(dbus_flags);
        result |= epoll_flags;
    }

    return result;
}

bool Watch::empty() const
{
    return watches.empty();
}

QueuedChangedItem::QueuedChangedItem(const Item &item) :
    item(item)
{

}

std::chrono::seconds QueuedChangedItem::age() const
{
    auto now = std::chrono::steady_clock::now();
    auto c = now - created_at;
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(c);
    return duration;
}
