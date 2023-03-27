#ifndef SHORTSERVICENAME_H
#define SHORTSERVICENAME_H

#include <string>
#include <stdint.h>


class ShortServiceName : public std::string
{
    static std::string get_value(const std::string &service, uint32_t instance);
    static std::string make_short(std::string service);
public:
    std::string service_type;
    ShortServiceName(const std::string &service, uint32_t instance);
    ShortServiceName();
};

namespace std {

    template <>
    struct hash<ShortServiceName>
    {
        std::size_t operator()(const ShortServiceName& k) const
        {
            return std::hash<std::string>()(k);
        }
    };

}

#endif // SHORTSERVICENAME_H
