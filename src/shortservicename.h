#ifndef SHORTSERVICENAME_H
#define SHORTSERVICENAME_H

#include <string>
#include <stdint.h>
#include "serviceidentifier.h"

namespace dbus_flashmq
{

class ShortServiceName : public std::string
{
    static std::string get_value(const std::string &service, ServiceIdentifier instance);
    static std::string make_short(std::string service);
public:
    std::string service_type;
    ShortServiceName(const std::string &service, ServiceIdentifier instance);
    ShortServiceName();
};

}

namespace std {

    template <>
    struct hash<dbus_flashmq::ShortServiceName>
    {
        std::size_t operator()(const dbus_flashmq::ShortServiceName& k) const
        {
            return std::hash<std::string>()(k);
        }
    };

}


#endif // SHORTSERVICENAME_H
