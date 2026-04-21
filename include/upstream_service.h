#ifndef UPSTREAM_SERVICE_H
#define UPSTREAM_SERVICE_H

struct UpstreamService
{
    const char *name;
    const char *host;
    int port;

    UpstreamService(const char *serviceName = nullptr,
                    const char *serviceHost = nullptr,
                    int servicePort = 0)
        : name(serviceName), host(serviceHost), port(servicePort)
    {
    }
};

#endif
