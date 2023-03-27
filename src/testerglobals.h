#ifndef TESTERGLOBALS_H
#define TESTERGLOBALS_H

#include <unordered_map>
#include <memory>
#include "queuedtasks.h"

class TesterGlobals
{
    TesterGlobals();


public:
    std::unordered_map<int, std::weak_ptr<void>> watchedFds;
    int epoll_fd = -1;
    QueuedTasks delayedTasks;

    static TesterGlobals *getInstance();
    void pollExternalFd(int fd, uint32_t events, const std::weak_ptr<void> &p);
    void pollExternalRemove(int fd);
};

#endif // TESTERGLOBALS_H
