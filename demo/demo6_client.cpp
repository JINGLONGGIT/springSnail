/*********************************************************************************
 * File Name: demo6_server.cpp
 * Description: 基于IO复用的聊天室程序：客户端
 * Author: jinglong
 * Date: 2020年4月28日 11:00
 * History: 
 *********************************************************************************/

#include "global.h"

#define         BUFFER_SIZE             1024

int main(int argc, char * argv [ ])
{
    int sockfd;
    struct sockaddr_in serve_addr;

    bzero(&serve_addr, sizeof(serve_addr));
    serve_addr.sin_family = AF_INET;
    serve_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serve_addr.sin_port = htons(1234);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if (connect(sockfd, (struct sockaddr*)&serve_addr, sizeof(serve_addr)) < 0)
    {
        printf("connect failed\n");
    }

    // 注册文件描述符0(标准输入)和文件描述符sockfd上的可读事件
    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    char readBuf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    while (1)
    {
        ret = poll(fds, 2, -1);
        if (ret < 0)
        {
            printf("poll failed\n");
            break;
        }

        if (fds[1].revents & POLLRDHUP)
        {
            printf("server close the connection\n");
            break;
        }
        else if (fds[1].revents & POLLIN)
        {
            memset(readBuf, '\0', sizeof(readBuf));
            recv(fds[1].fd, readBuf, sizeof(readBuf) - 1, 0);
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            // 使用splice实现零拷贝
            // 将标准输入上的流定向到管道中
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            assert(ret != -1);

            // 将管道中的输出定向到文件描述符sockfd中
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            assert(ret != -1);
        }
    }

    close(sockfd);
    return 0;
}

