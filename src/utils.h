#ifndef UTILS_H
#define UTILS_H

#include <dbus-1.0/dbus/dbus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <sys/random.h>

#include "types.h"
#include "serviceidentifier.h"

int dbus_watch_flags_to_epoll(int dbus_flags);
int epoll_flags_to_dbus_watch_flags(int epoll_flags);
std::vector<std::string> splitToVector(const std::string &input, const char sep, size_t max = std::numeric_limits<int>::max(), bool keep_empty_parts = true);
std::string get_service_type(const std::string &service);
std::string get_uid_from_topic(const std::vector<std::string> &subtopics);
ServiceIdentifier get_instance_from_items(const std::unordered_map<std::string, Item> &items);
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
std::string get_stdout_from_process(const std::string &process);
std::string get_stdout_from_process(const std::string &process, pid_t &out_pid);
std::string dbus_message_get_error_name_safe(DBusMessage *msg);

int16_t s_to_int16(const std::string &s);
uint8_t s_to_uint8(const std::string &s);
uint16_t s_to_uint16(const std::string &s);

template<class T>
T get_random()
{
    std::vector<T> buf(1);
    getrandom(buf.data(), sizeof(T), 0);
    T val = buf.at(0);
    return val;
}

bool client_id_is_bridge(const std::string &clientid);
bool crypt_match(const std::string &phrase, const std::string &crypted);

#endif // UTILS_H
