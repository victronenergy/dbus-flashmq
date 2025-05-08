#include "utils.h"
#include <sys/epoll.h>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <cctype>
#include <sstream>
#include <sys/wait.h>
#include <iostream>
#include <sys/resource.h>
#include <crypt.h>

#include "exceptions.h"
#include "fdguard.h"

using namespace dbus_flashmq;

int dbus_flashmq::dbus_watch_flags_to_epoll(int dbus_flags)
{
    int epoll_flags = 0;

    if (dbus_flags & DBUS_WATCH_READABLE)
    {
        epoll_flags |= EPOLLIN;
    }
    if (dbus_flags & DBUS_WATCH_WRITABLE)
    {
        epoll_flags |= EPOLLOUT;
    }

    return epoll_flags;
}

int dbus_flashmq::epoll_flags_to_dbus_watch_flags(int epoll_flags)
{
    int dbus_flags = 0;

    if (epoll_flags & EPOLLIN)
        dbus_flags |= DBusWatchFlags::DBUS_WATCH_READABLE;
    if (epoll_flags & EPOLLOUT)
        dbus_flags |= DBusWatchFlags::DBUS_WATCH_WRITABLE;
    if (epoll_flags & EPOLLERR)
        dbus_flags |= DBusWatchFlags::DBUS_WATCH_ERROR;
    if (epoll_flags & EPOLLHUP)
        dbus_flags |= DBusWatchFlags::DBUS_WATCH_HANGUP;

    return dbus_flags;
}

std::vector<std::string> dbus_flashmq::splitToVector(const std::string &input, const char sep, size_t max, bool keep_empty_parts)
{
    const auto substring_count = std::count(input.begin(), input.end(), sep) + 1;
    std::vector<std::string> result;
    result.reserve(substring_count);

    size_t start = 0;
    size_t end;

    while (result.size() < max && (end = input.find(sep, start)) != std::string::npos)
    {
        if (start != end || keep_empty_parts)
            result.push_back(input.substr(start, end - start));
        start = end + 1; // increase by length of seperator.
    }
    if (start != input.size() || keep_empty_parts)
        result.push_back(input.substr(start, std::string::npos));

    return result;
}

std::string dbus_flashmq::get_service_type(const std::string &service)
{
    if (service.find("com.victronenergy.") == std::string::npos)
        throw std::runtime_error("Not a victron service");

    const std::vector<std::string> parts = splitToVector(service, '.');
    return parts.at(2);
}

std::string dbus_flashmq::get_uid_from_topic(const std::vector<std::string> &subtopics)
{
    return std::string();
}

ServiceIdentifier dbus_flashmq::get_instance_from_items(const std::unordered_map<std::string, Item> &items)
{
    uint32_t deviceInstance = 0;

    for (auto &p : items)
    {
        const Item &i = p.second;
        if (i.get_path() == "/DeviceInstance")
        {
            deviceInstance = i.get_value().value.as_int();
            return deviceInstance;
        }
    }

    for (auto &p : items)
    {
        const Item &i = p.second;
        if (i.get_path() == "/Identifier")
        {
            const std::string s = i.get_value().value.as_text();
            return s;
        }
    }

    return deviceInstance;
}

void dbus_flashmq::ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void dbus_flashmq::rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void dbus_flashmq::trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

std::string dbus_flashmq::get_stdout_from_process(const std::string &process)
{
    pid_t p;
    return get_stdout_from_process(process, p);
}

