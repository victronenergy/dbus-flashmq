#include "dbusmessageiteropencontainerguard.h"

#include <stdexcept>

using namespace dbus_flashmq;

DBusMessageIterOpenContainerGuard::DBusMessageIterOpenContainerGuard(DBusMessageIter *iter, int container_type, const char *contained_signature) :
    iter(iter)
{
    if (contained_signature)
    {
        std::string_view sig(contained_signature);

        // Dbus segfaults if you don't protect this.
        if (sig.empty())
            throw std::runtime_error("Dbus container iterator contained_signature can't be empty string");
    }

    if (!dbus_message_iter_open_container(iter, container_type, contained_signature, &array_iter))
    {
        throw std::runtime_error("dbus_message_iter_open_container failed.");
    }

    open = true;
}

DBusMessageIterOpenContainerGuard::~DBusMessageIterOpenContainerGuard()
{
    if (!iter)
        return;

    if (open)
        dbus_message_iter_close_container(iter, &array_iter);

    iter = nullptr;
}

DBusMessageIter *DBusMessageIterOpenContainerGuard::get_array_iter()
{
    return &array_iter;
}
