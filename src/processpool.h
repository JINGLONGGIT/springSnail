#ifndef __PROCESSPOOL_H_
#define __PROCESSPOOL_H_

#include "global.h"
#include "fdwrapper.h"

// 子进程类
class CProcess
{
public:
    CProcess() : m_pid(-1){}

public:
    int m_busy_ratio;       // 子进程的繁忙程度(即负荷)
    pid_t m_pid;            // 目标子进程PID
    int m_pipefd[2];        // 父子进程之间通信的管道
};

// 进程池类
template <typename C, typename H, typename M>
class CProcesspool
{
private:
    CProcesspool(int listenfd, int process_number = 8);

public:
    static CProcesspool<C, H, M>* create(int listenfd, int process_number = 8)
    {
        if (!m_instance)
        {
            m_instance = new CProcesspool<C, H, M>(listenfd, process_number);
        }

        return m_instance;
    }

    ~CProcesspool()
    {
        delete [] m_sub_process;
    }

    void run(const vector<H>& arg);

private:
    void notify_parent_busy_ratio(int pipefd, M* manager);
    int get_most_free_srv();
    void setup_sig_pipe();
    void run_parent();
    void run_child(const vector<H>& arg);

private:
    static const int MAX_PROCESS_NUMBER = 16;       // 进程池允许的最大子进程数量
    static const int USER_PER_PROCESS = 65536;      // 每个子进程最多处理的客户端数量
    static const int MAX_EVENT_NUMBER = 10000;      // epoll最多监听的事件个数
    int m_process_number;                           // 进程池中的进程总数
    int m_idx;                                      // 子进程在进程池中的编号
    int m_epollfd;                                  // 内核事件表描述符
    int m_listenfd;                                 // 监听描述符
    int m_stop;                                     // 子进程通过m_stop决定是否停止
    CProcess* m_sub_process;                        // 进程池
    static CProcesspool<C, H, M>* m_instance;       // 进程池静态实例
};

template<typename C, typename H, typename M>
CProcesspool<C, H, M>* CProcesspool<C, H, M>::m_instance = NULL;

static int EPOLL_WAIT_TIME = 500;               // epoll_wait函数的超时值
static int sig_pipdfd[2];                       // 传输信号的管道：用于统一事件源

// 信号sig的信号处理函数
static void sig_handler(int sig)
{
    int sig_errno = errno;
    int msg = sig;
    send(sig_pipdfd[1], (char *)&msg, 1, 0);
    errno = sig_errno;
}

// 为信号sig注册信号处理函数
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
 * 函数名称：CProcesspool<C, H, M>::CProcesspool
 * 函数功能：进程池构造函数
 * 输入参数：int listenfd           监听描述符。必须在创建进程池之前被创建，否则子进程无法引用它
 *          int process_number      要创建的子进程的数量
 * 输出参数：无
 * 返 回 值：无
 **************************************************************/ 
