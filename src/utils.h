#ifndef UTILS_H
#define UTILS_H

#include <dbus-1.0/dbus/dbus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
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
std::string hash_file(const std::filesystem::path &p);

template<typename T>
typename std::enable_if<std::is_signed<T>::value, long long>::type
value_to_int(const std::string &value)
{
    try
    {
        size_t len = 0;
        const long long newVal{std::stoll(value, &len)};
        if (len != value.length())
        {
            std::string err("Can't parse value to int: " + value);
            throw std::runtime_error(err);
        }
        return newVal;
    }
    catch (std::exception &ex)
    {
        std::string err("Can't parse value to int: " + value);
        throw std::runtime_error(err);
    }
}

template<typename T>
typename std::enable_if<std::is_unsigned<T>::value, long long unsigned>::type
value_to_int(const std::string &value)
{
    try
    {
        size_t len = 0;
        long long unsigned newVal{std::stoull(value, &len)};
        if (len != value.length())
        {
            std::string err("Can't parse value to int: " + value);
            throw std::runtime_error(err);
        }
        return newVal;
    }
    catch (std::exception &ex)
    {
        std::string err("Can't parse value to int: " + value);
        throw std::runtime_error(err);
    }
}

/**
 * @brief Parse int safe from conversion errors or extraneous chars in value string.
 * @param value
 * @param min
 * @param max
 * @return
 */
template<typename T>
T value_to_int_ranged(const std::string &value, const T min=std::numeric_limits<T>::min(), const T max=std::numeric_limits<T>::max())
{
    const auto newVal{value_to_int<T>(value)};
    if (newVal < min || newVal > max)
    {
        std::ostringstream oss;
        oss << "Value '" << value << "' out of range, which must be between ";

        if (sizeof(T) == 1)
            oss << static_cast<int>(min);
        else
            oss << min;

        oss << " and ";

        if (sizeof(T) == 1)
            oss << static_cast<int>(max);
        else
            oss << max;

        throw std::runtime_error(oss.str());
    }
    return static_cast<T>(newVal);
}

template<typename T>
std::string make_string(const T &input, const size_t offset, const size_t len)
{
    if (len + offset > input.size())
        throw std::out_of_range("make_string");

    const auto _offset = static_cast<ssize_t>(offset);
    const auto _len = static_cast<ssize_t>(len);
    return std::string(input.begin() + _offset, input.begin() + _offset + _len);
}

template<typename T>
std::string_view make_string_view(const T &input, const size_t offset, const size_t len)
{
    if (len + offset > input.size())
        throw std::out_of_range("make_string_view");

    const auto _offset = static_cast<ssize_t>(offset);
    const auto _len = static_cast<ssize_t>(len);
    return std::string_view(input.begin() + _offset, input.begin() + _offset + _len);
}

std::string base64_encode(const std::string_view input);

}

#endif // UTILS_H
