#ifndef __CONN_H_
#define __CONN_H_

#include "fdwrapper.h"

class Conn
{
public:
    Conn();
    ~Conn();

public:
    void init_clt(int sockfd, const sockaddr_in& clnt_addr);
    void init_srv(int sockfd, const sockaddr_in& srv_addr);
    void reset();
    RET_CODE read_clt();
    RET_CODE write_clt();
    RET_CODE read_srv();
    RET_CODE write_srv();

public:
    static const int BUFF_SIZE = 2048;
    
    char* m_clt_buf;                // 客户端缓冲区
    int m_clt_read_idx;             // 客户端已经接收的字节数
    int m_clt_write_idx;            // 客户端已经写入的字节数
    int m_cltfd;
    sockaddr_in m_clt_addr;

    char* m_srv_buf;                // 服务端缓冲区
    int m_srv_read_idx;
    int m_srv_write_idx;
    int m_srvfd;
    sockaddr_in m_srv_addr;

    bool m_srv_closed;
};

#endif
