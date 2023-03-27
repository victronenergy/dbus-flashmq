#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <dbus-1.0/dbus/dbus.h>
#include <stdexcept>
#include "vevariant.h"
#include "shortservicename.h"
#include "cachedstring.h"
#include "boomstring.h"

class Item
{
    VeVariant value;
    BoomString path; // without instance or service

    BoomString vrm_id;
    BoomString service_name;
    ShortServiceName short_service_name;
    BoomString mqtt_publish_topic;

    CachedString cache_json;

    static std::string prefix_path_with_slash(const std::string &s);
    Item(const std::string &path, const VeVariant &&value);
public:
    Item();

    static Item from_get_items(DBusMessageIter *iter);
    static Item from_get_value(DBusMessageIter *iter);

    std::string as_json();
    void set_mapping_details(const std::string &vrm_id, const std::string &service, uint32_t instance);
    void publish(bool null_payload=false);
    const VeVariant &get_value() const;
    void set_value(const VeVariant &val);
    const std::string &get_path() const;
    const std::string &get_service_name() const;
};

#endif // TYPES_H
