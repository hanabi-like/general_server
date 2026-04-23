#include "fd_event.h"
#include "http_conn.h"

#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>

int HttpConn::g_epollFd = -1;
int HttpConn::g_connectionCount = 0;

void HttpConn::setEpollFd(int epollFd)
{
    g_epollFd = epollFd;
}

int HttpConn::connectionCount()
{
    return g_connectionCount;
}

void HttpConn::init(int sockFd, const sockaddr_in &addr)
{
    g_sockFd = sockFd;
    g_address = addr;
    fd_event::add(g_epollFd, sockFd, true);
    ++g_connectionCount;

    reset();
}

void HttpConn::close()
{
    if (g_sockFd != -1)
    {
        fd_event::remove(g_epollFd, g_sockFd);
        g_sockFd = -1;
        --g_connectionCount;
    }
}

bool HttpConn::read()
{
    if (g_requestParser.readIndex() >= HttpRequestParser::READ_BUFFER_SIZE)
        return false;

    ssize_t ret = 0;

    while (true)
    {
        ret = recv(
            g_sockFd,
            g_requestParser.buffer() + g_requestParser.readIndex(),
            HttpRequestParser::READ_BUFFER_SIZE - g_requestParser.readIndex(),
            0);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (ret == 0)
            return false;
        g_requestParser.increaseReadIndex(ret);
    }
    return true;
}

bool HttpConn::write()
{
    if (g_bytesToSend == 0)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        reset();
        return true;
    }
    ssize_t temp = 0;
    while (true)
    {
        temp = writev(g_sockFd, g_iov, g_iovCount);
        if (temp > 0)
        {
            g_bytesHaveSent += temp;
            g_bytesToSend -= temp;

            if (g_bytesToSend > 0)
                refreshIov();
        }
        else if (temp < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) // 缓冲区满
            {
                refreshIov();
                fd_event::mod(g_epollFd, g_sockFd, EPOLLOUT);
                return true;
            }
            g_fileResource.reset();
            return false;
        }

        if (g_bytesToSend == 0)
        {
            g_fileResource.reset();
            fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);

            if (g_responseMode == RESPONSE_PROXY)
                return false;

            if (g_requestParser.keepAlive())
            {
                reset();
                return true;
            }
            else
                return false;
        }
    }
}

void HttpConn::process()
{
    ProcessResult ret = parseRequest();
    if (ret == NO_REQUEST)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        return;
    }
    else if (ret == REQUEST_READY)
        ret = handleRequest();
    bool responseReady = buildResponse(ret);
    if (!responseReady)
    {
        close();
        return;
    }
    fd_event::mod(g_epollFd, g_sockFd, EPOLLOUT);
}

void HttpConn::reset()
{
    g_requestParser.reset();
    g_fileResource.reset();
    g_response.init();
    g_upstreamResponse.reset();

    std::memset(g_iov, 0, sizeof(g_iov));
    g_iovCount = 0;
    g_bytesHaveSent = 0;
    g_bytesToSend = 0;
    g_responseMode = RESPONSE_NONE;
}

HttpConn::ProcessResult HttpConn::parseRequest()
{
    HttpRequestParser::ParseResult parseResult = g_requestParser.process();

    switch (parseResult)
    {
    case HttpRequestParser::NO_REQUEST:
        return NO_REQUEST;
    case HttpRequestParser::REQUEST_READY:
        return REQUEST_READY;
    case HttpRequestParser::BAD_REQUEST:
        return BAD_REQUEST;
    default:
        return INTERNAL_ERROR;
    }
}

