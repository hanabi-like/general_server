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

void http_conn::init_mysql(mysql_conn_pool *connPool)
{
    MYSQL *conn = NULL;
    connRAII mysqlConn(&conn, connPool);

    if (mysql_query(conn, "SELECT username,password FROM user"))
        cout << "SELECT error" << endl;

    MYSQL_RES *res = mysql_store_result(conn);

    int num_fields = mysql_num_fields(res);

    MYSQL_FIELD *fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        string t1(row[0]);
        string t2(row[1]);
        h_users[t1] = t2;
    }
}

int http_conn::g_userCount = 0;
int http_conn::g_epollFd = -1;
unordered_map<string, string> http_conn::h_users;

void http_conn::setEpollFd(int epollFd)
{
    g_epollFd = epollFd;
}

int http_conn::userCount()
{
    return g_userCount;
}

void http_conn::init(int sockfd, const sockaddr_in &addr, string user, string pwd, string dbname)
{
    g_sockFd = sockfd;
    g_address = addr;
    fd_event::add(g_epollFd, sockfd, true);
    ++g_userCount;

    strcpy(mysql_user, user.c_str());
    strcpy(mysql_password, pwd.c_str());
    strcpy(mysql_dbname, dbname.c_str());

    init();
}

void http_conn::init()
{
    conn = NULL;
    g_requestParser.reset();

    g_fileResource.reset();

    g_response.init();
}

void http_conn::close_http_conn(bool real_close)
{
    if (real_close && g_sockFd != -1)
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
bool http_conn::read()
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

http_conn::HTTP_CODE http_conn::process_read()
{
    HttpRequestParser::HttpCode httpCode = g_requestParser.process();

    switch (httpCode)
    {
    case HttpRequestParser::NO_REQUEST:
        return NO_REQUEST;
    case HttpRequestParser::BAD_REQUEST:
        return BAD_REQUEST;
    case HttpRequestParser::GET_REQUEST:
        return do_request();
    default:
        return INTERNAL_ERROR;
    }
}

http_conn::HTTP_CODE http_conn::do_request()
{
    const char *targetUrl = g_requestDispatcher.resolve(
        g_requestParser.url(),
        g_requestParser.cgi(),
        g_requestParser.content(),
        conn,
        h_users,
        g_lock);

    FileResource::Result result = g_fileResource.load(targetUrl);
    switch (result)
    {
    case FileResource::OK:
        return FILE_REQUEST;
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

bool http_conn::process_write(HTTP_CODE http_code)
{
    switch (http_code)
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
    case FILE_REQUEST:
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

bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int iovec_ptr = 0;
    int bytes_to_send = g_response.bufferSize() + g_fileResource.size();
    if (bytes_to_send == 0)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        init();
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
                init();
                return true;
            }
            else
                return false;
        }
    }
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        fd_event::mod(g_epollFd, g_sockFd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
        close_http_conn();
    fd_event::mod(g_epollFd, g_sockFd, EPOLLOUT);
}
