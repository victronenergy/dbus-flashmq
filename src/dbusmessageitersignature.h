#ifndef DBUSMESSAGEITERSIGNATURE_H
#define DBUSMESSAGEITERSIGNATURE_H

#include <dbus-1.0/dbus/dbus.h>
#include <string>

class DBusMessageIterSignature
{
    std::string getSignature(DBusMessageIter *iter) const;
public:
    DBusMessageIterSignature(DBusMessageIter *iter);

    const std::string signature;
};

#endif // DBUSMESSAGEITERSIGNATURE_H
