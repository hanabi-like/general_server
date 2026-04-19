#ifndef MYSQL_CONN_POOL_H
#define MYSQL_CONN_POOL_H

#include <list>
#include <mysql/mysql.h>
#include <string>
#include <mutex>
#include <condition_variable>

class MysqlConnPool
{
private:
    MysqlConnPool();
    ~MysqlConnPool();
    void destroy();

    int g_maxConn;
    int g_curConn;
    int g_freeConn;
    std::mutex g_mutex;
    std::condition_variable g_condition;
    std::list<MYSQL *> g_connList;
    std::string g_url;
    int g_port;
    std::string g_user;
    std::string g_password;
    std::string g_dbName;

public:
    MYSQL *getConn();
    bool releaseConn(MYSQL *conn);

    static MysqlConnPool *getInstance();
    bool init(const std::string &url = "localhost", int port = 3306, const std::string &user = "user", const std::string &password = "180427", const std::string &dbName = "general_server", int maxConn = 8);
};

class MysqlConnGuard
{
private:
    MYSQL *g_curConn = nullptr;
    MysqlConnPool *g_connPool = nullptr;

public:
    MysqlConnGuard(MYSQL *&conn, MysqlConnPool *connPool);
    ~MysqlConnGuard();
};

#endif
