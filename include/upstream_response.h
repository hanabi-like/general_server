#ifndef UPSTREAM_RESPONSE_H
#define UPSTREAM_RESPONSE_H

#include <string>

struct UpstreamResponse
{
    std::string raw;

    void reset()
    {
        raw.clear();
    }
};

#endif
