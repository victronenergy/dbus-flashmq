#include "shortservicename.h"

#include <sstream>
#include <vector>

#include "exceptions.h"
#include "utils.h"

using namespace dbus_flashmq;

std::string ShortServiceName::get_value(const std::string &service, ServiceIdentifier instance)
{
    std::string short_name = make_short(service);
    std::ostringstream o;
    o << short_name << '/' << instance.getValue();
    return std::string(o.str());
}

std::string ShortServiceName::make_short(std::string service)
{
    if (service.find("com.victronenergy.") != std::string::npos)
    {
        const std::vector<std::string> parts = splitToVector(service, '.');
        service = parts.at(2);
    }
    else if (service.find(".") != std::string::npos || service.find("/") != std::string::npos)
    {
        throw ValueError("String doesn't look like com.victronenergy.something or something without dots or slashes.");
    }

    return service;
}

ShortServiceName::ShortServiceName(const std::string &service, ServiceIdentifier instance) :
    std::string(get_value(service, instance)),
    service_type(make_short(service))
{

}

ShortServiceName::ShortServiceName() : std::string()
{

}
