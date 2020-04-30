/*********************************************************************************
 * File Name: demo16.cpp
 * Description: 多线程环境中用一个线程处理所有信号
 * Author: jinglong
 * Date: 2020年4月30日 14:51
 * History: 
 *********************************************************************************/

#include "global.h"

void handle_error_en(int en, const char *msg)
{
    errno = en;
    perror(msg);
    exit(EXIT_FAILURE);
}

static void* sig_thread(void *arg)
{
    sigset_t *set = (sigset_t*)arg;
    int s, sig;

    while (1)
    {
        s = sigwait(set, &sig);
        if (s != 0)
        {
            handle_error_en(s, "sigwait");
        }

        printf("signal handling thread got signal %d\n", sig);
    }
}

int main(int argc, char * argv [ ])
{
    pthread_t thread;
    sigset_t set;
    int s;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);

    // 在主进程中设置信号掩码
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (s != 0)
    {
        handle_error_en(s, "pthread_sigmask");
    }

    s = pthread_create(&thread, NULL, sig_thread, (void*)&set);
    if (s != 0)
    {
        handle_error_en(s, "pthread_create");
    }

    return 0;
}

