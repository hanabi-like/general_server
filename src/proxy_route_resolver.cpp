#include "proxy_route_resolver.h"

#include <cstring>

namespace
{
    const UpstreamService AUTH_SERVICE("auth-service", "127.0.0.1", 8081);

    bool startWith(const char *value, const char *prefix)
    {
        if (value == nullptr || prefix == nullptr)
            return false;
        return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
    }
}

bool ProxyRouteResolver::resolve(const char *url, const char *query, ProxyRequestTarget &target) const
{
    if (startWith(url, "/api/auth/"))
    {
        target = ProxyRequestTarget(AUTH_SERVICE, url, query);
        return true;
    }

    return false;
}
