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

int setnonblocking(int fd)
{
    return fd_event::setNonBlocking(fd);
}

void addfd(int epollfd, int fd, bool one_shot)
{
    fd_event::add(epollfd, fd, one_shot);
}

void removefd(int epollfd, int fd)
{
    fd_event::remove(epollfd, fd);
}

void modfd(int epollfd, int fd, int ev)
{
    fd_event::mod(epollfd, fd, ev);
}