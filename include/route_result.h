#ifndef ROUTE_RESULT_H
#define ROUTE_RESULT_H

struct RouteResult
{
    enum Type
    {
        LOCAL,
        PROXY,
    };

    Type type;
    const char *target;

    RouteResult(Type routeType = LOCAL, const char *routeTarget = nullptr) : type(routeType), target(routeTarget) {}
};

#endif
