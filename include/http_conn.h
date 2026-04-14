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
#include "user_repository.h"

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
    void init(int sockFd, const sockaddr_in &addr, std::string user = "user", std::string password = "180427", std::string dbName = "general_server");
    void close();
    bool read();
    bool write();
    void process();

public:
    // Shared connection state
    static bool initUserRepository(mysql_conn_pool *connPool);
    static void setEpollFd(int epollFd);
    static int userCount();

public:
    // Threadpool-visible database handle
    MYSQL *conn;

private:
    void reset();
    ProcessResult process_read();
    ProcessResult do_request();
    bool process_write(ProcessResult processResult);

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
    static UserRepository g_userRepository;
};

#endif
