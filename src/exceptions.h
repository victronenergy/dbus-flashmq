#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdexcept>

class ValueError : public std::runtime_error
{
public:
    ValueError(const std::string &msg) : std::runtime_error(msg) {}
};


#endif // EXCEPTIONS_H
