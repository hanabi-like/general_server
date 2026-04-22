#include "http_request_dispatcher.h"

ResolvedRoute HttpRequestDispatcher::resolve(const char *url, const char *query)
{
    ProxyRequestTarget proxyRequestTarget;
    if (g_proxyRouteResolver.resolve(url, query, proxyRequestTarget))
        return ResolvedRoute(ResolvedRoute::PROXY, nullptr, proxyRequestTarget);

    return ResolvedRoute(ResolvedRoute::LOCAL, g_localRouteResolver.resolve(url));
}