template<typename C, typename H, typename M>
CProcesspool<C, H, M>::CProcesspool(int listenfd, int process_number)
    : m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false)
{
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));

    m_sub_process = new CProcess[process_number];
    assert(m_sub_process);

    // 创建process_number个子进程，并创建它们与父进程之间的管道
    for (int i = 0; i < process_number; i++)
    {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);

        if (m_sub_process[i].m_pid > 0)
        {
            close(m_sub_process[i].m_pipefd[1]);
            m_sub_process[i].m_busy_ratio = 0;
            continue;
        }
        else 
        {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

// 选取负荷最小的线程
template<typename C, typename H, typename M>
int CProcesspool<C, H, M>::get_most_free_srv()
{
    int ratio = m_sub_process[0].m_busy_ratio;
    int idx = 0;
    for (int i = 0; i < m_process_number; i++)
    {
        if (m_sub_process[i].m_busy_ratio < ratio)
        {
            idx = i;
            ratio = m_sub_process[i].m_busy_ratio;
        }
    }

    return idx;
}

// 统一事件源
template<typename C, typename H, typename M>
void CProcesspool<C, H, M>::setup_sig_pipe()
{
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipdfd);
    assert(ret != -1);

    setnonblocking(sig_pipdfd[1]);
    add_read_fd(m_epollfd, sig_pipdfd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

// 运行进程池
template<typename C, typename H, typename M>
void CProcesspool<C, H, M>::run(const vector<H>&arg)
{
    if (m_idx != -1)
    {
        run_child(arg);
        return ;
    }

    run_parent();
}

// 运行父进程
template<typename C, typename H, typename M>
void CProcesspool<C, H, M>::run_parent()
{
    setup_sig_pipe();

    for (int i = 0; i < m_process_number; i++)
    {
        add_read_fd(m_epollfd, m_sub_process[i].m_pipefd[0]);
    }
    
    add_read_fd(m_epollfd, m_listenfd);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, EPOLL_WAIT_TIME);
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
                // 新连接到来：选择负荷最小的线程来处理新到连接
                int idx = get_most_free_srv();
                send(m_sub_process[idx].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
            }
            // 处理父进程接收到的信号
            else if ((sockfd == sig_pipdfd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signal[1024];
                ret = recv(sig_pipdfd[0], signal, sizeof(signal), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else 
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signal[i])
                        {   
                            // 如果某个子进程退出
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while ((pid == waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    for (int i = 0; i < m_process_number; i++)
                                    {
                                        if (m_sub_process[i].m_pid == pid)
                                        {
                                            printf("child %d join\n", i);
                                            close(m_sub_process[i].m_pipefd[0]);
                                            m_sub_process[i].m_pid = -1;;
                                        }
                                    }
                                }

                                // 判断是否所有子进程退出
                                m_stop = true;
                                for (int i = 0; i < m_process_number; i++)
                                {
                                    if (m_sub_process[i].m_pid  != -1)
                                    {
                                        m_stop = false;
                                    }
                                }

                                break;
                            }

                            case SIGTERM:
                            case SIGINT:
                            {
                                // 父进程接收到终止信号，则杀死所有子进程
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
            else if (events[i].events & EPOLLIN)
            {
                int busy_ratio = 0;
                ret = recv(sockfd, (char*)&busy_ratio, sizeof(busy_ratio), 0);
                if (((ret < 0) && (errno != EAGAIN)) || ret == 0)
                {
                    continue;
                }

                for (int i = 0; i < m_process_number; i++)
                {
                    if (sockfd == m_sub_process[i].m_pipefd[0])
                    {
                        m_sub_process[i].m_busy_ratio = busy_ratio;
                        break;
                    }
                }

                continue;
            }
        }
    }

    for (int i = 0; i < m_process_number; i++)
    {
        closefd(m_epollfd, m_sub_process[i].m_pipefd[0]);
    }

    close(m_epollfd);
}

// 运行子进程
template<typename C, typename H, typename M>
void CProcesspool<C, H, M>::run_child(const vector<H>&arg)
{
    setup_sig_pipe();

    int pipefd_read = m_sub_process[m_idx].m_pipefd[1];
    add_read_fd(m_epollfd, pipefd_read);

    struct epoll_event events[MAX_EVENT_NUMBER];

    M* manager = new M(m_epollfd, arg[m_idx]);
    assert(manager);

    int number = 0;
    int ret = -1;

    while (!m_stop)
    {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, EPOLL_WAIT_TIME);
        if ((number < 0) && (errno != EINTR))
        {
            printf("epoll failed\n");
            break;
        }

        if (number == 0)
        {
            manager->recycle_conns();
            continue;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            if ((sockfd == pipefd_read) && (events[i].events & EPOLLIN))
            {
                int client;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EAGAIN)) || ret == 0)
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

                    add_read_fd(m_epollfd, connfd);

                    C* conn = manager->pick_conn(connfd);
                    if (!conn)
                    {
                        closefd(m_epollfd, connfd);
                        continue;
                    }

                    conn->init_clt(connfd, clnt_addr);
                    notify_parent_busy_ratio(pipefd_read, manager);
                }
            }
            else if ((sockfd == sig_pipdfd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipdfd[0], signals, sizeof(signals), 0);
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
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
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
            else if (events[i].events & EPOLLIN)
            {
                RET_CODE result = manager->process(sockfd, READ);
                switch (result)
                {
                    case CLOSED:
                    {
                        notify_parent_busy_ratio(pipefd_read, manager);
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                RET_CODE result = manager->process(sockfd, WRITE);
                switch (result)
                {
                    case CLOSED:
                    {
                        notify_parent_busy_ratio(pipefd_read, manager);
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
            }
            else 
            {
                continue;
            }
        }
    }

    close(pipefd_read);
    close(m_epollfd);
}

// 报告父进程繁忙程度
template<typename C, typename H, typename M>
void CProcesspool<C, H, M>::notify_parent_busy_ratio(int pipefd, M* manager)
{
    int msg = manager->get_used_conn_cnt();
    send(pipefd, (char*)&msg, 1, 0);
}

#endif

