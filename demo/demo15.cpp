/*********************************************************************************
 * File Name: demo15.cpp
 * Description: 死锁：在多线程环境中使用fork
 * Author: jinglong
 * Date: 2020年4月30日 14:35
 * History: 
 *********************************************************************************/

#include "global.h"

pthread_mutex_t mutex;

// 子线程运行的函数。首先获取互斥锁，暂停5s，再释放该互斥锁
void *another(void *arg)
{
    printf("in child thread, lock the mutex\n");
    pthread_mutex_lock(&mutex);
    sleep(5);
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char * argv [ ])
{
    pthread_mutex_init(&mutex, NULL);
    pthread_t id;

    pthread_create(&id, NULL, another, NULL);
    sleep(1);       // 暂停1s，确保主进程在执行fork之前，子线程已经获得了互斥变量mutex

    int pid = fork();
    if (pid < 0)
    {
        pthread_join(id, NULL);
        pthread_mutex_destroy(&mutex);
        return 1;
    }
    else if(pid == 0)
    {
        // 子进程从父进程继承了互斥锁mutex的状态，该互斥锁被子线程another锁住且未释放
        // 如果该子进程继续对互斥锁进行加锁，就会进入死锁状态
        printf("I am in the child, want to get the lock\n");
        pthread_mutex_lock(&mutex);
        printf("I can run to here, oop...\n");
        pthread_mutex_unlock(&mutex);
        exit(0);
    }
    else 
    {
        wait(NULL);
    }

    pthread_join(id, NULL);
    pthread_mutex_destroy(&mutex);
    return 0;
}


