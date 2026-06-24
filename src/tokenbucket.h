#ifndef TOKENBUCKET_H
#define TOKENBUCKET_H

#include <chrono>

class TokenBucket
{
    const double m_max_tokens {};
    const double m_fill_rate_per_second {};
    double m_tokens {};
    std::chrono::time_point<std::chrono::steady_clock> last_update = std::chrono::steady_clock::now();
    bool m_logged = false;

public:

    TokenBucket(double max_tokens, double fill_rate_per_second) :
        m_max_tokens(max_tokens),
        m_fill_rate_per_second(fill_rate_per_second),
        m_tokens(max_tokens)
    {

    }

    bool consume(double count);
    bool is_logged() const { return m_logged; }
    void set_logged() { m_logged = true;}
};

#endif // TOKENBUCKET_H
