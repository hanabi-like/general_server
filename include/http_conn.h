#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <netinet/in.h>
#include <sys/uio.h>
#include <unordered_map>
#include <string>

#include "file_resource.h"
#include "http_request_dispatcher.h"
#include "http_request_parser.h"
#include "http_response.h"

class HttpConn
{
public:
    enum ProcessResult
    {
        NO_REQUEST,
        REQUEST_READY,
        FILE_READY,
        BAD_REQUEST,
        FORBIDDEN_REQUEST,
        NO_RESOURCE,
        INTERNAL_ERROR
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
    static int userCount();

private:
    void reset();
    ProcessResult parseRequest();
    ProcessResult handleRequest();
    bool buildResponse(ProcessResult processResult);

private:
    static int g_epollFd;
    static int g_userCount;

private:
    // Connection state
    int g_sockFd;
    sockaddr_in g_address;

    // Send state
    struct iovec g_iov[2];
    int g_iovCount;
    size_t g_bytesHaveSent;
    size_t g_bytesToSend;

    // Coordinated subsystems
    HttpRequestParser g_requestParser;
    HttpRequestDispatcher g_requestDispatcher;
    FileResource g_fileResource;
    HttpResponse g_response;
};

#endif
