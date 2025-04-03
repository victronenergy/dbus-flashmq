#include "fdguard.h"

#include <unistd.h>

using namespace dbus_flashmq;

FdGuard::FdGuard(int fd) :
    fd(fd)
{

}

FdGuard::~FdGuard()
{
    if (fd > 0)
        close(fd);
    fd = -1;
}

int FdGuard::get() const
{
    return this->fd;
}
