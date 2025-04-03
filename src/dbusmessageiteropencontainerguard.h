#ifndef DBUSMESSAGEITEROPENCONTAINERGUARD_H
#define DBUSMESSAGEITEROPENCONTAINERGUARD_H

#include <dbus-1.0/dbus/dbus.h>

namespace dbus_flashmq
{

class DBusMessageIterOpenContainerGuard
{
    DBusMessageIter *iter = nullptr;
    DBusMessageIter array_iter;
    bool open = false;
public:
    DBusMessageIterOpenContainerGuard(DBusMessageIter *iter, int container_type,  const char *contained_signature);
    ~DBusMessageIterOpenContainerGuard();
    DBusMessageIter *get_array_iter();
};

}

#endif // DBUSMESSAGEITEROPENCONTAINERGUARD_H
