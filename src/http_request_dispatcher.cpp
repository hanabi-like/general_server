#include "http_request_dispatcher.h"

#include <cstring>

const char *HttpRequestDispatcher::resolve(
    const char *url,
    bool cgi,
    const char *content,
    MYSQL *conn,
    std::unordered_map<std::string, std::string> &users,
    locker &lock)
{
    const char *p = std::strrchr(url, '/');
    if (!p || *(p + 1) == '\0')
        return url;
    if (cgi && (*(p + 1) == '2' || *(p + 1) == '3'))
        return g_authRequestHandler.resolve(url, content, conn, users, lock);
    return g_pageRequestHandler.resolve(url);
}