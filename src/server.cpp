#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "fd_event.h"
#include "http_conn.h"
#include "mysql_conn_pool.h"
#include "thread_pool.h"

static volatile sig_atomic_t stop = 0;

static void handleStop(int)
{
    stop = 1;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handleStop);
    signal(SIGTERM, handleStop);
    signal(SIGPIPE, SIG_IGN);

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s ip port\n", basename(argv[0]));
        return 1;
    }

    const char *ip = argv[1];
    if (strcmp(ip, "localhost") == 0)
        ip = "127.0.0.1";

    char *end = nullptr;
    long parsedPort = strtol(argv[2], &end, 10);
    if (*argv[2] == '\0' || *end != '\0' || parsedPort <= 0 || parsedPort > 65535)
    {
        fprintf(stderr, "invalid port: %s\n", argv[2]);
        return 1;
    }
    int port = static_cast<int>(parsedPort);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &address.sin_addr) != 1)
    {
        fprintf(stderr, "invalid ip: %s\n", ip);
        return 1;
    }

    std::unique_ptr<HttpConn[]> conns;
    try
    {
        conns.reset(new HttpConn[MAX_FD_NUM]);
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "allocate HttpConn array failed: %s\n", e.what());
        return 1;
    }

    MysqlConnPool *connPool = MysqlConnPool::getInstance();
    if (!connPool->init())
    {
        fprintf(stderr, "mysql connection pool init failed\n");
        return 1;
    }

    HttpConn::initUserRepository(connPool);

    std::unique_ptr<ThreadPool<HttpConn>> pool;
    try
    {
        pool.reset(new ThreadPool<HttpConn>(connPool));
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "thread pool init failed: %s\n", e.what());
        return 1;
    }

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        perror("socket");
        return 1;
    }

    int option = 1;
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        close(listenFd);
        return 1;
    }

    if (bind(listenFd, (sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        close(listenFd);
        return 1;
    }

    if (listen(listenFd, SOMAXCONN) < 0)
    {
        perror("listen");
        close(listenFd);
        return 1;
    }

    epoll_event events[MAX_EVENT_NUM];

    int epollFd = epoll_create1(0);
    if (epollFd < 0)
    {
        perror("epoll_create1");
        close(listenFd);
        return 1;
    }
    fd_event::add(epollFd, listenFd, false);
    HttpConn::setEpollFd(epollFd);

    while (!stop)
    {
        int num = epoll_wait(epollFd, events, MAX_EVENT_NUM, -1);

        if (num < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < num; ++i)
        {
            int fd = events[i].data.fd;
            if (fd == listenFd)
            {
                while (true)
                {
                    sockaddr_in clientAddr;
                    socklen_t clientAddrLength = sizeof(clientAddr);
                    int connFd = accept(listenFd, (sockaddr *)&clientAddr, &clientAddrLength);
                    if (connFd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("accept");
                        break;
                    }
                    if (connFd >= MAX_FD_NUM)
                    {
                        close(connFd);
                        continue;
                    }
                    conns[connFd].init(connFd, clientAddr);
                }
            }
            else if (fd < 0 || fd >= MAX_FD_NUM)
                continue;
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                conns[fd].close();
            else if (events[i].events & EPOLLIN)
            {
                if (conns[fd].read())
                    pool->append(conns.get() + fd);
                else
                    conns[fd].close();
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!conns[fd].write())
                    conns[fd].close();
            }
        }
    }
    close(listenFd);
    close(epollFd);
    return 0;
}
