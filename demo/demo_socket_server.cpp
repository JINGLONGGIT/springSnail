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
    const char *filename = "/mnt/hgfs/gitSpace/weatherDC/remotedata/20200319185105.txt";
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        printf("文件不存在\n");
        return -1;
    }

    int listenfd, connfd;
    struct sockaddr_in serv_addr, clnt_addr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    listen(listenfd, 20);

    socklen_t clnt_addr_size = sizeof(clnt_addr);
    connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

    char buffer[BUFSIZE] = {'\0'};
    int nCount;
    while ((nCount = fread(buffer, 1, BUFSIZE, fp)) > 0)
    {
        send(connfd, buffer, BUFSIZE - 1, 0);
    }
    
    shutdown(connfd, SHUT_WR);                  // 断开输出流，向客户端发送FIN
    recv(connfd, buffer, sizeof(buffer), 0);

    fclose(fp);
    close(connfd);
    close(listenfd);
    return 0;
}

