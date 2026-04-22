#ifndef PROXY_REQUEST_TARGET_H
#define PROXY_REQUEST_TARGET_H

#include <string>

#include "upstream_service.h"

struct ProxyRequestTarget
{
    UpstreamService service;
    std::string path;
    const char *query;

    ProxyRequestTarget(const UpstreamService &targetService = UpstreamService(),
                       const std::string &targetPath = std::string(),
                       const char *targetQuery = nullptr)
        : service(targetService),
          path(targetPath),
          query(targetQuery)
    {
    }
};

#endif
