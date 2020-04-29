/*********************************************************************************
 * File Name: demo11_server.cpp
 * Description: 基于升序链表的定时器处理非活动连接(模拟socket选项KEEPALIVE)
 * Author: jinglong
 * Date: 2020年4月29日 09:29
 * History: 
 *********************************************************************************/

#include "demo11_server.h"

#define         FD_LIMIT                65535
#define         MAX_EVENT_NUMBER        1024
#define         TIMESLOT                5

static int pipefd[2];
static int epollfd = 0;
static sort_timer_lst timer_lst;

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
 *           int fd:      要注册的文件描述符
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

    // 将信号写入管道，以通知主线程
    send(pipefd[1], (char *)&msg, 1, 0);
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

// 定时器处理函数
void timer_handler()
{
    timer_lst.tick();
    alarm(TIMESLOT);
}

// 定时器回调函数: 删除非活动连接socket上的注册事件，并关闭连接
void cb_func(client_data* user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr;
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

    // 注册监听描述符的事件
    addfd(epollfd, listenfd);

    // 创建双向管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);

    // 注册管道pipdfd[0]上的可读事件
    addfd(epollfd, pipefd[0]);

    // 设置信号处理函数
    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;

    client_data* users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);

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

                // 注册连接描述符事件
                addfd(epollfd, connfd);

                // 创建用户信息
                users[connfd].addr = clnt_addr;
                users[connfd].sockfd = connfd;
                // 创建定时器，并设置超时时间和回调函数
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;        // 将用户和定时器绑定
                timer_lst.add_timer(timer);         // 将定时器添加到定时器链表中
            }
            // 处理信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
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
                        switch (signals[i])
                        {
                            // 用timeout标记有定时任务需要处理，但不会立即处理定时任务
                            // 因为定时任务的优先级并不高，优先处理其他重要任务
                            case SIGALRM:
                            {
                                timeout = true;
                                break;
                            }

                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            // 处理客户端连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                memset(users[sockfd].buf, '\0', sizeof(users[sockfd].buf));
                ret = recv(sockfd, users[sockfd].buf, sizeof(users[sockfd].buf) - 1, 0);
                printf("got %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd);

                util_timer* timer = users[sockfd].timer;
                if (ret < 0)
                {
                    // 如果发生错误，则关闭连接，并移除对应的定时器
                    if (errno != EAGAIN)
                    {
                        cb_func(&users[sockfd]);
                        if (timer)
                        {
                            timer_lst.del_timer(timer);
                        }
                    }
                }
                else if (ret == 0)
                {
                    // 如果对方已经关闭连接，那我们也关闭连接，并移除相应的定时器
                    cb_func(&users[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
                else 
                {
                    // 如果某个客户连接上有数据可读，则我们需要调整该连接对应的定时器，以延迟
                    // 该连接被关闭的时间
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("adjust time once\n");
                        timer_lst.adjust_timer(timer);
                    }
                }
            }
        }

        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }

    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete [] users;
    return 0;
}


