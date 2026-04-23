#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <netinet/in.h>
#include <sys/uio.h>

#include "file_resource.h"
#include "http_request_dispatcher.h"
#include "http_request_parser.h"
#include "http_response.h"
#include "upstream_client.h"
#include "upstream_response.h"

class HttpConn
{
public:
    enum ProcessResult
    {
        NO_REQUEST,
        REQUEST_READY,
        FILE_READY,
        PROXY_READY,
        BAD_REQUEST,
        FORBIDDEN_REQUEST,
        NO_RESOURCE,
        INTERNAL_ERROR,
        BAD_GATEWAY
    };

    enum ResponseMode
    {
        RESPONSE_NONE,
        RESPONSE_LOCAL,
        RESPONSE_PROXY
    };

public:
    HttpConn() {}
    ~HttpConn() {}

public:
    // Connection lifecycle
    void init(int sockFd, const sockaddr_in &addr);
    void close();
    bool read();
    bool write();
    void process();

public:
    // Shared connection state
    static void setEpollFd(int epollFd);
    static int connectionCount();

private:
    void reset();

    // Request lifecycle
    ProcessResult parseRequest();
    ProcessResult handleRequest();
    bool buildResponse(ProcessResult processResult);

    // Send state
    void refreshIov();

private:
    static int g_epollFd;
    static int g_connectionCount;

private:
    // Connection state
    int g_sockFd;
    sockaddr_in g_address;

    // Send state
    struct iovec g_iov[2];
    int g_iovCount;
    size_t g_bytesHaveSent;
    size_t g_bytesToSend;
    ResponseMode g_responseMode;

    // Coordinated subsystems
    HttpRequestParser g_requestParser;
    HttpRequestDispatcher g_requestDispatcher;
    FileResource g_fileResource;
    HttpResponse g_response;
    UpstreamClient g_upstreamClient;
    UpstreamResponse g_upstreamResponse;
};

#endif