HttpConn::ProcessResult HttpConn::handleRequest()
{
    ResolvedRoute route = g_requestDispatcher.resolve(g_requestParser.url(), g_requestParser.query());

    switch (route.type)
    {
    case ResolvedRoute::PROXY:
    {
        if (g_upstreamClient.forward(route.proxyRequestTarget, g_requestParser, g_upstreamResponse))
            return PROXY_READY;
        return BAD_GATEWAY;
    }
    case ResolvedRoute::LOCAL:
    {
        FileResource::Result result = g_fileResource.load(route.localPath);
        switch (result)
        {
        case FileResource::OK:
            return FILE_READY;
        case FileResource::BAD_REQUEST:
            return BAD_REQUEST;
        case FileResource::FORBIDDEN:
            return FORBIDDEN_REQUEST;
        case FileResource::NOT_FOUND:
            return NO_RESOURCE;
        default:
            return INTERNAL_ERROR;
        }
    }
    default:
        return INTERNAL_ERROR;
    }
}

bool HttpConn::buildResponse(ProcessResult processResult)
{
    switch (processResult)
    {
    case BAD_REQUEST:
    {
        if (!g_response.buildBadRequest(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        if (!g_response.buildForbidden(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case NO_RESOURCE:
    {
        if (!g_response.buildNotFound(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case INTERNAL_ERROR:
    {
        if (!g_response.buildInternalError(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case BAD_GATEWAY:
    {
        if (!g_response.buildBadGateway(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case FILE_READY:
    {
        if (!g_response.buildOkHeader(g_fileResource.size(), g_requestParser.keepAlive()))
            return false;

        g_iov[0].iov_base = g_response.buffer();
        g_iov[0].iov_len = g_response.bufferSize();
        g_bytesHaveSent = 0;

        if (g_fileResource.size() != 0)
        {
            g_iov[1].iov_base = g_fileResource.data();
            g_iov[1].iov_len = g_fileResource.size();
            g_iovCount = 2;
            g_bytesToSend = g_response.bufferSize() + g_fileResource.size();
        }
        else
        {
            g_iovCount = 1;
            g_bytesToSend = g_response.bufferSize();
        }
        g_responseMode = RESPONSE_LOCAL;
        return true;
    }
    case PROXY_READY:
    {
        if (g_upstreamResponse.raw.empty())
            return false;

        g_iov[0].iov_base = const_cast<char *>(g_upstreamResponse.raw.data());
        g_iov[0].iov_len = g_upstreamResponse.raw.size();
        g_iovCount = 1;

        g_bytesHaveSent = 0;
        g_bytesToSend = g_upstreamResponse.raw.size();
        g_responseMode = RESPONSE_PROXY;

        return true;
    }
    default:
        return false;
    }
    g_iov[0].iov_base = g_response.buffer();
    g_iov[0].iov_len = g_response.bufferSize();
    g_iovCount = 1;

    g_bytesHaveSent = 0;
    g_bytesToSend = g_response.bufferSize();
    g_responseMode = RESPONSE_LOCAL;

    return true;
}

void HttpConn::refreshIov()
{
    std::memset(g_iov, 0, sizeof(g_iov));
    g_iovCount = 0;

    if (g_bytesToSend == 0)
        return;

    if (g_responseMode == RESPONSE_PROXY)
    {
        g_iov[0].iov_base = const_cast<char *>(g_upstreamResponse.raw.data()) + g_bytesHaveSent;
        g_iov[0].iov_len = g_bytesToSend;
        g_iovCount = 1;
        return;
    }

    size_t headerSize = g_response.bufferSize();
    size_t fileSize = static_cast<size_t>(g_fileResource.size());

    if (g_bytesHaveSent < headerSize)
    {
        g_iov[0].iov_base = g_response.buffer() + g_bytesHaveSent;
        g_iov[0].iov_len = headerSize - g_bytesHaveSent;
        g_iovCount = 1;

        if (fileSize > 0)
        {
            g_iov[1].iov_base = g_fileResource.data();
            g_iov[1].iov_len = fileSize;
            g_iovCount = 2;
        }

        return;
    }

    size_t fileOffset = g_bytesHaveSent - headerSize;

    if (fileOffset < fileSize)
    {
        g_iov[0].iov_base = g_fileResource.data() + fileOffset;
        g_iov[0].iov_len = fileSize - fileOffset;
        g_iovCount = 1;
    }
}
