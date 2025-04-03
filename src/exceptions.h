#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdexcept>

namespace dbus_flashmq
{

class ValueError : public std::runtime_error
{
public:
    ValueError(const std::string &msg) : std::runtime_error(msg) {}
};

class ItemNotFound : public std::runtime_error
{
public:
    const std::string service;
    const std::string dbus_like_path;

    ItemNotFound(const std::string &msg, const std::string &service, const std::string &dbus_like_path);
};

}

#endif // EXCEPTIONS_H
