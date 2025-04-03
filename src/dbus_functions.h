#ifndef DBUS_FUNCTIONS_H
#define DBUS_FUNCTIONS_H

#include <dbus-1.0/dbus/dbus.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <any>

namespace dbus_flashmq
{

void dbus_dispatch_status_function(DBusConnection *connection, DBusDispatchStatus new_status, void *data);

dbus_bool_t dbus_add_watch_function(DBusWatch *watch, void *data);
void dbus_remove_watch_function(DBusWatch *watch, void *data);
void dbus_toggle_watch_function(DBusWatch *watch, void *data);
DBusHandlerResult dbus_handle_message(DBusConnection *connection, DBusMessage *message, void *user_data);

dbus_bool_t dbus_add_timeout_function(DBusTimeout *timeout, void *data);
void dbus_remove_timeout_function(DBusTimeout *timeout, void *data);
void dbus_toggle_timeout_function(DBusTimeout *timeout, void *data);
void dbus_timeout_do_handle(DBusTimeout *timeout);

void dbus_pending_call_notify(DBusPendingCall *pending, void *data) noexcept;

}

#endif // DBUS_FUNCTIONS_H
