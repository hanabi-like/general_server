#ifndef PROXY_REQUEST_TARGET_H
#define PROXY_REQUEST_TARGET_H

#include "upstream_service.h"

struct ProxyRequestTarget
{
    UpstreamService service;
    const char *path;
    const char *query;

    ProxyRequestTarget(const UpstreamService &targetService = UpstreamService(),
                       const char *targetPath = nullptr,
                       const char *targetQuery = nullptr)
        : service(targetService), path(targetPath), query(targetQuery)
    {
    }
};

#endif
