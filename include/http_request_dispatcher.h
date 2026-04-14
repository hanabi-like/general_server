#ifndef HTTP_REQUEST_DISPATCHER_H
#define HTTP_REQUEST_DISPATCHER_H

#include <string>

#include "auth_request_handler.h"
#include "mysql_conn_pool.h"
#include "page_request_handler.h"
#include "user_repository.h"

class HttpRequestDispatcher
{
public:
    const char *resolve(
        const char *url,
        bool cgi,
        const char *content,
        MYSQL *conn,
        UserRepository &userRepository);

private:
    PageRequestHandler g_pageRequestHandler;
    AuthRequestHandler g_authRequestHandler;
};

#endif
