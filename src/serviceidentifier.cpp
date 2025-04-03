#include "serviceidentifier.h"
#include <cassert>

using namespace dbus_flashmq;

ServiceIdentifier::ServiceIdentifier(int instance) :
    val(std::to_string(instance))
{

}

ServiceIdentifier::ServiceIdentifier(const std::string &identifier) :
    val(identifier)
{

}

ServiceIdentifier::ServiceIdentifier() :
    val(std::to_string(0))
{

}

const std::string &ServiceIdentifier::getValue() const
{
    return val;
}

ServiceIdentifier &ServiceIdentifier::operator=(const ServiceIdentifier &rhs)
{
    assert(this != &rhs);
    this->val = rhs.val;
    return *this;
}
