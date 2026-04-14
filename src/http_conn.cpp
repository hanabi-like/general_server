#include "fd_event.h"
#include "http_conn.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/uio.h>

int HttpConn::g_userCount = 0;
int HttpConn::g_epollFd = -1;
UserRepository HttpConn::g_userRepository;

bool HttpConn::initUserRepository(MysqlConnPool *connPool)
{
    return g_userRepository.init(connPool);
}

void HttpConn::setEpollFd(int epollFd)
{
    g_epollFd = epollFd;
}

int HttpConn::userCount()
{
    return g_userCount;
}

void HttpConn::init(int sockFd, const sockaddr_in &addr, std::string user, std::string password, std::string dbName)
{
    g_sockFd = sockFd;
    g_address = addr;
    fd_event::add(g_epollFd, sockFd, true);
    ++g_userCount;

    reset();
}

void HttpConn::reset()
{
    g_requestParser.reset();

    g_fileResource.reset();

    g_response.init();
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

/*
非阻塞读
循环读取直至无数据可读或者对方关闭连接
*/
bool HttpConn::read()
{
    if (g_requestParser.readIndex() >= HttpRequestParser::READ_BUFFER_SIZE)
        return false;

    int ret = 0;
    while (true)
    {
        ret = recv(
            g_sockFd,
            g_requestParser.buffer() + g_requestParser.readIndex(),
            HttpRequestParser::READ_BUFFER_SIZE - g_requestParser.readIndex(),
            0);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (ret == 0)
            return false;
        g_requestParser.increaseReadIndex(ret);
    }
    return true;
}

HttpConn::ProcessResult HttpConn::process_read()
{
    HttpRequestParser::ParseResult parseResult = g_requestParser.process();

    switch (parseResult)
    {
    case HttpRequestParser::NO_REQUEST:
        return NO_REQUEST;
    case HttpRequestParser::BAD_REQUEST:
        return BAD_REQUEST;
    case HttpRequestParser::REQUEST_READY:
        return REQUEST_READY;
    default:
        return INTERNAL_ERROR;
    }
}

HttpConn::ProcessResult HttpConn::do_request(MYSQL *conn)
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

bool HttpConn::process_write(ProcessResult processResult)
{
    switch (processResult)
    {
    case INTERNAL_ERROR:
    {
        if (!g_response.buildInternalError(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        if (!g_response.buildBadRequest(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case NO_RESOURCE:
    {
        if (!g_response.buildNotFound(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        if (!g_response.buildForbidden(g_requestParser.keepAlive()))
            return false;
        break;
    }
    case FILE_READY:
    {
        if (!g_response.buildOkHeader(g_fileResource.size(), g_requestParser.keepAlive()))
            return false;
        if (g_fileResource.size() != 0)
        {
            g_iov[0].iov_base = g_response.buffer();
            g_iov[0].iov_len = g_response.bufferSize();
            g_iov[1].iov_base = g_fileResource.data();
            g_iov[1].iov_len = g_fileResource.size();
            g_iovCount = 2;
            return true;
        }
    }
    default:
        return false;
    }
    g_iov[0].iov_base = g_response.buffer();
    g_iov[0].iov_len = g_response.bufferSize();
    g_iovCount = 1;
    return true;
}

bool HttpConn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int iovec_ptr = 0;
    int bytes_to_send = g_response.bufferSize() + g_fileResource.size();
    if (bytes_to_send == 0)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        reset();
        return true;
    }
    while (1)
    {
        temp = writev(g_sockFd, g_iov, g_iovCount);
        if (temp > 0)
        {
            bytes_have_send += temp;
            iovec_ptr = bytes_have_send - g_response.bufferSize();
        }
        else if (temp <= -1)
        {
            if (errno == EAGAIN) // 缓冲区已满
            {
                if (bytes_have_send >= g_iov[0].iov_len)
                {
                    g_iov[0].iov_len = 0;
                    g_iov[1].iov_base = g_fileResource.data() + iovec_ptr;
                    g_iov[1].iov_len = bytes_to_send;
                }
                else
                {
                    g_iov[0].iov_base = g_response.buffer() + bytes_have_send;
                    g_iov[0].iov_len -= bytes_have_send;
                }
                fd_event::mod(g_epollFd, g_sockFd, EPOLLOUT);
                return true;
            }
            g_fileResource.reset();
            return false;
        }
        bytes_to_send -= temp;
        if (bytes_to_send <= 0)
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
    ProcessResult read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        return;
    }
    if (read_ret == REQUEST_READY)
        read_ret = do_request(conn);
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close();
        return;
    }
    fd_event::mod(g_epollFd, g_sockFd, EPOLLOUT);
}
