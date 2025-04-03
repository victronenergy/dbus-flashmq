#include "boomstring.h"

#include <stdexcept>

using namespace dbus_flashmq;

BoomString::BoomString(const std::string &val) :
    s(val)
{

}

BoomString::BoomString(const BoomString &other)
{
    *this = other;
}

BoomString &BoomString::operator=(const BoomString &other)
{
    if (!this->s.empty() && this->s != other.s)
        throw std::runtime_error("Programming error: trying to change a read-only string");

    this->s = other.s;
    return *this;
}

const std::string &BoomString::get() const
{
    return s;
}
