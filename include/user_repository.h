#ifndef USER_REPOSITORY_H
#define USER_REPOSITORY_H

#include <string>
#include <unordered_map>
#include <mutex>

#include "mysql_conn_pool.h"

class UserRepository
{
public:
    bool init(mysql_conn_pool *connPool);
    bool exist(const std::string &username) const;
    bool create(const std::string &username, const std::string &password, MYSQL *conn);
    bool verify(const std::string &username, const std::string &password) const;

private:
    std::unordered_map<std::string, std::string> g_user;
    mutable std::mutex g_mutex;
};

#endif