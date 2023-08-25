/*
This file is part of FlashMQ (https://www.flashmq.org)
Copyright (C) 2021-2023 Wiebe Cazemier

Relicensed by copyright holder for Victron Energy BV, dbus-flashmq:

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#include "network.h"

#include <arpa/inet.h>
#include <string.h>
#include <stdexcept>

#include "utils.h"


Network::Network(const std::string &network)
{
    memset(&this->data, 0, sizeof (struct sockaddr_in6));

    if (network.find(".") != std::string::npos)
    {
        struct sockaddr_in *_sockaddr_in = reinterpret_cast<struct sockaddr_in*>(&this->data);

        _sockaddr_in->sin_family = AF_INET;

        int maskbits = inet_net_pton(AF_INET, network.c_str(), &_sockaddr_in->sin_addr, sizeof(struct in_addr));

        if (maskbits < 0)
            throw std::runtime_error("Network '" + network + "' is not a valid network notation.");

        uint32_t _netmask = (uint64_t)0xFFFFFFFFu << (32 - maskbits);
        this->in_mask = htonl(_netmask);
    }
    else if (network.find(":") != std::string::npos)
    {
        // Why does inet_net_pton not support AF_INET6...?

        struct sockaddr_in6 *_sockaddr_in6 = reinterpret_cast<struct sockaddr_in6*>(&this->data);

        _sockaddr_in6->sin6_family = AF_INET6;

        std::vector<std::string> parts = splitToVector(network, '/', 2, false);
        std::string &addrPart = parts[0];
        int maskbits = 128;

        if (parts.size() == 2)
        {
            const std::string &maskstring = parts[1];

            const bool invalid_chars = std::any_of(maskstring.begin(), maskstring.end(), [](const char &c) { return c < '0' || c > '9'; });
            if (invalid_chars || maskstring.length() > 3)
                throw std::runtime_error("Mask '" + maskstring + "' is not valid");

            maskbits = std::stoi(maskstring);
        }

        if (inet_pton(AF_INET6, addrPart.c_str(), &_sockaddr_in6->sin6_addr) != 1)
        {
            throw std::runtime_error("Network '" + network + "' is not a valid network notation.");
        }

        if (maskbits > 128 || maskbits < 0)
        {
            throw std::runtime_error("Network '" + network + "' is not a valid network notation.");
        }

        int m = maskbits;
        memset(in6_mask, 0, 16);
        int i = 0;
        const uint64_t x = 0xFFFFFFFF00000000;

        while (m >= 0)
        {
            int shift_remainder = std::min<int>(m, 32);
            uint32_t b = x >> shift_remainder;
            in6_mask[i++] = htonl(b);
            m -= 32;
        }

        for (int i = 0; i < 4; i++)
        {
            network_addr_relevant_bits.__in6_u.__u6_addr32[i] = _sockaddr_in6->sin6_addr.__in6_u.__u6_addr32[i] & in6_mask[i];
        }
    }
    else
    {
        throw std::runtime_error("Network '" + network + "' is not a valid network notation.");
    }
}

bool Network::match(const sockaddr *addr) const
{
    const struct sockaddr* _sockaddr = reinterpret_cast<const struct sockaddr*>(&this->data);

    if (_sockaddr->sa_family == AF_INET)
    {
        const struct sockaddr_in *_sockaddr_in = reinterpret_cast<const struct sockaddr_in*>(&this->data);
        const struct sockaddr_in *_addr = reinterpret_cast<const struct sockaddr_in*>(addr);
        return _sockaddr->sa_family == addr->sa_family && ((_sockaddr_in->sin_addr.s_addr & this->in_mask) == (_addr->sin_addr.s_addr & this->in_mask));
    }
    else if (_sockaddr->sa_family == AF_INET6)
    {
        const struct sockaddr_in6 *arg_addr = reinterpret_cast<const struct sockaddr_in6*>(addr);

        struct in6_addr arg_addr_relevant_bits;
        for (int i = 0; i < 4; i++)
        {
            arg_addr_relevant_bits.__in6_u.__u6_addr32[i] = arg_addr->sin6_addr.__in6_u.__u6_addr32[i] & in6_mask[i];
        }

        uint8_t matches[4];
        for (int i = 0; i < 4; i++)
        {
            matches[i] = arg_addr_relevant_bits.__in6_u.__u6_addr32[i] == network_addr_relevant_bits.__in6_u.__u6_addr32[i];
        }

        return (_sockaddr->sa_family == addr->sa_family) & (matches[0] & matches[1] & matches[2] & matches[3]);
    }

    return false;
}

bool Network::match(const sockaddr_in *addr) const
{
    const struct sockaddr *_addr = reinterpret_cast<const struct sockaddr*>(addr);
    return match(_addr);
}

bool Network::match(const sockaddr_in6 *addr) const
{
    const struct sockaddr *_addr = reinterpret_cast<const struct sockaddr*>(addr);
    return match(_addr);
}
