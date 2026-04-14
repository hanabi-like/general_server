#ifndef AUTH_REQUEST_HANDLER_H
#define AUTH_REQUEST_HANDLER_H

#include <string>

#include "mysql_conn_pool.h"
#include "user_repository.h"

class AuthRequestHandler
{
public:
    static const int USER_INFO_LEN = 100;

    const char *resolve(
        const char *url,
        const char *content,
        MYSQL *conn,
        UserRepository &userRepository);

private:
    void parseUserInfo(const char *content, std::string &username, std::string &password) const;
};

#endif
