/*********************************************************************************
 * File Name: demo10_client.cpp
 * Description: 具有超时功能的connect函数demo
 * Author: jinglong
 * Date: 2020年4月28日 21:37
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

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    socklen_t len = sizeof(timeout);
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, len);
    assert(ret != -1);

    ret = connect(sockfd, (struct sockaddr*)&serve_addr, sizeof(serve_addr));

    if (ret == -1)
    {
        if (errno == EINPROGRESS)
        {
            printf("connecting timeout, process timeout logic\n");
            return -1;
        }

        printf("error occur when connecting to server\n");
        return -1;
    }

    close(sockfd);
    return 0;
}


