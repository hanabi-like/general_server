#ifndef HTTP_REQUEST_DISPATCHER_H
#define HTTP_REQUEST_DISPATCHER_H

#include <string>
#include <unordered_map>

#include "auth_request_handler.h"
#include "locker.h"
#include "mysql_conn_pool.h"
#include "page_request_handler.h"

class HttpRequestDispatcher
{
public:
    const char *resolve(
        const char *url,
        bool cgi,
        const char *content,
        MYSQL *conn,
        std::unordered_map<std::string, std::string> &users,
        locker &lock);

private:
    PageRequestHandler g_pageRequestHandler;
    AuthRequestHandler g_authRequestHandler;
};

#endif