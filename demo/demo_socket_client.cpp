/*********************************************************************************
 * File Name: demo_socket_client.cpp
 * Description: 文件传输系统客户端demo
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

#define          MAXBUFSIZE          1024

// 文件传输系统
int main(int argc, char * argv [ ])
{
    char *filename = "/mnt/hgfs/gitSpace/weatherDC/remotedata/20200319185105.txt";
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        printf("文件不存在\n");
        return -1;
    }

    int clnt_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);
    connect(clnt_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    char buffer[MAXBUFSIZE] = {'\0'};
    int iCount = 0;
    while ((iCount = read(clnt_sock, buffer, sizeof(buffer))) > 0)
    {
        fwrite(buffer, iCount, 1, fp);
    }

    printf("文件传输完毕\n");

    fclose(fp);
    close(clnt_sock);

    return 0;
}

