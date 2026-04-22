#ifndef PROXY_ROUTE_RESOLVER_H
#define PROXY_ROUTE_RESOLVER_H

#include "proxy_request_target.h"

class ProxyRouteResolver
{
public:
    bool resolve(const char *url, const char *query, ProxyRequestTarget &target) const;
};

#endif
