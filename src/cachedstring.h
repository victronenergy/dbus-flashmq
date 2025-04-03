#ifndef CACHEDSTRING_H
#define CACHEDSTRING_H

#include <string>

namespace dbus_flashmq
{

/**
 * @brief Cached string that by the grace of RAII should be imune to staleness when used as a member of a class.
 */
struct CachedString
{
    std::string v;

    CachedString() = default;
    CachedString(CachedString &&other) = delete;
    CachedString(const CachedString &other);
    CachedString &operator=(const CachedString &other);
};

}

#endif // CACHEDSTRING_H
