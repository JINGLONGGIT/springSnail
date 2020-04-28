/*********************************************************************************
 * File Name: demo_socket_server.cpp
 * Description: 文件传输系统服务端demo
 * Author: jinglong
 * Date: 2020年4月27日 16:00
 * History: 
 *********************************************************************************/

#include "global.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define     BUFSIZE         1024

// 文件传输系统
int main(int argc, char * argv [ ])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    char buffer[BUFSIZE];

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);
    bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    listen(serv_sock, 20);

    socklen_t clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

    int i32RecvLen = recv(clnt_sock, buffer, sizeof(buffer), 0);
    if (i32RecvLen > 0)
    {
        printf("recv data: %s", buffer);
    }

    send(clnt_sock, buffer, i32RecvLen, 0);

    close(clnt_sock);
    close(serv_sock);

    return 0;
}

