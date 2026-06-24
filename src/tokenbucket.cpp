#include "tokenbucket.h"

bool TokenBucket::consume(double count)
{
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> diff = now - last_update;
    m_tokens += diff.count() * m_fill_rate_per_second;
    m_tokens = std::min<double>(m_tokens, m_max_tokens);
    last_update = now;

    if (m_tokens >= count)
    {
        m_tokens -= count;
        m_logged = false;
        return true;
    }
    return false;
}
