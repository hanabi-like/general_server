#include "mysql_conn_pool.h"

#include <cstdio>

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

    g_maxConn = 0;
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

bool MysqlConnPool::init(const std::string &url, int port, const std::string &user, const std::string &password, const std::string &dbName, int maxConn)
{
    if (maxConn <= 0)
    {
        fprintf(stderr, "invalid mysql max connection count: %d\n", maxConn);
        return false;
    }
    g_url = url;
    g_port = port;
    g_user = user;
    g_password = password;
    g_dbName = dbName;

    for (int i = 0; i < maxConn; ++i)
    {
        MYSQL *conn = mysql_init(nullptr);
        if (conn == nullptr)
        {
            destroy();
            fprintf(stderr, "mysql_init failed\n");
            return false;
        }

        if (mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), dbName.c_str(), port, nullptr, 0) == nullptr)
        {
            mysql_close(conn);
            destroy();
            fprintf(stderr, "mysql_real_connect failed\n");
            return false;
        }

        g_connList.push_back(conn);
        ++g_freeConn;
    }

    g_maxConn = maxConn;

    return true;
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

MysqlConnGuard::MysqlConnGuard(MYSQL *&conn, MysqlConnPool *connPool) : g_curConn(nullptr), g_connPool(nullptr)
{
    if (connPool == nullptr)
        return;
    conn = connPool->getConn();
    g_curConn = conn;
    g_connPool = connPool;
}

MysqlConnGuard::~MysqlConnGuard()
{
    if (g_curConn != nullptr && g_connPool != nullptr)
        g_connPool->releaseConn(g_curConn);
}
