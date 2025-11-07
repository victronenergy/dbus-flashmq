#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <dbus-1.0/dbus/dbus.h>
#include <stdexcept>
#include "vevariant.h"
#include "shortservicename.h"
#include "cachedstring.h"
#include "boomstring.h"

#define BRIDGE_DBUS "GXdbus"
#define BRIDGE_RPC "GXrpc"
#define BRIDGE_DEACTIVATED_STRING "deactivated"

namespace dbus_flashmq
{

enum class VrmPortalMode
{
    Unknown,
    Off,
    ReadOnly,
    Full
};

struct ValueMinMax
{
    VeVariant value;
    VeVariant min;
    VeVariant max;

    ValueMinMax() = default;
    ValueMinMax (const ValueMinMax&) = default;
    ValueMinMax &operator=(const ValueMinMax &other);
};

class Item
{
    ValueMinMax value;
    BoomString path; // without instance or service

    BoomString vrm_id;
    BoomString service_name;
    ShortServiceName short_service_name;
    BoomString mqtt_publish_topic;

    CachedString cache_json;

    static std::string join_paths_with_slash(const std::string &a, const std::string &b);
    static std::string prefix_path_with_slash(const std::string &s);
    Item(const std::string &path, const ValueMinMax &&value);
public:
    Item();

    static Item from_get_items(DBusMessageIter *iter);
    static Item from_get_value(DBusMessageIter *iter, const std::string &path_prefix);
    static Item from_properties_changed(DBusMessage *msg);

    std::string as_json();
    void set_partial_mapping_details(const std::string &service);
    void set_mapping_details(const std::string &vrm_id, const std::string &service, ServiceIdentifier instance);
    void publish(bool null_payload=false);
    const ValueMinMax &get_value() const;
    void set_value(const ValueMinMax &val);
    const std::string &get_path() const;
    const std::string &get_service_name() const;
    bool should_be_retained() const;
    bool is_ap_password() const;
    bool is_pincode() const;
    bool is_vrm_portal_mode() const;
};

}

#endif // TYPES_H
