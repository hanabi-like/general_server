#ifndef AUTH_REQUEST_HANDLER_H
#define AUTH_REQUEST_HANDLER_H

#include <string>
#include <unordered_map>

#include "locker.h"
#include "mysql_conn_pool.h"

class AuthRequestHandler
{
public:
    static const int USER_INFO_LEN = 100;

    const char *resolve(
        const char *url,
        const char *content,
        MYSQL *conn,
        std::unordered_map<std::string, std::string> &users,
        locker &lock);

private:
    void parseUserInfo(const char *content, std::string &username, std::string &password) const;
};

#endif