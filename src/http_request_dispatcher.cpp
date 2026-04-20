#include "http_request_dispatcher.h"

#include <cstring>

namespace
{
    bool startWith(const char *value, const char *prefix)
    {
        if (value == nullptr || prefix == nullptr)
            return false;
        return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
    }
}

RouteResult HttpRequestDispatcher::resolve(const char *url)
{
    if (startWith(url, "/api/auth/"))
        return RouteResult(RouteResult::PROXY, url);

    return RouteResult(RouteResult::LOCAL, g_localRouteResolver.resolve(url));
}
