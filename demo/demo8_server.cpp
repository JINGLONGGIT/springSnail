/*********************************************************************************
 * File Name: demo8_server.cpp
 * Description: 统一事件源：将信号事件和其他IO事件一样被处理
 * Author: jinglong
 * Date: 2020年4月28日 14:47
 * History: 
 *********************************************************************************/

#include "global.h"

#define     MAX_EVENT_NUMBER        1024
static int sig_pipefd[2];               // 用于处理信号传输的管道

/**********************************************************
 * 函数名称：setnonblocking
 * 函数功能：将文件描述符设置成非阻塞的
 * 输入参数：int fd        需要设置的文件描述符
 * 输出参数：无
 * 返 回 值：旧的文件描述符标志
 **********************************************************/
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/*************************************************************************
 * 函数名称：addfd
 * 函数功能：注册内核事件表
 * 输入参数：int epollfd: 内核事件表的文件描述符，由epoll_create()函数生成
 *           int fd: 要注册的文件描述符
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void addfd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/*************************************************************************
 * 函数名称：sig_handler
 * 函数功能：信号处理函数
 * 输入参数：int sig
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void sig_handler(int sig)
{
    // 保留原来的errno, 在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;

    // 将信号写入管道，以通知主循环
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

/*************************************************************************
 * 函数名称：addsig
 * 函数功能：设置信号的处理函数
 * 输入参数：int sig
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr;
    int connfd;

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 注册监听描述符事件
    addfd(epollfd, listenfd);

    // 创建双向管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);

    // 注册管道sig_pipefd[0]上的可读事件
    addfd(epollfd, sig_pipefd[0]);

    // 设置信号处理函数
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failed\n");
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
                // 注册连接描述符上的事件
                addfd(epollfd, connfd);
            }
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];

                ret = recv(sockfd, signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else 
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch(signals[i])
                        {
                            case SIGCHLD:
                            case SIGHUP:
                            {
                                continue;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else 
            {

            }
        }
    }

    close(listenfd);
    close(sig_pipefd[1]);
    close(sig_pipefd[0]);
    return 0;
}

