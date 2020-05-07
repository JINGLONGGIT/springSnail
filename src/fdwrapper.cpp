/*********************************************************************************
 * File Name: fdwrapper.cpp
 * Description: 文件描述符操作模块
 * Author: jinglong
 * Date: 2020年5月7日 09:18
 * History: 
 *********************************************************************************/

#include "global.h"
#include "conn.h"
#include "fdwrapper.h"

// 将文件描述符设置为非阻塞的
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 注册文件描述符fd的可读事件EPOLLIN
void add_read_fd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 注册文件描述符fd上的可写事件EPOLLOUT
void add_write_fd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 删除文件描述符fd上注册的所有事件，并关闭文件描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 删除文件描述符fd上注册的所有事件
void closefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
}

// 给文件描述符新增ev事件
void modfd(int epollfd, int fd, int ev)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

