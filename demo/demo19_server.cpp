/*****************************************************************
 * File Name: demo19_server.cpp
 * Description: 利用线程池实现简单的WEB服务器(采用同步IO模拟Proactor事件处理模式)
 * Author: jinglong
 * Date: 2020年5月4日 15:47
 * History: 
 *****************************************************************/

#include "http_conn.h"
#include "threadpool.h"
#include "locker.h"

#define         MAX_FD                      65536               // 最大连接的用户数量
#define         MAX_EVENT_NUMBER            10000               // epoll最多可以监听的事件数量

extern int addfd(int epollfd, int fd, bool one_shot);

void addsig(int sig, void(*sig_handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;

    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }

    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info)
{
    printf("%s\n", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char * argv [ ])
{
    // 忽略SIGPIPE信号
    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池
    threadpool<http_conn>* pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        return 1;
    }

    // 预先为每一个可能连接的客户分配一个http_conn对象
    http_conn* users = new http_conn[MAX_FD];
    assert(users);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    struct linger tmp = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);

    int ret = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 注册监听描述符事件
    addfd(epollfd, listenfd, false);

    http_conn::m_epollfd = epollfd;

    while (true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
                if (connfd < 0)
                {
                    printf("errno is %d\n", errno);
                    continue;
                }

                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    continue;
                }

                // 初始化客户连接
                users[connfd].init(connfd, clnt_addr);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 如果有异常，直接关闭客户端连接
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                // 根据读的结果，决定是将任务添加到线程池还是关闭连接
                if (users[sockfd].read())
                {
                    pool->append(users + sockfd);
                }
                else 
                {
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!users[sockfd].write())
                {
                    users[sockfd].close_conn();
                }
            }
            else 
            {

            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}

