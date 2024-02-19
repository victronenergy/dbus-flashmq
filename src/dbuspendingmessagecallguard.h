#ifndef DBUSPENDINGMESSAGECALLGUARD_H
#define DBUSPENDINGMESSAGECALLGUARD_H

#include <dbus-1.0/dbus/dbus.h>

/**
 * @brief The DBusPendingMessageCallGuard class releases the reference you get after dbus_connection_send_with_reply(), for instance.
 *
 * The documentation of dbus_connection_send_with_reply() is not clear about whether you own a reference, but apparently you do.
 */
struct DBusPendingMessageCallGuard
{
    DBusPendingCall *d = nullptr;

    DBusPendingMessageCallGuard();
    DBusPendingMessageCallGuard(const DBusPendingMessageCallGuard &other) = delete;
    DBusPendingMessageCallGuard(DBusPendingMessageCallGuard &&other) = delete;
    ~DBusPendingMessageCallGuard();
};




#endif // DBUSPENDINGMESSAGECALLGUARD_H
