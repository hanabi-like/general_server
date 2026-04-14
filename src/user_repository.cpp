#include "user_repository.h"

bool UserRepository::init(mysql_conn_pool *connPool)
{
    MYSQL *conn = nullptr;
    connRAII curConn(&conn, connPool);

    if (mysql_query(conn, "SELECT username,password FROM user"))
        return false;

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return false;

    std::lock_guard<std::mutex> lock(g_mutex);

    while (MYSQL_ROW row = mysql_fetch_row(res))
        g_user[row[0]] = row[1];

    mysql_free_result(res);

    return true;
}

bool UserRepository::exist(const std::string &username) const
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_user.find(username) != g_user.end();
}

bool UserRepository::create(const std::string &username, const std::string &password, MYSQL *conn)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_user.find(username) != g_user.end())
        return false;

    std::string mysqlInsertQuery = "INSERT INTO user(username, password) VALUES('" + username + "', '" + password + "')";

    int ret = mysql_query(conn, mysqlInsertQuery.c_str());
    if (!ret)
        g_user[username] = password;

    return ret == 0;
}

bool UserRepository::verify(const std::string &username, const std::string &password) const
{
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_user.find(username);
    return it != g_user.end() && it->second == password;
}