#ifndef RESOLVED_ROUTE_H
#define RESOLVED_ROUTE_H

#include "proxy_request_target.h"

struct ResolvedRoute
{
    enum Type
    {
        LOCAL,
        PROXY,
    };

    Type type;
    const char *localPath;
    ProxyRequestTarget proxyRequestTarget;

    ResolvedRoute(Type routeType = LOCAL,
                  const char *routeLocalPath = nullptr,
                  const ProxyRequestTarget &routeProxyRequestTarget = ProxyRequestTarget())
        : type(routeType), localPath(routeLocalPath), proxyRequestTarget(routeProxyRequestTarget)
    {
    }
};

#endif
