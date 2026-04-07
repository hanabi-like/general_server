#include "fd_event.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace fd_event
{
int setNonBlocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void add(int epollFd, int fd, bool oneShoot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (oneShoot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

void remove(int epollFd, int fd)
{
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void mod(int epollFd, int fd, int e)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = e | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
}
}
