#include "dbuspendingmessagecallguard.h"

using namespace dbus_flashmq;

DBusPendingMessageCallGuard::DBusPendingMessageCallGuard()
{

}

DBusPendingMessageCallGuard::~DBusPendingMessageCallGuard()
{
    dbus_pending_call_unref(d);
}


