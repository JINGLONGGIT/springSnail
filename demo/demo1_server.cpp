/*********************************************************************************
 * File Name: demo1_server.cpp
 * Description: 接收带外数据服务端
 * Author: jinglong
 * Date: 2020年4月27日 21:00
 * History: 
 *********************************************************************************/

#include "global.h"

#define     BUFSIZE         1024

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
    else 
    {
        char buffer[BUFSIZE];
        memset(buffer, '\0', BUFSIZE);
        ret = recv(connfd, buffer, BUFSIZE - 1, 0);
        printf("got %d bytes of normal data '%s'\n", ret, buffer);

        memset(buffer, '\0', BUFSIZE);
        ret = recv(connfd, buffer, BUFSIZE - 1, MSG_OOB);
        printf("got %d bytes of oob data '%s'\n", ret, buffer);

        memset(buffer, '\0', BUFSIZE);
        ret = recv(connfd, buffer, BUFSIZE - 1, 0);
        printf("got %d bytes of normal data '%s'\n", ret, buffer);
    }

    close(connfd);
    close(listenfd);
    return 0;
}

