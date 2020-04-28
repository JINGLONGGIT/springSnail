/*********************************************************************************
 * File Name: demo7_server.cpp
 * Description: 能同时处理TCP连接和UDP连接的回射服务器
 * Author: jinglong
 * Date: 2020年4月28日 14:47
 * History: 
 *********************************************************************************/

#include "global.h"

#define     MAX_EVENT_NUMBER        1024
#define     TCP_BUFFER_SIZE         512
#define     UDP_BUFFER_SIZE         1024

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

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr;
    int connfd;

    // 创建TCP socket，并将其绑定到端口上
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

    // 创建UDP socket，并将其绑定到端口上
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);

    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(udpfd >= 0);

    ret = bind(udpfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 注册描述符事件
    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);

    while (1)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0)
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
                addfd(epollfd, connfd);
            }
            else if (sockfd == udpfd)
            {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', sizeof(buf));
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);

                ret = recvfrom(sockfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
                if (ret > 0)
                {
                    sendto(sockfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*)&clnt_addr, clnt_addr_len);
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                char buf[TCP_BUFFER_SIZE];
                while (1)
                {
                    memset(buf, '\0', sizeof(buf));
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if (ret < 0)
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        {
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
                        send(sockfd, buf, ret, 0);
                    }
                }
            }
            else 
            {
                printf("something else happend\n");
            }
        }
    }

    close(listenfd);
    return 0;
}

