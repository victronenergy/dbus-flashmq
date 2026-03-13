#ifndef UTILS_H
#define UTILS_H

#include <dbus-1.0/dbus/dbus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <sys/random.h>

#include "types.h"
#include "serviceidentifier.h"

namespace dbus_flashmq
{

uint32_t dbus_watch_flags_to_epoll(unsigned int dbus_flags);
unsigned int epoll_flags_to_dbus_watch_flags(uint32_t epoll_flags);
std::vector<std::string> splitToVector(const std::string &input, const char sep, size_t max = std::numeric_limits<size_t>::max(), bool keep_empty_parts = true);
std::string get_service_type(const std::string &service);
ServiceIdentifier get_instance_from_items(const std::unordered_map<std::string, Item> &items);
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
std::string get_stdout_from_process(const std::string &process);
std::string get_stdout_from_process(const std::string &process, pid_t &out_pid);
std::string dbus_message_get_error_name_safe(DBusMessage *msg);
std::string &str_make_lower(std::string &s);

template<class T>
T get_random()
{
    std::vector<T> buf(1);
    getrandom(buf.data(), sizeof(T), 0);
    T val = buf.at(0);
    return val;
}

bool username_is_bridge(const std::string &username);
bool crypt_match(const std::string &phrase, const std::string &crypted);
VrmPortalMode parseVrmPortalMode(int val);

}

#endif // UTILS_H
