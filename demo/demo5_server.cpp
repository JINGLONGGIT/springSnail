/*********************************************************************************
 * File Name: demo5_server.cpp
 * Description: epoll函数中使用文件描述符的EPOLLONESHOT事件
 * Author: jinglong
 * Date: 2020年4月27日 21:00
 * History: 
 *********************************************************************************/

#include "global.h"

#define         MAX_EVENT_NUMBER        1024
#define         BUFFER_SIZE             1024

struct fds
{
    int epollfd;
    int sockfd;
};

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
 *           bool oneshot: 是否注册fd文件描述符的EPOLLONESHOT事件
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void addfd(int epollfd, int fd, bool oneshot)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    if (oneshot)
    {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/*************************************************************************
 * 函数名称：reset_oneshot
 * 函数功能：重置fd上的事件
 * 输入参数：int epollfd: 内核事件表的文件描述符，由epoll_create()函数生成
 *           int fd: 要注册的文件描述符
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void reset_oneshot(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 工作线程
void* worker(void *arg)
{
    int sockfd = ((fds*)arg)->sockfd;
    int epollfd = ((fds*)arg)->epollfd;
    printf("start new thread to receive data on fd: %d", sockfd);

    char buf[BUFFER_SIZE];
    memset(buf, '\0', sizeof(buf));

    while (1)
    {
        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if (ret == 0)
        {
            printf("foreiner closed the connection\n");
            close(sockfd);
            break;
        }
        else if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }
        else 
        {
            printf("get content : %s\n", buf);
            sleep(5);
        }
    }
}

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr, clnt_addr;
    int listenfd, connfd;

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

    // 注册监听描述符事件: 监听描述符listenfd是不能被注册为EPOLLONESHOT事件的，否则只能被触发一次
    addfd(epollfd, listenfd, false);

    while (1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0)
        {
            printf("epoll failed\n");
            break;
        }

        for (int i = 0; i < ret; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
            
                // 对每个非监听描述符都注册EPOLLONESHOT事件
                addfd(epollfd, connfd, true);
            }
            else if (events[i].events & EPOLLIN)
            {
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;

                // 创建一个新线程来为sockfd工作
                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
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

