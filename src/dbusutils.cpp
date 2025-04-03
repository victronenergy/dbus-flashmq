#include "dbusutils.h"
#include "vendor/flashmq_plugin.h"
#include "exceptions.h"
#include "dbusmessageitersignature.h"

using namespace dbus_flashmq;

// TODO: we need some kind of generalization / template for this, but I'm waiting with that until I know all the variants of result
// sets that I'm getting.
std::vector<std::string> dbus_flashmq::get_array_from_reply(DBusMessage *msg)
{
    int msg_type = dbus_message_get_type(msg);

    if (msg_type != DBUS_MESSAGE_TYPE_METHOD_RETURN && msg_type != DBUS_MESSAGE_TYPE_SIGNAL)
        throw std::runtime_error("Message is not a method return or signal.");

    std::vector<std::string> result;

    int result_n = 0;
    DBusMessageIter iter;
    int current_type = 0;
    dbus_message_iter_init(msg, &iter);

    while ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID)
    {
        if (current_type != DBUS_TYPE_ARRAY)
            throw std::runtime_error("Result is not an array");

        if (result_n++ > 1)
            throw std::runtime_error("there is more than one value in the result. We expected one array.");

        int n = dbus_message_iter_get_element_count(&iter);
        result.reserve(n);

        DBusMessageIter sub_iter;
        int current_sub_type = 0;
        dbus_message_iter_recurse(&iter, &sub_iter);
        while((current_sub_type = dbus_message_iter_get_arg_type(&sub_iter)) != DBUS_TYPE_INVALID)
        {
            if constexpr (std::is_same_v<std::string, std::string>) // TODO: construct to be used if we template this.
            {
                if (current_sub_type != DBUS_TYPE_STRING)
                    throw std::runtime_error("Result contains type we didn't expect.");
            }

            DBusBasicValue value;
            dbus_message_iter_get_basic(&sub_iter, &value);

            std::string s(value.str);

            result.push_back(std::move(s));

            dbus_message_iter_next(&sub_iter);
        }

        dbus_message_iter_next(&iter);
    }

    return result;
}

/**
 * @brief Parse the result from a com.victronenergy.BusItem.GetItems() call and ItemsChanged signal.
 * @param msg
 * @return
 *
 * The signature should be a{sa{sv}}.
 *
 * Example return value as given by dbus-send:
 *
 * method return time=1681786815.914743 sender=:1.98 -> destination=:1.513923 serial=3113638 reply_serial=2
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
 *    dict entry(
 *       string "/Mgmt/ProcessVersion"
 *       array [
 *          dict entry(
 *             string "Value"
 *             variant                   string "2.94"
 *          )
 *          dict entry(
 *             string "Text"
 *             variant                   string "2.94"
 *          )
 *       ]
 *    )
 * ]
 */
std::unordered_map<std::string, Item> dbus_flashmq::get_from_dict_with_dict_with_text_and_value(DBusMessage *msg)
{
    int msg_type = dbus_message_get_type(msg);

    if (msg_type != DBUS_MESSAGE_TYPE_METHOD_RETURN && msg_type != DBUS_MESSAGE_TYPE_SIGNAL)
        throw std::runtime_error("Message is not a method return or signal.");

    std::unordered_map<std::string, Item> result;

    int result_n = 0;
    DBusMessageIter iter;
    int current_type = 0;
    dbus_message_iter_init(msg, &iter);

    DBusMessageIterSignature signature(&iter);

    if (signature.signature != "a{sa{sv}}")
        throw std::runtime_error("Return from GetItems() is not the correct signature");

    while ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID)
    {
        if (current_type != DBUS_TYPE_ARRAY)
            throw std::runtime_error("Result from GetItems is not an array");

        if (result_n++ > 1)
        {
            flashmq_logf(LOG_ERR, "There is more than one value in the result of GetItems(). We expected one array. Ignoring it.");
            break;
        }

        DBusMessageIter sub_iter;
        dbus_message_iter_recurse(&iter, &sub_iter);
        while(dbus_message_iter_get_arg_type(&sub_iter) != DBUS_TYPE_INVALID)
        {
            try
            {
                Item item = Item::from_get_items(&sub_iter);
                result[item.get_path()] = item;
            }
            catch (std::exception &er)
            {
                flashmq_logf(LOG_ERR, "Skipping item creation because: %s", er.what());
            }

            dbus_message_iter_next(&sub_iter);
        }

        dbus_message_iter_next(&iter);
    }

    return result;
}

