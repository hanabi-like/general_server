#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <netinet/in.h>
#include <sys/uio.h>
#include <unordered_map>
#include <string>

#include "locker.h"
#include "mysql_conn_pool.h"
#include "file_resource.h"
#include "http_request_dispatcher.h"
#include "http_request_parser.h"
#include "http_response.h"

class http_conn
{
public:
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        FORBIDDEN_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
        NO_RESOURCE,
        FILE_REQUEST
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    // Connection lifecycle
    void init(int sockfd, const sockaddr_in &addr, std::string user = "user", std::string pwd = "180427", std::string dbname = "general_server");
    void close_http_conn(bool real_close = true);
    void process();
    bool read();
    bool write();
    void init_mysql(mysql_conn_pool *connPool);

public:
    // Shared connection state
    static void setEpollFd(int epollFd);
    static int userCount();

public:
    // Threadpool-visible database handle
    MYSQL *conn;

private:
    void init();
    HTTP_CODE process_read();
    HTTP_CODE do_request();
    bool process_write(HTTP_CODE http_code);

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

    // Coordinated subsystems
    FileResource g_fileResource;
    HttpRequestDispatcher g_requestDispatcher;
    HttpRequestParser g_requestParser;
    HttpResponse g_response;

    // Shared auth/cache state
    static std::unordered_map<std::string, std::string> h_users;
    char mysql_user[100];
    char mysql_password[100];
    char mysql_dbname[100];
    locker g_lock;
};

#endif
