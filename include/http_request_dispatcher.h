#ifndef HTTP_REQUEST_DISPATCHER_H
#define HTTP_REQUEST_DISPATCHER_H

#include "local_route_resolver.h"
#include "route_result.h"

class HttpRequestDispatcher
{
public:
    RouteResult resolve(const char *url);

private:
    LocalRouteResolver g_localRouteResolver;
};

#endif
