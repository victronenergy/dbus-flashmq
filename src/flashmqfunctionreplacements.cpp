//#include "flashmqfunctionreplacements.h"

#include "vendor/flashmq_plugin.h"

#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "testerglobals.h"

using namespace dbus_flashmq;

std::string getLogLevelString(int level)
{
    switch (level)
    {
    case LOG_NONE:
        return "NONE";
    case LOG_INFO:
        return "INFO";
    case LOG_NOTICE:
        return "NOTICE";
    case LOG_WARNING:
        return "WARNING";
    case LOG_ERR:
        return "ERROR";
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_SUBSCRIBE:
        return "SUBSCRIBE";
    case LOG_UNSUBSCRIBE:
        return "UNSUBSCRIBE";
    default:
        return "UNKNOWN LOG LEVEL";
    }
}

/**
 * @brief flashmq_logf is normally provided by FlashMQ. We need to have it in our test env as well.
 * @param level
 * @param str
 */
void flashmq_logf(int level, const char *str, ...)
{
    (void)level;

    time_t time = std::time(nullptr);
    struct tm tm = *std::localtime(&time);

    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] [" << getLogLevelString(level) << "] ";
    oss.flush();
    const std::string prefix = oss.str();

    const int buf_size = 512;
    char buf[buf_size + 1];
    buf[buf_size] = 0;

    va_list valist;
    va_start(valist, str);
    vsnprintf(buf, buf_size, str, valist);
    va_end(valist);

    const std::string formatted_line(buf);
    std::ostringstream oss2;
    oss2 << prefix << formatted_line;
    std::cout << "\033[01;33m" << oss2.str() << "\033[00m" << std::endl;
}

void flashmq_poll_add_fd(int fd, uint32_t events, const std::weak_ptr<void> &p)
{
    TesterGlobals *globals = TesterGlobals::getInstance();
    globals->pollExternalFd(fd, events, p);
}

void flashmq_poll_remove_fd(uint32_t fd)
{
    TesterGlobals *globals = TesterGlobals::getInstance();
    globals->pollExternalRemove(fd);
}

uint32_t flashmq_add_task(std::function<void ()> f, uint32_t delay_in_ms)
{
    TesterGlobals *globals = TesterGlobals::getInstance();
    return globals->delayedTasks.addTask(f, delay_in_ms);
}

void flashmq_remove_task(uint32_t id)
{
    TesterGlobals *globals = TesterGlobals::getInstance();

    globals->delayedTasks.eraseTask(id);
}

void flashmq_publish_message(const std::string &topic, const uint8_t qos, const bool retain, const std::string &payload, uint32_t expiryInterval,
                             const std::vector<std::pair<std::string, std::string>> *userProperties,
                             const std::string *responseTopic, const std::string *correlationData, const std::string *contentType)
{
    std::cout << "DUMMY: " << topic << ": " << payload << std::endl;
}

/**
 * @brief flashmq_get_client_address is normally provided by FlashMQ, but for out test binary, we need to mock it, because we have no network clients.
 * @param client
 * @param text
 * @param addr
 */
void flashmq_get_client_address(const std::weak_ptr<Client> &client, std::string *text, FlashMQSockAddr *addr)
{
    std::shared_ptr<Client> c = client.lock();

    if (!c)
        return;

    if (text)
        *text = "dummy-we-dont-know";

    if (addr)
    {
        struct sockaddr_in dummy_addr;
        memset(&dummy_addr, 0, sizeof(struct sockaddr_in));

        inet_pton(AF_INET, "127.0.0.1", &dummy_addr.sin_addr);
        dummy_addr.sin_family = AF_INET;
        dummy_addr.sin_port = htons(666);

        memcpy(addr->getAddr(), &dummy_addr, addr->getLen());
    }
}

sockaddr *FlashMQSockAddr::getAddr()
{
    return reinterpret_cast<struct sockaddr*>(&this->addr_in6);
}

constexpr int FlashMQSockAddr::getLen()
{
    return sizeof(struct sockaddr_in6);
}

void flashmq_continue_async_authentication(const std::weak_ptr<Client> &client, AuthResult result, const std::string &authMethod, const std::string &returnData)
{

}

void flashmq_get_session_pointer(const std::string &clientid, const std::string &username, std::weak_ptr<Session> &sessionOut)
{

}

void flashmq_get_client_pointer(const std::weak_ptr<Session> &session, std::weak_ptr<Client> &clientOut)
{

}