std::string dbus_flashmq::get_stdout_from_process(const std::string &process, pid_t &out_pid)
{
    int pipe_fds[2];

    if (pipe(pipe_fds) < 0)
        throw std::runtime_error("Can't create pipes");

    pid_t pid = fork();

    if (pid == -1)
        throw std::runtime_error("What the fork?");

    if (pid == 0) // the forked instance, which we are transforming with execlp
    {
        // Capture stdout on the write end of the pipe.
        while ((dup2(pipe_fds[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}

        // We duplicated our write fd, so we can close these.
        close(pipe_fds[1]);
        close(pipe_fds[0]);

        // Brute force close any open file/socket, because I don't want the child to see them.
        struct rlimit rlim;
        memset(&rlim, 0, sizeof (struct rlimit));
        getrlimit(RLIMIT_NOFILE, &rlim);
        for (rlim_t i = 3; i < rlim.rlim_cur; ++i) close (i);

        execlp(process.c_str(), process.c_str(), nullptr);
        std::cerr << strerror(errno) << std::endl;
        exit(66);
    }

    out_pid = pid;

    close(pipe_fds[1]); // Close the write-end of the pipe, because we're only reading from it.
    FdGuard fd_guard = pipe_fds[0];

    int status = 0;
    waitpid(pid, &status, 0);
    out_pid = -1;

    std::ostringstream o;

    if (!WIFEXITED(status))
    {
        if (WIFSIGNALED(status))
        {
            o << "Calling '" << process << "' signalled: " << WTERMSIG(status);
            throw std::runtime_error(o.str());
        }

        o << "Process '" << process << "did not exit normally. Does it exist?";
        throw std::runtime_error(o.str());
    }
    else
    {
        int return_code = WEXITSTATUS(status);

        if (return_code != 0)
        {
            o << "Process '" << process << "' exitted with " << return_code;
            throw std::runtime_error(o.str());
        }
    }

    char buf[256];
    memset(&buf, 0, 256);

    ssize_t n;
    while ((n = read(fd_guard.get(), &buf, 128)) < 0)
    {
        if (n < 0 && errno == EINTR)
            continue;

        throw std::runtime_error(strerror(errno));
    }

    std::string result(buf);
    return result;
}

int16_t dbus_flashmq::s_to_int16(const std::string &s)
{
    int32_t x = std::stol(s);

    if (x > 32767 || x < -32768)
        throw ValueError("Value '" + s + "' too big for int16");

    return x;
}


uint8_t dbus_flashmq::s_to_uint8(const std::string &s)
{
    uint16_t x = std::stoi(s);

    if (x & 0xFF00)
        throw ValueError("Value '" + s + "' too big for uint8");

    return x;
}

uint16_t dbus_flashmq::s_to_uint16(const std::string &s)
{
    uint32_t x = std::stol(s);

    if (x & 0xFFFF0000)
        throw ValueError("Value '" + s + "' too big for uint16");

    return x;
}


std::string dbus_flashmq::dbus_message_get_error_name_safe(DBusMessage *msg)
{
    std::string result;
    const int msg_type = dbus_message_get_type(msg);

    if (msg_type == DBUS_MESSAGE_TYPE_ERROR)
    {
        const char *_msg = dbus_message_get_error_name(msg);

        if (_msg)
            result = _msg;
    }

    return result;
}

bool dbus_flashmq::username_is_bridge(const std::string &username)
{
    return username == "GXdbus" || username == "GXrpc";
}

/**
 * @brief crypt_match uses the system crypt function to match hashed passwords.
 * @param phrase like 'hallo'
 * @param crypted like '$2a$08$LBfjL0PfMBbjWxCzLBfjLurkA7K0tuDn44rNUXDBvatSgSqHvwaHS'
 * @return
 *
 * Password 'hallo' yields this:
 * $2a$08$LBfjL0PfMBbjWxCzLBfjLurkA7K0tuDn44rNUXDBvatSgSqHvwaHS
 */
bool dbus_flashmq::crypt_match(const std::string &phrase, const std::string &crypted)
{
    struct crypt_data data;
    memset(&data, 0, sizeof(struct crypt_data));
    crypt_r(phrase.c_str(), crypted.c_str(), &data);

    const std::string new_crypt(data.output);
    return crypted == new_crypt;
}


VrmPortalMode dbus_flashmq::parseVrmPortalMode(int val)
{
    switch(val)
    {
    case(0):
        return VrmPortalMode::Off;
    case(1):
        return VrmPortalMode::ReadOnly;
    case(2):
        return VrmPortalMode::Full;
    default:
        return VrmPortalMode::Unknown;
    }
}






