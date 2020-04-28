/*********************************************************************************
 * File Name: demo_socket_server.cpp
 * Description: 发送带外数据客户端
 * Author: jinglong
 * Date: 2020年4月27日 21:00
 * History: 
 *********************************************************************************/

#include "global.h"

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
    else 
    {
        const char *normal_data = "123";
        const char *oob_data = "abc";
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
        send(sockfd, normal_data, strlen(normal_data), 0);
    }

    close(sockfd);
    return 0;
}

