/*********************************************************************************
 * File Name: demo14_server.cpp
 * Description: 使用共享内存的聊天室服务器
 * Author: jinglong
 * Date: 2020年4月30日 09:00
 * History: 
 *********************************************************************************/

#include "global.h"

#define         USER_LIMIT              5               // 最大用户数量
#define         FD_LIMIT                65525           // 文件描述符最大数量
#define         BUFFER_SIZE             1024
#define         MAX_EVENT_NUMBER        1024
#define         PROCESS_LIMIT           65536

// 用户数据
struct client_data
{
    sockaddr_in addr;                   // 客户端socket地址
    int connfd;                         // socket描述符
    pid_t pid;                          // 处理这个连接的子进程PID
    int pipefd[2];                      // 和父进程通信的管道
};

static const char* shm_name = "/my_shm";
int sig_pipefd[2];                          // 用来传输信号的管道
int epollfd;                                
int listenfd;
int shmfd;
char* share_mem = 0;
int * sub_process = 0;      // 子进程和客户连接的映射关系表。用进程的PID来索引这个数组，即可得到该进程所处理的客户连接的编号
client_data* users = 0;      // 客户连接数组。进程用客户连接的编号来索引这个数组，即可取得相关的客户连接
int user_count = 0;          // 当前客户数量
bool stop_child = false;

// 将文件描述符设置成非阻塞的
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 注册fd文件描述符的事件
void addfd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数
void sig_handler(int sig)
{
    int save_erron = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_erron;
}

// 设置信号sig的信号处理函数
void addsig(int sig, void(* sig_handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void del_resource()
{
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

// 子进程的信号处理函数停止一个子进程
void child_term_handler(int sig)
{
    stop_child = true;
}

/**********************************************************
 * 函数名称：run_child
 * 函数功能：运行子进程
 * 输入参数：int idx                   该子进程处理的客户连接的编号
 *           client_data* users     保存所有客户连接数据的数组
 *           char* share_mem        共享内存的起始地址
 * 输出参数：无
 * 返 回 值：0
 **********************************************************/
int run_child(int idx, client_data* users, char* share_mem)
{
    struct epoll_event events[MAX_EVENT_NUMBER];

    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);

    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);

    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);

    int ret;
    addsig(SIGTERM, child_term_handler, false);

    while (!stop_child)
    {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failed\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if ((sockfd == connfd) && (events[i].events & EPOLLIN))
            {
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN)
                    {
                        stop_child = true;
                    }
                }
                else if (ret == 0)
                {
                    stop_child = true;
                }
                else 
                {
                    // 成功读取客户数据后通知主进程来处理
                    send(pipefd, (char*)&idx, sizeof(idx), 0);
                }
            }
            // 主进程通知本进程(通过管道)将第client个客户的数据发送到本进程负责的客户端
            else if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
            {
                int client;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN)
                    {
                        stop_child = true;
                    }
                }
                else if (ret == 0)
                {
                    stop_child = true;
                }
                else 
                {
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            }
            else 
            {
                continue;
            }
        }
    }

    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
}

int main(int argc, char * argv [ ])
{
    struct sockaddr_in serv_addr;
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

    user_count = 0;
    users = new client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; i++)
    {
        sub_process[i] = -1;
    }

    struct epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 注册监听描述符的事件
    addfd(epollfd, listenfd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);

    // 注册管道pipdfd[0]上的可读事件
    addfd(epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);

    bool stop_server = false;
    bool terminate =  false;

    // 创建共享内存，作为所有客户socket连接的读缓存
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
    assert(ret != -1);
    share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failed\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            // 处理新客户连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in clnt_addr;
                socklen_t clnt_addr_len = sizeof(clnt_addr);

                int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
                if (connfd < 0)
                {
                    printf("errno is %d\n", errno);
                    continue;
                }

                // 如果用户数量超过限制，则关闭新到用户连接
                if (user_count >= USER_LIMIT)
                {
                    const char* info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, sizeof(info), 0);
                    close(connfd);
                    continue;
                }

                users[user_count].addr = clnt_addr;
                users[user_count].connfd = connfd;

                // 在主进程和子进程之间创建管道，以传递必要的数据
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);

                pid_t pid = fork();
                if (pid < 0)
                {
                    close(connfd);
                    continue;
                }
                else if (pid == 0)
                {
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem  );
                    munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                }
                else 
                {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    user_count++;
                }
            }
            // 处理信号事件
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else 
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                            // 子进程退出，表示有某个客户端关闭了连接
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;

                                while ((pid == waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    // 用子进程的PID获取被关闭的客户连接的编号
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if ((del_user < 0) || (del_user > USER_LIMIT))
                                    {
                                        continue;
                                    }

                                    // 清除第del_user个客户连接的相关数据
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = del_user;
                                }

                                if (terminate && user_count == 0)
                                {
                                    stop_server = true;
                                }

                                break;
                            }

                            case SIGTERM:
                            case SIGINT:
                            {
                                printf("kill all the child now\n");
                                if (user_count ==  0)
                                {
                                    stop_server = true;
                                    break;
                                }

                                for (int i = 0; i < user_count; i++)
                                {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM);
                                }

                                terminate = true;
                                break;
                            }

                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
            // 处理数据收发：某个子进程向父进程写入了数据
            else if (events[i].events & EPOLLIN)
            {
                int child = 0;
                ret = recv(sockfd, (char*)&child, sizeof(child), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else 
                {
                    for (int j = 0; j < user_count; j++)
                    {
                        if (users[j].pipefd[0] != sockfd)
                        {
                            printf("send data to child accross pipe\n");
                            send(users[j].pipefd[0], (char *)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }

    del_resource();
    return 0;
}

