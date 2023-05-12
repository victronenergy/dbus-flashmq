#ifndef STATE_H
#define STATE_H

#include <dbus-1.0/dbus/dbus.h>
#include <stdexcept>
#include <unordered_map>
#include <memory>
#include <sys/eventfd.h>
#include <functional>
#include <atomic>
#include "types.h"
#include <thread>

/**
 * @brief The Watch class is not an owner of the watches. DBus itself is.
 *
 * From a dbus dev:
 *
 * The short version is that libdbus does not guarantee that it will only
 * have one watch per fd, so lower-level code needs to be prepared to:
 * watch the fd for the union of all the flags of multiple watches; when
 * it is reported as readable, trigger the callbacks of all read-only or
 * rw watches; and when it is reported as writable, trigger the callbacks
 * of all write-only or rw watches.
 */
class Watch
{

    std::vector<DBusWatch*> watches;
public:

    int fd = -1;

    ~Watch();

    void add_watch(DBusWatch *watch);
    void remove_watch(DBusWatch *watch);
    const std::vector<DBusWatch*> &get_watches() const;

    int get_combined_epoll_flags();
    bool empty() const;

};

struct State
{
    std::thread vrm_registrator_thread;
    pid_t registrator_pid = -1;

    static std::atomic_int instance_counter;
    std::string unique_vrm_id;

    /*
     * TODO: Maybe implement the selective keep-alive mechanism like dbus-mqtt had.
     *
     * We're putting that on hold for now though. Apparently it back-fired, in terms of load.
     */
    bool alive = false;

    uint32_t keep_alive_reset_task_id = 0;

    int dispatch_event_fd = -1;
    DBusConnection *con = nullptr;
    std::unordered_map<dbus_uint32_t, std::function<void(DBusMessage *msg)>> async_handlers;
    std::unordered_map<int, std::shared_ptr<Watch>> watches;
    std::unordered_map<std::string, std::string> service_id_to_names; // like 1:31 to com.victronenergy.settings
    std::unordered_map<ShortServiceName, std::string> service_type_and_instance_to_full_service; // like 'solarcharger/258' to 'com.victronenergy.solarcharger.ttyO2'
    std::unordered_map<std::string, uint32_t> service_names_to_instance; // like 'com.victronenergy.solarcharger.ttyO2' to 258
    std::unordered_map<std::string, std::unordered_map<std::string, Item>> dbus_service_items; // keyed by service, then by dbus path, without instance.

    State();
    ~State();
    void add_dbus_to_mqtt_mapping(const std::string &serivce, std::unordered_map<std::string, Item> &items, bool instance_must_be_known);
    void add_dbus_to_mqtt_mapping(const std::string &service, uint32_t instance, Item &item);
    void handle_properties_changed(DBusMessage *msg, const std::string &service);
    const Item &find_item_by_mqtt_path(const std::string &topic) const;
    Item &find_matching_active_item(const Item &item);
    Item &find_by_service_and_dbus_path(const std::string &service, const std::string &dbus_path);
    void get_unique_id();
    void open();
    void scan_all_dbus_services();
    void scan_dbus_service(const std::string &service);
    void remove_dbus_service(const std::string &service);
    void setDispatchable();
    dbus_uint32_t call_method(const std::string &service, const std::string &path, const std::string &interface, const std::string &method,
                              const std::vector<VeVariant> &args = std::vector<VeVariant>());
    void write_to_dbus(const std::string &topic, const std::string &payload);
    uint32_t store_and_get_instance_from_service(const std::string &service, const std::unordered_map<std::string, Item> &items, bool instance_must_be_known);
    void handle_keepalive(const std::string &payload);
    void unset_keepalive();
    void publish_all();
    void set_new_id_to_owner(const std::string &owner, const std::string &name);
    std::string get_named_owner(std::string sender) const;
    void remove_id_to_owner(const std::string &owner);
    void handle_read(const std::string &topic);
};

#endif // STATE_H
