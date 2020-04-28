/*********************************************************************************
 * File Name: demo3_server.cpp
 * Description: 使用select函数改写接收带外数据的服务端程序，仍然使用demo1_client.cpp作为客户端
 * Author: jinglong
 * Date: 2020年4月27日 21:00
 * History: 
 *********************************************************************************/

#include "global.h"

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

    socklen_t clnt_addr_len = sizeof(clnt_addr);
    connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
    if (connfd < 0)
    {
        printf("errno is %d\n", errno);
    }

    char buffer[1024];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    while (1)
    {
        memset(buffer, '\0', sizeof(buffer));
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);

        int ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0)
        {
            printf("select failed\n");
            break;
        }

        if (FD_ISSET(connfd, &read_fds))
        {
            ret = recv(connfd, buffer, sizeof(buffer) - 1, 0);
            if (ret <= 0)
            {
                break;
            }

            printf("got %d bytes of normal data '%s'\n", ret, buffer);
        }
        else if (FD_ISSET(connfd, &exception_fds))
        {
            ret = recv(connfd, buffer, sizeof(buffer) - 1, MSG_OOB);
            if (ret <= 0)
            {
                break;
            }

            printf("got %d bytes of oob data '%s'\n", ret, buffer);
        }
    }

    close(connfd);
    close(listenfd);
    return 0;
}

