/*****************************************************************
 * File Name: stress_test.cpp
 * Description: WEB服务器(demo19_server.cpp)压力测试程序
 * Author: jinglong
 * Date: 2020年5月4日 21:53
 * History: 
 *****************************************************************/

#include "global.h"

static const char* request = "GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxx";

// 把文件描述符fd设置为非阻塞的
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 注册文件描述符fd上的可读事件/错误事件
void addfd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 向服务器写入len字节的数据
bool write_nbytes(int sockfd, const char* buffer, int len)
{
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, sockfd);

    while (1)
    {
        bytes_write = send(sockfd, buffer, len, 0);
        if (bytes_write == -1)
        {
            return false;
        }
        else if (bytes_write == 0)
        {
            return false;
        }

        len -= bytes_write;
        buffer = buffer + bytes_write;

        if (len <= 0)
        {
            return true;
        }
    }
}

// 从服务器读取数据
bool read_once(int sockfd, char* buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);

    bytes_read = recv(sockfd, buffer, len, 0);

    if (bytes_read == -1)
    {
        return false;
    }
    else if (bytes_read == 0)
    {
        return false;
    }

    printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);

    return true;
}

/***************************************************************************
 * 函数名称：start_conn
 * 函数功能：向服务器发起num个TCP连接请求(可以通过改变num来调整测试压力)
 * 输入参数：int epollfd                    内核事件表的文件描述符
 *           int num                        要创建的TCP连接的个数
 *           const char* ip                 服务器IP地址
 *           int port                       服务器端口号
 * 输出参数：无
 * 返 回 值：无
 ***************************************************************************/
void start_conn(int epollfd, int num, const char* ip, int port)
{
    struct sockaddr_in serve_addr;
    bzero(&serve_addr, sizeof(serve_addr));
    serve_addr.sin_family = AF_INET;
    serve_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serve_addr.sin_port = htons(1234);

    for (int i = 0; i < num; i++)
    {
        sleep(1);
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        printf("create 1 socket\n");

        if (sockfd < 0)
        {
            continue;
        }

        if (connect(sockfd, (struct sockaddr*)&serve_addr, sizeof(serve_addr)) == 0)
        {
            printf("build connection %d\n", i);

            // 注册sockfd上的可写事件
            addfd(epollfd, sockfd);
        }
    }
}

/**********************************************************
 * 函数名称：close_conn
 * 函数功能：删除sockfd注册的事件，并关闭连接
 * 输入参数：int epollfd:   内核注册事件描述符
 *           int sockfd:    要关闭的文件描述符
 * 输出参数：无
 * 返 回 值：无
 **********************************************************/
void close_conn(int epollfd, int sockfd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

int main(int argc, char * argv [ ])
{
    if (argc != 2)
    {
        printf("usage: %s TCP_Con_Num\n", basename(argv[0]));
        return 1;
    }
    
    int epollfd = epoll_create(100);
    start_conn(epollfd, atoi(argv[1]), "127.0.0.1", 1234);

    struct epoll_event events[10000];
    char buffer[2048];

    while (1)
    {
        int number = epoll_wait(epollfd, events, 10000, 2000);

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                if (!read_once(sockfd, buffer, 2048))
                {
                    close_conn(epollfd, sockfd);
                }

                // 注册sockfd上的可写事件
                struct epoll_event event;
                event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!write_nbytes(sockfd, request, strlen(request)))
                {
                    close_conn(epollfd, sockfd);
                }

                // 注册sockfd上的可读事件
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
            }
            else if (events[i].events & EPOLLERR)
            {
                close_conn(epollfd, sockfd);
            }
        }
    }

    return 0;
}

