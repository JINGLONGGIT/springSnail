/*********************************************************************************
 * File Name: demo17_server.cpp
 * Description: 利用进程池(processpool.h)实现的高并发CGI服务器
 * Author: jinglong
 * Date: 2020年5月1日 14:36
 * History: 
 *********************************************************************************/

#include "processpool.h"

// 用于处理客户CGI请求的类，它可以作为processPoll类的模板参数
class cgi_conn
{
public:
    cgi_conn(){}
    ~cgi_conn(){}

public:
    // 初始化客户端连接, 清空缓冲区
    void init(int epollfd, int sockfd, const sockaddr_in& clnt_addr)
    {
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_addr = clnt_addr;
        memset(m_buf, '\0', sizeof(BUFFER_SIZE));
        m_read_idx = 0;
    }

    void process()
    {
        int idx = 0;
        int ret = -1;
        while (true)
        {
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
            if (ret < 0)
            {
                if (errno != EAGAIN)
                {
                    removefd(m_epollfd, m_sockfd);
                }
                break;
            }
            else if (ret == 0)
            {
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else 
            {
                m_read_idx += ret;
                printf("users content is: %s\n", m_buf);
                
                // 如果遇到字符'\r\n'，则开始处理客户请求
                for (; idx < m_read_idx; idx++)
                {
                    if ((idx >= 1) && (m_buf[idx - 1] == '\r') && (m_buf[idx] == '\n'))
                    {
                        break;
                    }
                }

                // 如果没有遇到'\r\n'， 则需要读取更多的数据
                if (idx == m_read_idx)
                {
                    continue;
                }

                m_buf[idx - 1] = '\0';

                // 判断客户要执行的CGI程序是否存在
                char *filename = m_buf;
                if (access(filename, F_OK) == -1)
                {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }

                // 创建子进程来执行CGI程序
                ret = fork();
                if (ret == -1)
                {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else if (ret > 0)
                {
                    // 父进程关闭连接
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else 
                {
                    // 子进程将标准输出定向到m_sockfd，并执行CGI程序
                    close(STDOUT_FILENO);
                    dup(m_sockfd);
                    execl(m_buf, m_buf, 0);
                    exit(0);
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;        // 读缓冲区的大小
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_addr;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;         // 标记缓冲区中已经读入的客户数据的最后一个字节的下一个位置
};

int cgi_conn::m_epollfd = -1;

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

    processPoll<cgi_conn>* pool = processPoll<cgi_conn>::create(listenfd);
    if (pool)
    {
        pool->run();
        delete pool;
    }

    close(listenfd);
    return 0;
}

