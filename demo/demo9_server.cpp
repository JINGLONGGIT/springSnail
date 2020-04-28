/*********************************************************************************
 * File Name: demo9_server.cpp
 * Description: 使用SIGURG信号检测带外数据是否到达
 * Author: jinglong
 * Date: 2020年4月28日 21:37
 * History: 
 *********************************************************************************/

#include "global.h"

#define         BUFFER_SIZE             1024

static int connfd;

/*************************************************************************
 * 函数名称：sig_handler
 * 函数功能：信号处理函数
 * 输入参数：int sig
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void sig_handler(int sig)
{
    // 保留原来的errno, 在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', sizeof(buffer));

    int ret = recv(connfd, buffer, BUFFER_SIZE - 1, MSG_OOB);
    printf("got %d bytes of oob data '%s'\n", ret, buffer);

    errno = save_errno;
}

/*************************************************************************
 * 函数名称：addsig
 * 函数功能：设置信号的处理函数
 * 输入参数：int sig                        信号
 *           void(*sig_handler)(int)        函数指针，指向信号处理函数
 * 输出参数：无
 * 返 回 值：无
 *************************************************************************/
void addsig(int sig, void(*sig_handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(1234);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
    if (connfd < 0)
    {
        printf("errno is %d\n", errno);
    }
    else 
    {
        addsig(SIGURG, sig_handler);

        // 使用SIGURG信号之前，我们必须设置socket的宿主进程或者进程组
        fcntl(connfd, F_SETOWN, getpid());

        char buffer[BUFFER_SIZE];
        while (1)
        {
            memset(buffer, '\0', sizeof(buffer));
            ret = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
            if (ret <= 0)
            {
                break;
            }

            printf("got %d bytes of normal data '%s'\n", ret, buffer);
        }

        close(connfd);
    }

    close(listenfd);
    return 0;
}

