#ifndef DBUSMESSAGEGUARD_H
#define DBUSMESSAGEGUARD_H

#include <dbus-1.0/dbus/dbus.h>

struct DBusMessageGuard
{
    DBusMessage *d = nullptr;

    DBusMessageGuard(DBusMessage *msg);
    DBusMessageGuard(const DBusMessageGuard &other) = delete;
    DBusMessageGuard(DBusMessageGuard &&other) = delete;
    ~DBusMessageGuard();
};

#endif // DBUSMESSAGEGUARD_H
