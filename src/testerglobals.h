#ifndef TESTERGLOBALS_H
#define TESTERGLOBALS_H

#include <unordered_map>
#include <memory>
#include "queuedtasks.h"

extern int testCount;
extern int failCount;

#define FMQ_COMPARE(actual, expected) \
if (!fmq_compare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return 1; \

    bool fmq_assert(bool b, const char *failmsg, const char *actual, const char *expected, const char *file, int line);

template <typename T1, typename T2>
inline bool fmq_compare(const T1 &t1, const T2 &t2, const char *actual, const char *expected, const char *file, int line)
{
    return fmq_assert(t1 == t2, "Compared values are not the same", actual, expected, file, line);
}

namespace dbus_flashmq
{

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

}

#endif // TESTERGLOBALS_H
