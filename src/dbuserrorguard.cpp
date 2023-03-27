#include "dbuserrorguard.h"
#include <string>
#include <stdexcept>

DBusErrorGuard::DBusErrorGuard()
{
    dbus_error_init(&err);
}

DBusErrorGuard::~DBusErrorGuard()
{
    if (dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
    }
}

DBusError *DBusErrorGuard::get()
{
    return &this->err;
}

void DBusErrorGuard::throw_error()
{
    if (dbus_error_is_set(&err))
    {
        std::string s(err.message);
        throw std::runtime_error(s);
    }
}
