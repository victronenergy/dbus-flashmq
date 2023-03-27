#include "dbusmessageitersignature.h"

std::string DBusMessageIterSignature::getSignature(DBusMessageIter *iter) const
{
    char *_sig = dbus_message_iter_get_signature(iter);
    std::string signature(_sig);
    dbus_free(_sig);
    _sig = nullptr;
    return signature;
}

DBusMessageIterSignature::DBusMessageIterSignature(DBusMessageIter *iter) :
    signature(getSignature(iter))
{

}
