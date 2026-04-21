#ifndef HTTP_REQUEST_DISPATCHER_H
#define HTTP_REQUEST_DISPATCHER_H

#include "local_route_resolver.h"
#include "proxy_route_resolver.h"
#include "resolved_route.h"

class HttpRequestDispatcher
{
public:
    ResolvedRoute resolve(const char *url, const char *query);

private:
    LocalRouteResolver g_localRouteResolver;
    ProxyRouteResolver g_proxyRouteResolver;
};

#endif
