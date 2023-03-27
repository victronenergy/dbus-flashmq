#include "types.h"

#include <sstream>

#include "exceptions.h"
#include "vendor/flashmq_plugin.h"
#include "vendor/json.hpp"


/**
 * @brief Item::set_path stores the path with a leading slash.
 * @param path
 *
 * There are discrepancies between older and newer dbus methods wether the leading slash is used.
 */
std::string Item::prefix_path_with_slash(const std::string &path)
{
    std::string new_path;
    if (path.find('/') != 0)
        new_path.push_back('/');
    new_path.append(path);
    return new_path;
}

Item::Item(const std::string &path, const VeVariant &&value) :
    value(std::move(value)),
    path(prefix_path_with_slash(path))
{

}

Item::Item()
{

}

/**
 * @brief Item::from_get_items constructs an item as returned by the GetItems method on Victron dbus services.
 * @param iter pointing at the first dict entry in below example.
 * @return
 *
 * GetItems() returns arrays like these:
 *
 * array [
 *    dict entry(
 *       string "/Mgmt/ProcessName"
 *       array [
 *          dict entry(
 *             string "Value"
 *             variant                   string "/opt/victronenergy/dbus-systemcalc-py/dbus_systemcalc.py"
 *          )
 *          dict entry(
 *             string "Text"
 *             variant                   string "/opt/victronenergy/dbus-systemcalc-py/dbus_systemcalc.py"
 *          )
 *       ]
 *    )
 *    etc
 *  ]
 *
 * Null entry looks like (empty array):
 *
 * dict entry(
 *   string "/ProductId"
 *   array [
 *      dict entry(
 *         string "Value"
 *         variant                   array [
 *            ]
 *      )
 *      dict entry(
 *         string "Text"
 *         variant                   string "---"
 *      )
 *   ]
 * )
 *
 * This one parses one of those dict entries, containing a path as key and dict with text and value, to an Item.
 */
Item Item::from_get_items(DBusMessageIter *iter)
{
    int type = dbus_message_iter_get_arg_type(iter);

    if (type != DBUS_TYPE_DICT_ENTRY)
        throw ValueError("Expected array from dbus when constructing item.");

    DBusMessageIter dict_iter;
    dbus_message_iter_recurse(iter, &dict_iter);

    if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING)
        throw ValueError("Dict key should be string.");

    DBusBasicValue key;
    dbus_message_iter_get_basic(&dict_iter, &key);
    const std::string path(key.str);

    dbus_message_iter_next(&dict_iter);

    if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_ARRAY)
        throw ValueError("Dict value should be array.");

    DBusMessageIter array_iter;
    dbus_message_iter_recurse(&dict_iter, &array_iter);

    VeVariant value;

    int value_type = 0;
    while ((value_type = dbus_message_iter_get_arg_type(&array_iter)) != DBUS_TYPE_INVALID)
    {
        if (value_type != DBUS_TYPE_DICT_ENTRY)
        {
            throw ValueError("Item can only be created from dict entries.");
        }

        DBusMessageIter one_item_iter;
        dbus_message_iter_recurse(&array_iter, &one_item_iter);

        DBusBasicValue key_v;
        dbus_message_iter_get_basic(&one_item_iter, &key_v);

        std::string key(key_v.str);

        dbus_message_iter_next(&one_item_iter);

        if (dbus_message_iter_get_arg_type(&one_item_iter) != DBUS_TYPE_VARIANT)
            throw ValueError("Value/Text elements in dict must be variant.");

        VeVariant val(&one_item_iter);

        if (key == "Value")
        {
            value = std::move(val);
        }

        dbus_message_iter_next(&array_iter);
    }

    Item item(path, std::move(value));
    return item;
}

/**
 * @brief Item::from_get_value
 * @param iter
 * @return Item
 *
 * GetValue() returns arrays like these:
 *
 *  variant       array [
 *       dict entry(
 *          string "History/Daily/18/TimeInFloat"
 *          variant                double 0
 *       )
 *       dict entry(
 *          string "History/Daily/29/TimeInBulk"
 *          variant                double 0
 *       )
 *       dict entry(
 *          string "Yield/Power"
 *          variant                double 251
 *       )
 *       etc
 *   ]
 *
 * This one parses one of those inner dict entries to an Item.
 */
Item Item::from_get_value(DBusMessageIter *iter)
{
    const int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_DICT_ENTRY)
        throw ValueError("Expected array from dbus when constructing item.");

    DBusMessageIter dict_iter;
    dbus_message_iter_recurse(iter, &dict_iter);

    if (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_STRING)
        throw ValueError("Dict key should be string.");

    DBusBasicValue key;
    dbus_message_iter_get_basic(&dict_iter, &key);
    const std::string path(key.str);

    dbus_message_iter_next(&dict_iter);

    const int value_type = dbus_message_iter_get_arg_type(&dict_iter);
    if (value_type != DBUS_TYPE_VARIANT)
        throw ValueError("Expected array from dbus when constructing item.");

    const VeVariant val = VeVariant(&dict_iter);

    Item item(path, std::move(val));
    return item;
}

std::string Item::as_json()
{
    if (!cache_json.v.empty())
        return cache_json.v;

    nlohmann::json j { {"value", value.as_json_value()} };
    cache_json.v = j.dump();
    return cache_json.v;
}

void Item::set_mapping_details(const std::string &vrm_id, const std::string &service, uint32_t instance)
{
    ShortServiceName short_service_name(service, instance);

    this->vrm_id = vrm_id;
    this->short_service_name = short_service_name;
    this->service_name = service;

    std::ostringstream topic_stream;
    topic_stream << "N" << "/" << this->vrm_id.get() << "/" << short_service_name << path.get();
    this->mqtt_publish_topic = topic_stream.str();
}

void Item::publish(bool null_payload)
{
    if (this->mqtt_publish_topic.get().empty())
        return;

    // Blocked entries
    if ((short_service_name.service_type == "vebus" && path.get() == "/Interfaces/Mk2/Tunnel") || (short_service_name.service_type == "paygo" && path.get() == "/LVD/Threshold"))
        return;

    std::string payload;

    if (!null_payload)
        payload = as_json();

    bool retain = false;

    if ((short_service_name.service_type == "system" && path.get() == "/Serial") || path.get() == "/keepalive")
        retain = true;

    // Now that we use retain very selectively, never unpublish it.
    if (!(retain && payload.empty()))
    {
        // Note that FlashMQ merely appends the packet to the TCP client's output buffer as bytes, and once you return control
        // to the main loop, this buffer is flushed. This is a prerequisite to being fast.
        flashmq_publish_message(this->mqtt_publish_topic.get(), 0, retain, payload);
    }
}

/**
 * @brief Item::get_value is const and returns on purpose, because this value should only be set through Item::set_value().
 * @return
 */
const VeVariant &Item::get_value() const
{
    return this->value;
}

void Item::set_value(const VeVariant &val)
{
    this->cache_json.v.clear();
    this->value = val;
}

const std::string &Item::get_path() const
{
    return this->path.get();
}

const std::string &Item::get_service_name() const
{
    return this->service_name.get();
}










