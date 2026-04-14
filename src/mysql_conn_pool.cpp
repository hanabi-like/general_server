#include "mysql_conn_pool.h"

#include <cstdlib>

MysqlConnPool *MysqlConnPool::getInstance()
{
    static MysqlConnPool connPool;
    return &connPool;
}

void MysqlConnPool::destroy()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    for (auto conn : g_connList)
        mysql_close(conn);

    g_curConn = 0;
    g_freeConn = 0;
    g_connList.clear();
}

MysqlConnPool::MysqlConnPool()
    : g_maxConn(0), g_curConn(0), g_freeConn(0)
{
}

MysqlConnPool::~MysqlConnPool()
{
    destroy();
}

void MysqlConnPool::init(std::string url, int port, std::string user, std::string password, std::string dbName, int maxConn)
{
    g_url = url;
    g_port = port;
    g_user = user;
    g_password = password;
    g_dbName = dbName;

    for (int i = 0; i < maxConn; ++i)
    {
        MYSQL *conn = nullptr;
        conn = mysql_init(conn);

        if (conn == nullptr)
            exit(1);
        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), dbName.c_str(), port, nullptr, 0);
        if (conn == nullptr)
            exit(1);
        g_connList.push_back(conn);
        ++g_freeConn;
    }

    g_maxConn = maxConn;
}

MYSQL *MysqlConnPool::getConn()
{
    std::unique_lock<std::mutex> lock(g_mutex);

    g_condition.wait(lock, [this]
                     { return !g_connList.empty(); });

    MYSQL *conn = g_connList.front();
    g_connList.pop_front();

    --g_freeConn;
    ++g_curConn;

    return conn;
}

bool MysqlConnPool::releaseConn(MYSQL *conn)
{
    if (!conn)
        return false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_connList.push_back(conn);
        ++g_freeConn;
        --g_curConn;
    }

    g_condition.notify_one();
    return true;
}

MysqlConnGuard::MysqlConnGuard(MYSQL **conn, MysqlConnPool *connPool)
{
    *conn = connPool->getConn();
    g_curConn = *conn;
    g_connPool = connPool;
}

MysqlConnGuard::~MysqlConnGuard()
{
    g_connPool->releaseConn(g_curConn);
}
