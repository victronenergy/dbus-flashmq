#ifndef BOOMSTRING_H
#define BOOMSTRING_H

#include <string>

namespace dbus_flashmq
{

class BoomString
{
    std::string s;
public:
    BoomString() = default;
    BoomString(const std::string &val);
    BoomString(const BoomString &other);
    BoomString &operator=(const BoomString &other);
    const std::string &get() const;
};

}

#endif // BOOMSTRING_H
