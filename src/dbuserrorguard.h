#ifndef DBUSERRORGUARD_H
#define DBUSERRORGUARD_H

#include <dbus-1.0/dbus/dbus.h>

/**
 * @brief The DBusErrorGuard class makes dealing with DBusErrors easier and prevent leaks.
 *
 * https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html
 */
class DBusErrorGuard
{
    DBusError err;
public:
    DBusErrorGuard();
    ~DBusErrorGuard();
    DBusError *get();
    void throw_error();
};

#endif // DBUSERRORGUARD_H
