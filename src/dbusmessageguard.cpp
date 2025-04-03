#include "dbusmessageguard.h"

using namespace dbus_flashmq;

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
