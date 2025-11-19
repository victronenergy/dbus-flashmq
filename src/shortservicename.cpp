#include "shortservicename.h"

#include <algorithm>

#include "exceptions.h"

using namespace dbus_flashmq;

std::string ShortServiceName::make_short(std::string const &service)
{
    static constexpr std::string_view VICTRON_PREFIX{"com.victronenergy."};
    if (service.starts_with(VICTRON_PREFIX))
    {
        // Skip "com.victronenergy."
        const auto startPos = service.begin() + VICTRON_PREFIX.size();
        // Find the next '.' after "com.victronenergy."
        const auto dotPos = std::find(startPos, service.end(), '.');
        // Return [startPos, dotPos>. It is ok for dotPos to be service.end()
        return std::string(startPos, dotPos);
    }
    else if (service.find_first_of("./") == std::string::npos)
    {
        // Does not contain . or /, so assume it's already short
        return service;
    }

    throw ValueError("String doesn't look like com.victronenergy.something or something without dots or slashes.");
}
