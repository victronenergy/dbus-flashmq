#ifndef DBUSUTILS_H
#define DBUSUTILS_H

#include <vector>
#include <string>
#include <unordered_map>
#include <dbus-1.0/dbus/dbus.h>
#include <optional>

#include "types.h"


namespace dbus_flashmq
{

std::vector<std::string> get_array_from_reply(DBusMessage *msg);
std::unordered_map<std::string, Item> get_from_dict_with_dict_with_text_and_value(DBusMessage *msg);
std::unordered_map<std::string, Item> get_from_get_value_on_root(DBusMessage *msg, const std::string &path_prefix);
std::unordered_map<std::string, Item> get_from_properties_changed(DBusMessage *msg);
std::string get_string_from_reply(DBusMessage *msg);
std::optional<dbus_int32_t> get_return_code_from_reply(DBusMessage *msg);

}

#endif // DBUSUTILS_H
