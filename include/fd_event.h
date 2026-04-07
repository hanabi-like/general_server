#ifndef FD_EVENT_H
#define FD_EVENT_H

namespace fd_event
{
int setNonBlocking(int fd);
void add(int epollFd, int fd, bool oneShoot);
void remove(int epollFd, int fd);
void mod(int epollFd, int fd, int event);
}

int setnonblocking(int fd);
void addfd(int epollfd, int fd, bool one_shot);
void removefd(int epollfd, int fd);
void modfd(int epollfd, int fd, int ev);

#endif