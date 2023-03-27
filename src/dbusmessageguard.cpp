#include "dbusmessageguard.h"

DBusMessageGuard::DBusMessageGuard(DBusMessage *msg) :
    d(msg)
{

}

DBusMessageGuard::~DBusMessageGuard()
{
    if (d)
    {
        dbus_message_unref(d);
        d = nullptr;
    }
}
