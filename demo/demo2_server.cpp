/*********************************************************************************
 * File Name: demo2_server.cpp
 * Description: dup函数实现将标准输出重定向到网络连接
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
    else 
    {
        close(STDOUT_FILENO);       // 关闭标准输出
        dup(connfd);                // 将标准输出重定向到已连接描述符
        printf("abc\n");            // 本应该输出到标准输出的字符现在被重定向输出到连接描述符
    }

    close(connfd);
    close(listenfd);
    return 0;
}

