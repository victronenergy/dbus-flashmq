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
#include "utils.h"
#include <stdexcept>
#include <cstring>

using namespace dbus_flashmq;

/**
 * @brief Get socket family from addr that works with strict type aliasing.
 *
 * Disgruntled: if type aliasing rules are so strict, why is there no library
 * function to obtain the family from a sockaddr...?
 */
sa_family_t dbus_flashmq::getFamilyFromSockAddr(const sockaddr *addr)
{
    if (!addr)
        return AF_UNSPEC;

    sockaddr tmp;
    std::memcpy(&tmp, addr, sizeof(tmp));

    return tmp.sa_family;
}

Network::Network(const std::string &network)
{
    if (network.find(".") != std::string::npos)
    {
        struct sockaddr_in tmpaddr{};

        tmpaddr.sin_family = AF_INET;

        int maskbits = inet_net_pton(AF_INET, network.c_str(), &tmpaddr.sin_addr, sizeof(struct in_addr));

        if (maskbits < 0)
            throw std::runtime_error("Network '" + network + "' is not a valid network notation.");

        std::memcpy(this->data.data(), &tmpaddr, sizeof(tmpaddr));

        uint32_t _netmask = (uint64_t)0xFFFFFFFFu << (32 - maskbits);
        this->in_mask = htonl(_netmask);

        this->family = AF_INET;
    }
    else if (network.find(":") != std::string::npos)
    {
        // Why does inet_net_pton not support AF_INET6...?

        struct sockaddr_in6 tmpaddr{};

        tmpaddr.sin6_family = AF_INET6;

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

        if (inet_pton(AF_INET6, addrPart.c_str(), &tmpaddr.sin6_addr) != 1)
        {
            throw std::runtime_error("Network '" + network + "' is not a valid network notation.");
        }

        std::memcpy(this->data.data(), &tmpaddr, sizeof(tmpaddr));

        if (maskbits > 128 || maskbits < 0)
        {
            throw std::runtime_error("Network '" + network + "' is not a valid network notation.");
        }

        int m = maskbits;
        int i = 0;
        const uint64_t x{0xFFFFFFFF00000000};

        while (m > 0)
        {
            int shift_remainder = std::min<int>(m, 32);
            uint32_t b = x >> shift_remainder;
            in6_mask.at(i++) = htonl(b);
            m -= 32;
        }

        for (int i = 0; i < 4; i++)
        {
            network_addr_relevant_bits.__in6_u.__u6_addr32[i] = tmpaddr.sin6_addr.__in6_u.__u6_addr32[i] & in6_mask.at(i);
        }

        this->family = AF_INET6;
    }
    else
    {
        throw std::runtime_error("Network '" + network + "' is not a valid network notation.");
    }
}

bool Network::match(const sockaddr *addr) const
{
    const sa_family_t fam_arg = getFamilyFromSockAddr(addr);

    if (this->family != fam_arg)
        return false;

    if (this->family == AF_INET)
    {
        struct sockaddr_in tmp_this;
        struct sockaddr_in tmp_arg;

        std::memcpy(&tmp_this, this->data.data(), sizeof(tmp_this));
        std::memcpy(&tmp_arg, addr, sizeof(tmp_arg));

        return (tmp_this.sin_addr.s_addr & this->in_mask) == (tmp_arg.sin_addr.s_addr & this->in_mask);
    }
    else if (this->family == AF_INET6)
    {
        struct sockaddr_in6 tmp_arg;
        std::memcpy(&tmp_arg, addr, sizeof(tmp_arg));

        struct in6_addr arg_addr_relevant_bits;
        for (int i = 0; i < 4; i++)
        {
            arg_addr_relevant_bits.__in6_u.__u6_addr32[i] = tmp_arg.sin6_addr.__in6_u.__u6_addr32[i] & in6_mask.at(i);
        }

        uint8_t matches[4];
        for (int i = 0; i < 4; i++)
        {
            matches[i] = arg_addr_relevant_bits.__in6_u.__u6_addr32[i] == network_addr_relevant_bits.__in6_u.__u6_addr32[i];
        }

        return (matches[0] & matches[1] & matches[2] & matches[3]);
    }

    return false;
}


