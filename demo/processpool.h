#ifndef __PROCESSPOOL_H_
#define __PROCESSPOOL_H_

// 高效的半同步/半异步进程池
#include "global.h"

// 子进程类
class process
{
public:
    process() : m_pid(-1){}

public:
    pid_t m_pid;            // 目标子进程的PID
    int m_pipdfd[2];        // 父进程和子进程之间通信的管道
};

// 进程池类
template <typename T>
class processPoll
{
private:
    // 单例模式：进程池实例只能由静态函数create来创建
    processPoll(int listenfd, int process_number = 0);

public:
    static processPoll<T>* create(int listenfd, int process_number = 8)
    {
        if (!m_instance)
        {
            m_instance = new processPoll<T>(listenfd, process_number);
        }

        return m_instance;
    }

    ~processPoll()
    {
        delete [] m_sub_process;
    }

    // 启动进程池
    void run();

private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();

private:
    static const int MAX_PROCESS_NUMBER = 16;        // 进程池允许的最大子进程数量
    static const int USER_PER_PROCESS = 65535;       // 每个子进程最多能处理的客户端数量
    static const int MAX_EVENT_NUMBER = 10000;       // epoll最多能处理的事件数
    int m_process_number;                            // 进程池中的进程总数
    int m_idx;                                       // 子进程在进程池中的编号。从0开始
    int m_epollfd;                                   // 每个子进程都有一个epoll内核事件表，用m_epollfd标识
    int m_listenfd;                                  // 监听socket
    int m_stop;                                      // 子进程通过m_stop决定是否停止运行
    process* m_sub_process;                          // 保存所有子进程的描述信息
    static processPoll<T>* m_instance;               // 进程池静态实例
};

template <typename T>
processPoll<T>* processPoll<T>::m_instance = NULL;

static int sig_pipefd[2];       // 信号管道，用于统一事件源

// 将描述符fd设置为非阻塞的
static int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 注册内核事件表
static void addfd(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核事件表中删除描述符fd上所有的注册事件
static void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 信号处理函数
static void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号sig的信号处理函数
static void addsig(int sig, void(*handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }

    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

/**************************************************************
 * 函数名称：processPoll<T>::processPoll
 * 函数功能：进程池构造函数
 * 输入参数：int listenfd           监听描述符。必须在创建进程池之前被创建，否则子进程无法引用它
 *          int process_number      要创建的子进程的数量
 * 输出参数：无
 * 返 回 值：无
 **************************************************************/ 
template<typename T>
processPoll<T>::processPoll(int listenfd, int process_number)
    : m_listenfd(listenfd), m_process_number(process_number), m_idx(-1) 
{
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));

    // 创建process_number个子进程，并建立它们和父进程之间的管道
    m_sub_process = new process[process_number];
    assert(m_sub_process);
    
    for (int i = 0; i < process_number; i++)
    {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipdfd);
        assert(ret != -1);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        
        if (m_sub_process[i].m_pid > 0)
        {
            close(m_sub_process[i].m_pipdfd[1]);
            continue;
        }
        else 
        {
            close(m_sub_process[i].m_pipdfd[0]);
            m_idx = i;
            break;
        }
    }
}

// 统一事件源
template<typename T>
void processPoll<T>::setup_sig_pipe()
{
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    // 设置信号处理函数
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

/**************************************************************
 * 函数名称：run
 * 函数功能：启动进程池。父进程中m_idx等于-1，子进程中m_idx全部大于等于0
 * 输入参数：无
 * 输出参数：无
 * 返 回 值：无
 **************************************************************/ 
template<typename T>
void processPoll<T>::run()
{
    if (m_idx != -1)
    {
        run_child();
        return ;
    }

    run_parent();
}

// 运行父进程
template<typename T>
void processPoll<T>::run_parent()
{
    setup_sig_pipe();
    addfd(m_epollfd, m_listenfd);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failed\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd)
            {
                // 如果有新连接到来，就交给子进程处理
                int i = sub_process_counter;
                do
                {
                    if (m_sub_process[i].m_pid != -1)
                    {
                        break;
                    }
                    i = (i + 1) % m_process_number;
                } while (i != sub_process_counter);

                if (m_sub_process[i].m_pid == -1)
                {
                    m_stop = true;
                    break;
                }

                sub_process_counter = (i + 1) % m_process_number;
                send(m_sub_process[i].m_pipdfd[0], (char *)&new_conn, sizeof(new_conn), 0);
                printf("send request to child %d\n", i);
            }
            // 处理父进程接收到的信号
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else 
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD:
                            {
                                // 如果某个子进程退出
                                pid_t pid;
                                int stat;
                                while ((pid == waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    for (int i = 0; i < m_process_number; i++)
                                    {
                                        if (m_sub_process[i].m_pid == pid)
                                        {
                                            printf("child %d join\n", pid);
                                            close(m_sub_process[i].m_pipdfd[0]);
                                            m_sub_process[i].m_pid = -1;
                                        }
                                    }
                                }

                                // 如果所有子进程都退出了，那么父进程也退出
                                m_stop = true;
                                for (int i = 0; i < m_process_number; i++)
                                {
                                    if (m_sub_process[i].m_pid != -1)
                                    {
                                        m_stop = false;
                                    }
                                }

                                break;
                            }

                            case SIGTERM:
                            case SIGINT:
                            {
                                // 如果父进程接收到终止信号，就杀死所有的子进程
                                printf("kill all the child now\n");
                                for (int i = 0; i < m_process_number; i++)
                                {
                                    int pid = m_sub_process[i].m_pid;
                                    if (pid != -1)
                                    {
                                        kill(pid, SIGTERM);
                                    }
                                }

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
            else 
            {
                continue;
            }
        }
    }
    
    close(m_epollfd);
}

// 运行子进程
template<typename T>
void processPoll<T>::run_child()
{
    setup_sig_pipe();

    int pipefd = m_sub_process[m_idx].m_pipdfd[1];
    addfd(m_epollfd, pipefd);

    struct epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];
    assert(users);
    
    int number = 0;
    int ret = -1;

    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failed\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            // 子进程处理客户连接
            if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
            {
                int client = 0;
                ret = recv(sockfd, (char *)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EAGAIN)) || (ret == 0))
                {
                    continue;
                }
                else
                {
                    struct sockaddr_in clnt_addr;
                    socklen_t clnt_addr_len = sizeof(clnt_addr);
                    
                    int connfd = accept(m_listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
                    if (connfd < 0)
                    {
                        printf("errno is %d\n", errno);
                        continue;
                    }

                    addfd(m_epollfd, connfd);
                    // 模板类T必须实现init方法，以初始化一个客户连接
                    // 直接使用connfd来索引客户对象(T类型的对象)，以提高程序效率
                    users[connfd].init(m_epollfd, connfd, clnt_addr);
                }
            }
            // 处理子进程收到的信号
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else 
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while ((pid == waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    continue;
                                }

                                break;
                            }

                            case SIGTERM:
                            case SIGINT:
                            {
                                m_stop = true;
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
            // 如果是其他可读数据，那么必然是客户请求到来，调用客户对象的process方法处理
            else if (events[i].events & EPOLLIN)
            {
                users[sockfd].process();
            }
            else 
            {
                continue;
            }
        }
    }
    
    delete [] users;
    users = NULL;
    close(pipefd);
    close(m_epollfd);
}


#endif

