#include <sys/epoll.h>
#include <cstring>

#include "flashmq-dbus-plugin-tests.h"
#include "vendor/flashmq_plugin.h"
#include "testerglobals.h"
#include "utils.h"
#include "state.h"

#define MAX_EVENTS 25

void tests_init_once()
{
    testCount = 0;
    failCount = 0;
}

using namespace dbus_flashmq;

AuthResult dbus_flashmq::acl_check_helper(
    void *thread_data, const AclAccess access, const std::string &clientid, const std::string &username,
    const std::string &topic, const std::string &payload)
{
    std::vector<std::string> subtopics = splitToVector(topic, '/');
    return flashmq_plugin_acl_check(thread_data, access, clientid, username, topic, subtopics, "", payload, 0, false, {}, {}, nullptr);
}

int integration_permission_tests(void *data)
{
    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "some_client_id", "token/evcharger/HQ2401ABCDE", "I/local/in/evcharger/HQ2401ABCDE/vregset/gx2evcs"),
        AuthResult::success);

    // Normal token users can publish on out paths.
    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "some_client_id", "token/evcharger/HQ2401ABCDE", "I/local/out/evcharger/HQ2401ABCDE/vregset/gx2evcs"),
        AuthResult::acl_denied);


    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "some_client_id", "token/evcharger/QQ2401ABCDE", "I/local/in/evcharger/HQ2401ABCDE/vregset/gx2evcs"),
        AuthResult::acl_denied);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "some_client_id", "token/evcharger/QQ2401ABCDE", "I/local/out/evcharger/HQ2401ABCDE/vregset/gx2evcs"),
        AuthResult::acl_denied);


    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "some_client_id", "token/evcharger/HQ2401ABCDE", "I/local/in/otherrole/HQ2401ABCDE/vregset/gx2evcs"),
        AuthResult::acl_denied);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "some_client_id", "token/evcharger/HQ2401ABCDE", "I/local/out/otherrole/HQ2401ABCDE/vregset/gx2evcs"),
        AuthResult::acl_denied);

    return 0;
}

int read_only_vrm_mode_tests(void *data)
{
    State *state = static_cast<State*>(data);
    VrmPortalMode vrm_mode_org = state->vrm_portal_mode;

    state->vrm_portal_mode = VrmPortalMode::Full;

    const std::string vrmid = state->unique_vrm_id;

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "W/" + vrmid + "/keepalive/bla/0", "{ \"value\": 0 }"),
        AuthResult::success);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "R/" + vrmid + "/settings/0/Settings/Gui/StartWithMenuView"),
        AuthResult::success);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "P/" + vrmid + "/in"),
        AuthResult::success);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "I/" + vrmid + "/in"),
        AuthResult::success);

    state->vrm_portal_mode = VrmPortalMode::ReadOnly;

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "W/" + vrmid + "/x", "{ \"value\": 0 }"),
        AuthResult::acl_denied);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "R/" + vrmid + "/settings/0/Settings/LoadSheddingApi/Token"),
        AuthResult::success);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "P/" + vrmid + "/in"),
        AuthResult::acl_denied);

    FMQ_COMPARE(
        acl_check_helper(data, AclAccess::write, "GXdbus_foo", "GXdbus", "I/" + vrmid + "/in"),
        AuthResult::acl_denied);


    state->vrm_portal_mode = vrm_mode_org;

    return 0;
}


int pre_event_loop_test(void *data)
{
    FMQ_COMPARE(true, true);

    integration_permission_tests(data);
    read_only_vrm_mode_tests(data);

    return 0;
}

int main(int argc, char **argv)
{
    tests_init_once();

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

    pre_event_loop_test(data);

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

    if (failCount > 0)
        return 66;

    return 0;
}
