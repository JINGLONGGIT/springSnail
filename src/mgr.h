#ifndef __MGR_H_
#define __MGR_H_

#include "global.h"
#include "conn.h"
#include "fdwrapper.h"

class Chost
{
public:
    char m_hostname[1024];          // IP地址
    int m_port;                     // 端口号
    int m_conncnt;                  // 连接数量
};

class Cmgr
{
public:
    Cmgr(int epollfd, const Chost& srv);
    ~Cmgr();

public:
    int conn2srv(const sockaddr_in& addr);
    Conn* pick_conn(int cltfd);
    void free_conn(Conn* connection);
    int get_used_conn_cnt();
    void recycle_conns();
    RET_CODE process(int fd, OP_TYPE type);

private:
    static int m_epollfd;
    map<int, Conn*> m_conns;
    map<int, Conn*> m_used;
    map<int, Conn*> m_freed;
    Chost m_logic_srv;
};

#endif

