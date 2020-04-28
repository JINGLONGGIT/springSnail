/*********************************************************************************
 * File Name: demo6_server.cpp
 * Description: 基于IO复用的聊天室程序：服务端
 * Author: jinglong
 * Date: 2020年4月28日 11:00
 * History: 
 *********************************************************************************/

#include "global.h"

#define         USER_LIMIT      5           // 最大用户数量
#define         BUFFER_SIZE     64          // 读缓冲区的大小
#define         FD_LIMIT        65535       // 文件描述符最大数量限制

// 用户数据
struct client_data
{
    sockaddr_in addr;               // 客户端地址
    char *writeBuf;                 // 待写入到客户端的数据
    char buf[BUFFER_SIZE];          // 从客户端读入的数据
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

    struct client_data* users = new client_data[USER_LIMIT];
    int userCounts = 0;

    struct pollfd fds[FD_LIMIT + 1];
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    for (int i = 1; i <= FD_LIMIT; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    while (1)
    {
        ret = poll(fds, userCounts + 1, -1);
        if (ret < 0)
        {
            printf("poll failed\n");
            break;
        }

        for (int i = 0; i < userCounts + 1; i++)
        {
            if ((fds[i].fd == listenfd) && (fds[i].revents & POLLIN))
            {
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
                if (connfd < 0)
                {
                    printf("errno is %d\n", errno);
                    continue;
                }

                // 如果请求太多，则关闭新到连接
                if (userCounts >= USER_LIMIT)
                {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, sizeof(info), 0);
                    close(connfd);
                    continue;
                }

                userCounts++;
                users[connfd].addr = clnt_addr;
                setnonblocking(connfd);
                fds[userCounts].fd = connfd;
                fds[userCounts].events = POLLIN | POLLRDHUP | POLLERR;
                fds[userCounts].revents = 0;
                printf("create a new user, now has %d users\n", userCounts);
            }
            else if (fds[i].revents & POLLERR)
            {
                printf("get an error from %d\n", fds[i].fd);
                char errors[100];
                memset(errors, '\0', sizeof(errors));
                socklen_t length = sizeof(errors);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
                {
                    printf("get socket option failed\n");
                }
                continue;
            }
            else if (fds[i].revents & POLLRDHUP)
            {
                users[fds[i].fd] = users[fds[userCounts].fd];
                close(fds[i].fd);
                fds[i] = fds[userCounts];
                i--;
                userCounts--;
                printf("a client left\n");
            }
            else if (fds[i].revents & POLLIN)
            {
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);

                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
                if (ret < 0)
                {
                    // 如果读操作出错，则关闭连接
                    if (errno != EAGAIN)
                    {
                        close(connfd);
                        users[fds[i].fd] = users[fds[userCounts].fd];
                        fds[i] = fds[userCounts];
                        i--;
                        userCounts--;
                    }
                }
                else if (ret == 0)
                {

                }
                else 
                {
                    // 如果接收到客户数据，则通知其他socket连接准备写数据
                    for (int j = 1; j <= userCounts; j++)
                    {
                        if (fds[j].fd == connfd)
                            continue;
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].writeBuf = users[connfd].buf;
                    }
                }
            }
            else if (fds[i].revents & POLLOUT)
            {
                int connfd = fds[i].fd;
                if (!users[connfd].writeBuf)
                    continue;

                ret = send(connfd, users[connfd].writeBuf, strlen(users[connfd].writeBuf), 0);
                users[connfd].writeBuf = NULL;

                // 写完数据后重新注册fds[i]上的可读事件
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    
    delete[] users;
    close(listenfd);
    return 0;
}

