#include "utils.h"
#include <sys/epoll.h>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <cctype>
#include <sstream>
#include <sys/wait.h>
#include <iostream>

#include "exceptions.h"
#include "fdguard.h"

int dbus_watch_flags_to_epoll(int dbus_flags)
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

int epoll_flags_to_dbus_watch_flags(int epoll_flags)
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

std::vector<std::string> splitToVector(const std::string &input, const char sep, size_t max, bool keep_empty_parts)
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

std::string get_service_type(const std::string &service)
{
    if (service.find("com.victronenergy.") == std::string::npos)
        throw std::runtime_error("Not a victron service");

    const std::vector<std::string> parts = splitToVector(service, '.');
    return parts.at(2);
}

std::string get_uid_from_topic(const std::vector<std::string> &subtopics)
{
    return std::string();
}

uint32_t get_instance_from_items(const std::unordered_map<std::string, Item> &items)
{
    uint32_t deviceInstance = 0;

    for (auto &p : items)
    {
        const Item &i = p.second;
        if (i.get_path() == "/DeviceInstance")
        {
            deviceInstance = i.get_value().as_int();
            break;
        }
    }

    return deviceInstance;
}

void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

std::string get_stdout_from_process(const std::string &process)
{
    pid_t p;
    return get_stdout_from_process(process, p);
}

std::string get_stdout_from_process(const std::string &process, pid_t &out_pid)
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

int16_t s_to_int16(const std::string &s)
{
    int32_t x = std::stol(s);

    if (x > 32767 || x < -32768)
        throw ValueError("Value '" + s + "' too big for int16");

    return x;
}


uint8_t s_to_uint8(const std::string &s)
{
    uint16_t x = std::stoi(s);

    if (x & 0xFF00)
        throw ValueError("Value '" + s + "' too big for uint8");

    return x;
}

uint16_t s_to_uint16(const std::string &s)
{
    uint32_t x = std::stol(s);

    if (x & 0xFFFF0000)
        throw ValueError("Value '" + s + "' too big for uint16");

    return x;
}


std::string dbus_message_get_error_name_safe(DBusMessage *msg)
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










