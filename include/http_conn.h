#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <netinet/in.h>
#include <sys/uio.h>
#include <unordered_map>
#include <string>

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
    void process(MYSQL *conn);

public:
    // Shared connection state
    static void setEpollFd(int epollFd);
    static bool initUserRepository(MysqlConnPool *connPool);
    static int userCount();

private:
    void reset();
    ProcessResult process_read();
    ProcessResult do_request(MYSQL *conn);
    bool process_write(ProcessResult processResult);

private:
    static int g_epollFd;
    static UserRepository g_userRepository;
    static int g_userCount;

private:
    // Connection state
    int g_sockFd;
    sockaddr_in g_address;

    // Send state
    struct iovec g_iov[2];
    int g_iovCount;

    // Coordinated subsystems
    HttpRequestParser g_requestParser;
    HttpRequestDispatcher g_requestDispatcher;
    FileResource g_fileResource;
    HttpResponse g_response;
};

#endif
