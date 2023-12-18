#ifndef SERVICEIDENTIFIER_H
#define SERVICEIDENTIFIER_H

#include <string>

/**
 * @brief The ServiceIdentifier class was created to support the new 'identifier' for places where DeviceInstance doesn't make sense to
 * represent multiple occurances.
 */
class ServiceIdentifier
{
    std::string val;
public:
    ServiceIdentifier(int instance);
    ServiceIdentifier(const std::string &identifier);
    ServiceIdentifier();
    const std::string &getValue() const;
    ServiceIdentifier &operator=(const ServiceIdentifier &rhs);
};

#endif // SERVICEIDENTIFIER_H
