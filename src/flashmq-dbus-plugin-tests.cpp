#include <sys/epoll.h>
#include <cstring>

#include "flashmq-dbus-plugin-tests.h"
#include "vendor/flashmq_plugin.h"
#include "testerglobals.h"
#include "utils.h"

#define MAX_EVENTS 25

int main(int argc, char **argv)
{
    if (!crypt_match("hallo", "$2a$08$LBfjL0PfMBbjWxCzLBfjLurkA7K0tuDn44rNUXDBvatSgSqHvwaHS"))
    {
        throw std::runtime_error("crypt test failed.");
    }

    std::unordered_map<std::string, std::string> pluginOpts;

    TesterGlobals *globals = TesterGlobals::getInstance();
    globals->epoll_fd = epoll_create(1024);

    void *data = nullptr;;

    flashmq_plugin_main_init(pluginOpts);
    flashmq_plugin_allocate_thread_memory(&data, pluginOpts);

    flashmq_plugin_init(data, pluginOpts, false);

    struct epoll_event events[MAX_EVENTS];
    memset(&events, 0, sizeof (struct epoll_event)*MAX_EVENTS);

    while (true)
    {
        const uint32_t next_task_delay = globals->delayedTasks.getTimeTillNext();
        const uint32_t epoll_wait_time = std::min<uint32_t>(next_task_delay, 100);

        const int num_fds = epoll_wait(globals->epoll_fd, events, MAX_EVENTS, epoll_wait_time);

        if (epoll_wait_time == 0)
        {
            globals->delayedTasks.performAll();
        }

        if (num_fds < 0)
        {
            if (errno == EINTR)
                continue;
        }

        for (int i = 0; i < num_fds; i++)
        {
            int cur_fd = events[i].data.fd;

            auto pos = globals->watchedFds.find(cur_fd);
            if (pos != globals->watchedFds.end())
            {
                std::weak_ptr<void> &p = pos->second;
                flashmq_plugin_poll_event_received(data, cur_fd, events[i].events, p);
            }
        }
    }

    flashmq_plugin_deinit(data, pluginOpts, false);

    flashmq_plugin_deallocate_thread_memory(data, pluginOpts);
    flashmq_plugin_main_deinit(pluginOpts);

    return 0;
}
