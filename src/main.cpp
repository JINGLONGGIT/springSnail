/*********************************************************************************
 * File Name: springSnail
 * Description: 小型负载均衡服务器
 * Author: jinglong
 * Date: 2020年5月7日 14:36
 * History: 
 *********************************************************************************/

#include "processpool.h"
#include "conn.h"
#include "mgr.h"

static const char* version = "1.0";

int main(int argc, char * argv [ ])
{
    vector<Chost> logical_srv;

    Chost tmp;
    strcpy(tmp.m_hostname, "127.0.0.1");
    tmp.m_port = 1234;

    logical_srv.push_back(tmp);

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

    CProcesspool<Conn, Chost, Cmgr>* pool = CProcesspool<Conn, Chost, Cmgr>::create(listenfd, logical_srv.size());
    if (pool)
    {
        pool->run(logical_srv);
        delete pool;
    }

    close(listenfd);
    return 0;
}

