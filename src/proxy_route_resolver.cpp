#include "proxy_route_resolver.h"

#include <cstring>
#include <string>

namespace
{
    struct ProxyRoute
    {
        const char *externalPrefix;
        const char *upstreamPrefix;
        UpstreamService service;
    };

    const UpstreamService GAME_INFORMATION_MANAGER_SERVICE(
        "game-information-manager",
        "127.0.0.1",
        8081);

    const ProxyRoute PROXY_ROUTES[] = {
        {"/api/game-information-manager/auth/", "/api/auth/", GAME_INFORMATION_MANAGER_SERVICE},
        {"/api/game-information-manager/game/", "/api/game/", GAME_INFORMATION_MANAGER_SERVICE}};

    bool startWith(const char *value, const char *prefix)
    {
        if (value == nullptr || prefix == nullptr)
            return false;
        return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
    }

    std::string rewritePath(const char *url, const char *externalPrefix, const char *upstreamPrefix)
    {
        const char *suffix = url + std::strlen(externalPrefix);
        std::string path(upstreamPrefix);
        path += suffix;
        return path;
    }
}

bool ProxyRouteResolver::resolve(const char *url, const char *query, ProxyRequestTarget &target) const
{
    for (size_t i = 0; i < sizeof(PROXY_ROUTES) / sizeof(PROXY_ROUTES[0]); ++i)
    {
        const ProxyRoute &route = PROXY_ROUTES[i];

        if (startWith(url, route.externalPrefix))
        {
            target = ProxyRequestTarget(
                route.service,
                rewritePath(url, route.externalPrefix, route.upstreamPrefix),
                query);
            return true;
        }
    }

    return false;
}
