#include "cachedstring.h"

CachedString::CachedString(const CachedString &other)
{
    *this = other;
}

CachedString &CachedString::operator=(const CachedString &other)
{
    this->v.clear();
    return *this;
}