/**
 * @brief Doing GetValue on / gives a variant with array of the items. GetItems is preferred though. For one, GetValue doesn't return 'text'.
 * @param msg
 * @return
 *
 * Example return value:
 *
 * dbus-send --system --print-reply --dest=com.victronenergy.solarcharger.ttyO2 / com.victronenergy.BusItem.GetValue
 *
 * Output:
 *
 * variant       array [
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
 */
std::unordered_map<std::string, Item> dbus_flashmq::get_from_get_value_on_root(DBusMessage *msg, const std::string &path_prefix)
{
    int msg_type = dbus_message_get_type(msg);

    if (msg_type != DBUS_MESSAGE_TYPE_METHOD_RETURN && msg_type != DBUS_MESSAGE_TYPE_SIGNAL)
        throw std::runtime_error("Message is not a method return or signal.");

    std::unordered_map<std::string, Item> result;

    int result_n = 0;
    DBusMessageIter iter;
    int current_type = 0;
    dbus_message_iter_init(msg, &iter);

    DBusMessageIterSignature signature(&iter);

    if (signature.signature != "v")
        throw std::runtime_error("Return from GetValue() is not the correct signature");

    while ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID)
    {
        if (current_type != DBUS_TYPE_VARIANT)
            throw std::runtime_error("Result from GetValue() is not a variant");

        if (result_n++ > 1)
        {
            flashmq_logf(LOG_ERR, "There is more than one value in the result of GetValue(). We expected one variant. Ignoring it.");
            break;
        }

        DBusMessageIter outer_variant_iter;
        dbus_message_iter_recurse(&iter, &outer_variant_iter);

        if (dbus_message_iter_get_arg_type(&outer_variant_iter) != DBUS_TYPE_ARRAY)
            throw std::runtime_error("Expected string as dict key.");

        DBusMessageIter variant_array_iter;
        dbus_message_iter_recurse(&outer_variant_iter, &variant_array_iter);

        while(dbus_message_iter_get_arg_type(&variant_array_iter) != DBUS_TYPE_INVALID)
        {
            try
            {
                Item item = Item::from_get_value(&variant_array_iter, path_prefix);
                result[item.get_path()] = item;
            }
            catch (std::exception &er)
            {
                flashmq_logf(LOG_ERR, "Skipping item creation because: %s", er.what());
            }

            dbus_message_iter_next(&variant_array_iter);
        }

        dbus_message_iter_next(&iter);
    }

    return result;
}

/**
 * @brief get_from_properties_changed processes a PropertiesChanged signal into a list of Item, even though it's only one. It allows for passing to
 * add_dbus_to_mqtt_mapping(...).
 * @param msg
 * @return a list of Item, even though it's only one.
 *
 * Example signal PropertiesChanged:
 *
 * signal time=1691485092.671780 sender=:1.47 -> destination=(null destination) serial=8149 path=/Settings/Pump0/TankService; interface=com.victronenergy.BusItem; member=PropertiesChanged
 * array [
 *    dict entry(
 *       string "Value"
 *       variant             string "notanksensor"
 *    )
 *    dict entry(
 *       string "Text"
 *       variant             string "notanksensor"
 *    )
 *    dict entry(
 *       string "Min"
 *       variant             int32 0
 *    )
 *    dict entry(
 *       string "Max"
 *       variant             int32 0
 *    )
 *    dict entry(
 *       string "Default"
 *       variant             string "notanksensor"
 *    )
 * ]
 */
std::unordered_map<std::string, Item> dbus_flashmq::get_from_properties_changed(DBusMessage *msg)
{
    std::unordered_map<std::string, Item> result;

    Item item = Item::from_properties_changed(msg);
    result[item.get_path()] = item;

    return result;
}

// TODO: for the basic types, I think I can template this one.
std::string dbus_flashmq::get_string_from_reply(DBusMessage *msg)
{
    const int msg_type = dbus_message_get_type(msg);

    if (msg_type != DBUS_MESSAGE_TYPE_METHOD_RETURN && msg_type != DBUS_MESSAGE_TYPE_SIGNAL)
        throw std::runtime_error("Message is not a method return or signal.");

    DBusMessageIter iter;
    dbus_message_iter_init(msg, &iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
    {
        throw ValueError("Trying to read a string, but it's not.");
    }

    DBusBasicValue val;
    dbus_message_iter_get_basic(&iter, &val);
    std::string result(val.str);
    return result;
}
