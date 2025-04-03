#include "testerglobals.h"
#include "sys/epoll.h"
#include <cstring>
#include "vendor/flashmq_plugin.h"

using namespace dbus_flashmq;

TesterGlobals::TesterGlobals()
{

}

TesterGlobals *TesterGlobals::getInstance()
{
    static TesterGlobals *instance = new TesterGlobals;
    return instance;
}

void TesterGlobals::pollExternalFd(int fd, uint32_t events, const std::weak_ptr<void> &p)
{
    int mode = EPOLL_CTL_MOD;
    auto pos = watchedFds.find(fd);
    if (pos == watchedFds.end())
    {
        mode = EPOLL_CTL_ADD;
    }

    if (mode == EPOLL_CTL_ADD || !p.expired())
        watchedFds[fd] = p;

    struct epoll_event ev;
    memset(&ev, 0, sizeof (struct epoll_event));
    ev.data.fd = fd;
    ev.events = events;
    if (epoll_ctl(this->epoll_fd, mode, fd, &ev) == -1)
    {
        flashmq_logf(LOG_ERR, "Adding/changing externally watched fd %d to/from epoll produced error: %s", fd, strerror(errno));
    }
}

void TesterGlobals::pollExternalRemove(int fd)
{
    this->watchedFds.erase(fd);
    if (epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, fd, NULL) != 0)
    {
        flashmq_logf(LOG_ERR, "Removing externally watched fd %d from epoll produced error: %s", fd, strerror(errno));
    }
}
