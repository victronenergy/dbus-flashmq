#include "dbuspendingmessagecallguard.h"

DBusPendingMessageCallGuard::DBusPendingMessageCallGuard()
{

}

DBusPendingMessageCallGuard::~DBusPendingMessageCallGuard()
{
    dbus_pending_call_unref(d);
}


