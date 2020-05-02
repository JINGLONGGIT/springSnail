/*********************************************************************************
 * File Name: demo4_server.cpp
 * Description: 使用epoll函数的服务端程序，实现LT和ET两种工作模式，使用telnet作为客户端
 * Author: jinglong
 * Date: 2020年4月27日 21:00
 * History: 
 *********************************************************************************/

#include "global.h"

#define         MAX_EVENT_NUMBER        1024
#define         BUFFER_SIZE             10

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
 *           bool enable_et: 是否对fd启用et模式
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void addfd(int epollfd, int fd, bool enable_et)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;

    if (enable_et)
    {
        event.events |= EPOLLET;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/*************************************************************************
 * 函数名称：lt
 * 函数功能：LT工作模式
 * 输入参数：struct epoll_event* events        就绪事件
 *           int number                   epoll_wait()返回就绪的文件符个数
 *           int epollfd                  内核事件表的文件描述符
 *           int listenfd                 监听描述符
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void lt(struct epoll_event* events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];

    for (int i = 0; i < number; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in clnt_addr;
            socklen_t clnt_addr_len = sizeof(clnt_addr);
            int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);

            // 注册连接描述符事件: 禁用ET模式
            addfd(epollfd, connfd, false);
        }
        else if (events[i].events & EPOLLIN)
        {
            // 只要socket读缓存中还有未读出的数据，这段代码就会被触发
            printf("event trigger once\n");
            memset(buf, '\0', sizeof(BUFFER_SIZE));

            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret <= 0)
            {
                close(sockfd);
                continue;
            }

            printf("get %d bytes of content: %s\n", ret, buf);
        }
        else 
        {
            printf("something else happend\n");
        }
    }
}

/*************************************************************************
 * 函数名称：et
 * 函数功能：ET工作模式
 * 输入参数：struct epoll_event* events        就绪事件
 *           int number                   epoll_wait()返回就绪的文件符个数
 *           int epollfd                  内核事件表的文件描述符
 *           int listenfd                 监听描述符
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void et(struct epoll_event* events, int number, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];

    for (int i = 0; i < number; i++)
    {
        int sockfd = events[i].data.fd;

        if (sockfd == listenfd)
        {
            struct sockaddr_in clnt_addr;
            socklen_t clnt_addr_len = sizeof(clnt_addr);
            int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);

            // 注册连接描述符事件：启用ET模式
            addfd(epollfd, connfd, true);
        }
        else if (events[i].events & EPOLLIN)
        {
            // 这段代码不会被重复触发，所以我们可以循环读取数据，以确保把socket读缓存中的所有数据读出
            printf("event trigger once\n");

            while (1)
            {
                memset(buf, '\0', sizeof(BUFFER_SIZE));
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);

                if (ret < 0)
                {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                    {
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if (ret == 0)
                {
                    close(sockfd);
                }
                else
                {
                    printf("got %d bytes of content %s\n", ret, buf);
                }
            }
        }
        else 
        {
            printf("something else happend\n");
        }
    }
}

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr, clnt_addr;
    int listenfd;

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 注册监听描述符事件
    addfd(epollfd, listenfd, true);

    while (1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
        {
            printf("epoll failed\n");
            break;
        }

        lt(events, ret, epollfd, listenfd);
//        et(events, ret, epollfd, listenfd);
    }

    close(listenfd);
    return 0;
}

