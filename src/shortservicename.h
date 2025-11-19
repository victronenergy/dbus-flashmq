#ifndef SHORTSERVICENAME_H
#define SHORTSERVICENAME_H

#include <string>
#include <iostream>
#include "serviceidentifier.h"

namespace dbus_flashmq
{

class ShortServiceName
{
    static std::string make_short(std::string const &service);

    std::string m_service_type;
    std::string m_instance;
    std::string m_total;
public:
    ShortServiceName() = default;
    ShortServiceName(const std::string &service, const ServiceIdentifier &instance)
        : m_service_type(make_short(service))
        , m_instance(instance.getValue())
        , m_total(m_service_type + '/' + m_instance)
    { }
    ShortServiceName(ShortServiceName const & other) = default;
    ShortServiceName& operator=(ShortServiceName const & other) = default;
    const std::string& service_type() const { return m_service_type; }
    const std::string& instance() const { return m_instance; }
    const std::string& total() const { return m_total; }
};

inline bool operator==(const ShortServiceName& a, const ShortServiceName& b)
{
    return a.total() == b.total();
}

inline bool operator!=(const ShortServiceName& a, const ShortServiceName& b)
{
    return a.total() != b.total();
}

inline std::ostream& operator<<(std::ostream& os, const ShortServiceName& s)
{
    os << s.total();
    return os;
}

}

namespace std {

    template <>
    struct hash<dbus_flashmq::ShortServiceName>
    {
        std::size_t operator()(const dbus_flashmq::ShortServiceName& k) const
        {
            return std::hash<std::string>()(k.total());
        }
    };

}


#endif // SHORTSERVICENAME_H
