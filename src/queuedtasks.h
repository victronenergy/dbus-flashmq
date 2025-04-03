#ifndef QUEUEDTASKS_H
#define QUEUEDTASKS_H

#include <functional>
#include <set>
#include <unordered_map>
#include <memory>
#include <chrono>

namespace dbus_flashmq
{

struct QueuedTask
{
    std::chrono::time_point<std::chrono::steady_clock> when;
    uint32_t id = 0;

    bool operator<(const QueuedTask &rhs) const;
};

/**
 * @brief Contains delayed tasks to perform.
 *
 * This is only for the test env of the plugin. Normally all the tasks stuff is provided by FlashMQ.
 */
class QueuedTasks
{
    uint32_t nextId = 1;
    std::multiset<QueuedTask> queuedTasks;
    std::unordered_map<uint32_t, std::function<void()>> tasks;
    std::chrono::time_point<std::chrono::steady_clock> next = std::chrono::time_point<std::chrono::steady_clock>::max();

public:
    QueuedTasks();
    uint32_t addTask(std::function<void()> f, uint32_t delayInMs);
    void eraseTask(uint32_t id);
    uint32_t getTimeTillNext() const;
    void performAll();
};

}

#endif // QUEUEDTASKS_H
