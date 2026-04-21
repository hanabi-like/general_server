#ifndef UPSTREAM_CLIENT_H
#define UPSTREAM_CLIENT_H

#include <string>

#include "http_request_parser.h"
#include "proxy_request_target.h"
#include "upstream_response.h"

class UpstreamClient
{
public:
    bool forward(const ProxyRequestTarget &proxyRequestTarget, const HttpRequestParser &request, UpstreamResponse &response) const;

private:
    bool buildRequest(const ProxyRequestTarget &proxyRequestTarget, const HttpRequestParser &request, std::string &requestText) const;
};

#endif
