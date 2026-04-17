#include "fd_event.h"
#include "http_conn.h"

#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>

int HttpConn::g_epollFd = -1;
UserRepository HttpConn::g_userRepository;
int HttpConn::g_userCount = 0;

void HttpConn::setEpollFd(int epollFd)
{
    g_epollFd = epollFd;
}

bool HttpConn::initUserRepository(MysqlConnPool *connPool)
{
    return g_userRepository.init(connPool);
}

int HttpConn::userCount()
{
    return g_userCount;
}

void HttpConn::init(int sockFd, const sockaddr_in &addr)
{
    g_sockFd = sockFd;
    g_address = addr;
    fd_event::add(g_epollFd, sockFd, true);
    ++g_userCount;

    reset();
}

void HttpConn::close()
{
    if (g_sockFd != -1)
    {
        fd_event::remove(g_epollFd, g_sockFd);
        g_sockFd = -1;
        --g_userCount;
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
        }
        else if (temp < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) // 缓冲区满
            {
                if (g_bytesHaveSent >= g_response.bufferSize())
                {
                    size_t offset = g_bytesHaveSent - g_response.bufferSize();
                    g_iov[0].iov_len = 0;
                    g_iov[1].iov_base = g_fileResource.data() + offset;
                    g_iov[1].iov_len = g_bytesToSend;
                }
                else
                {
                    g_iov[0].iov_base = g_response.buffer() + g_bytesHaveSent;
                    g_iov[0].iov_len = g_response.bufferSize() - g_bytesHaveSent;
                }
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

void HttpConn::process(MYSQL *conn)
{
    ProcessResult ret = parseRequest();
    if (ret == NO_REQUEST)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        return;
    }
    else if (ret == REQUEST_READY)
        ret = handleRequest(conn);
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

    std::memset(g_iov, 0, sizeof(g_iov));
    g_iovCount = 0;
    g_bytesHaveSent = 0;
    g_bytesToSend = 0;
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

HttpConn::ProcessResult HttpConn::handleRequest(MYSQL *conn)
{
    const char *targetUrl = g_requestDispatcher.resolve(
        g_requestParser.url(),
        g_requestParser.cgi(),
        g_requestParser.content(),
        conn,
        g_userRepository);

    FileResource::Result result = g_fileResource.load(targetUrl);
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

    return true;
}
