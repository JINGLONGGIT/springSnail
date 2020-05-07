/*********************************************************************************
 * File Name: mgr.cpp
 * Description: 
 * Author: jinglong
 * Date: 2020年5月7日 14:36
 * History: 
 *********************************************************************************/

#include "mgr.h"

int Cmgr::m_epollfd = -1;

Cmgr::Cmgr(int epollfd, const Chost & srv) : m_logic_srv(srv)
{
    m_epollfd = epollfd;
    int ret = 0;

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, srv.m_hostname, &addr.sin_addr);
    addr.sin_port = htons(srv.m_port);
    printf("logical srv host info: (%s, %d)", srv.m_hostname, srv.m_port);

    for (int i = 0; i < srv.m_conncnt; i++)
    {
        sleep(1);
        int sockfd = conn2srv(addr);
        if (sockfd < 0)
        {
            printf("build connection %d failed\n", i);
        }
        else 
        {
            printf("build connection %d to server success\n", i);
            Conn* tmp = NULL;
            try
            {
                tmp = new Conn;
            }
            catch (...)
            {
                close(sockfd);
                continue;
            }
            tmp->init_srv(sockfd, addr);
            m_conns.insert(pair<int, Conn*>(sockfd, tmp));
        }
    }
}

Cmgr::~Cmgr()
{

}

int Cmgr::conn2srv(const sockaddr_in & addr)
{
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int Cmgr::get_used_conn_cnt()
{
    return m_used.size();
}

Conn* Cmgr::pick_conn(int cltfd)
{
    if (m_conns.empty())
    {
        printf("not enough srv connection to server\n");
        return NULL;
    }

    map<int, Conn*>::iterator iter = m_conns.begin();
    int srvfd = iter->first;
    Conn* tmp = iter->second;
    if (!tmp)
    {
        printf("empty server connection object\n");
        return NULL;
    }

    m_conns.erase(iter);
    m_used.insert(pair<int, Conn*>(cltfd, tmp));
    m_used.insert(pair<int, Conn*>(srvfd, tmp));
    add_read_fd(m_epollfd, srvfd);
    add_read_fd(m_epollfd, cltfd);

    printf("bind client sock %d with server sock %d\n", cltfd, srvfd);
    return tmp;
}

void Cmgr::free_conn(Conn * connection)
{
    int cltfd = connection->m_cltfd;
    int srvfd = connection->m_srvfd;
    closefd(m_epollfd, cltfd);
    closefd(m_epollfd, srvfd);
    m_used.erase(cltfd);
    m_used.erase(srvfd);
    connection->reset();
    m_freed.insert(pair<int, Conn*>(srvfd, connection));
}

void Cmgr::recycle_conns()
{
    if (m_freed.empty())
    {
        return;
    }

    for (map<int, Conn*>::iterator iter = m_freed.begin(); iter != m_freed.end(); iter++)
    {
        sleep(1);
        int srvfd = iter->first;
        Conn* tmp = iter->second;
        srvfd = conn2srv(tmp->m_srv_addr);
        if (srvfd < 0)
        {
            printf("fix connection failed\n");
        }
        else 
        {
            printf("fix connection success\n");
            tmp->init_srv(srvfd, tmp->m_srv_addr);
            m_conns.insert(pair<int, Conn*>(srvfd, tmp));
        }
    }

    m_freed.clear();
}

RET_CODE Cmgr::process(int fd, OP_TYPE type)
{
    Conn* connection = m_used[fd];
    if (!connection)
    {
        return NOTHING;
    }

    if (connection->m_cltfd == fd)
    {
        int srvfd = connection->m_srvfd;
        switch (type)
        {
            case READ:
            {
                RET_CODE res = connection->read_clt();
                switch (res)
                {
                    case OK:
                    {
                        printf("content read from client: %s", connection->m_clt_buf);
                        break;
                    }

                    case BUFFER_FULL:
                    {
                        modfd(m_epollfd, srvfd, EPOLLOUT);
                        break;
                    }

                    case IOERR:
                    case CLOSED:
                    {
                        free_conn(connection);
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }

                if (connection->m_srv_closed)
                {
                    free_conn(connection);
                    return CLOSED;
                }

                break;
            }

            case WRITE:
            {
                RET_CODE res = connection->write_clt();
                switch (res)
                {
                    case TRY_AGAIN:
                    {
                        modfd(m_epollfd, fd, EPOLLOUT);
                        break;
                    }

                    case BUFFER_EMPTY:
                    {
                        modfd(m_epollfd, srvfd, EPOLLOUT);
                        modfd(m_epollfd, fd, EPOLLIN);
                        break;
                    }

                    case IOERR:
                    case CLOSED:
                    {
                        free_conn(connection);
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }

                if (connection->m_srv_closed)
                {
                    free_conn(connection);
                    return CLOSED;
                }
                
                break;
            }

            default:
            {
                printf("other operation not support yet\n");
                break;
            }
        }
    }
    else if (connection->m_srvfd == fd)
    {
        int cltfd = connection->m_cltfd;
        switch( type )
        {
            case READ:
            {
                RET_CODE res = connection->read_srv();
                switch (res)
                {
                    case OK:
                    {
                        printf("content read from server: %s\n", connection->m_srv_buf);
                        break;
                    }

                    case BUFFER_FULL:
                    {
                        modfd(m_epollfd, cltfd, EPOLLOUT);
                        break;
                    }

                    case IOERR:
                    case CLOSED:
                    {
                        modfd(m_epollfd, cltfd, EPOLLOUT);
                        connection->m_srv_closed = true;
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }

                break;
            }
            case WRITE:
            {
                RET_CODE res = connection->write_srv();
                switch (res)
                {
                    case TRY_AGAIN:
                    {
                        modfd(m_epollfd, fd, EPOLLOUT);
                        break;
                    }

                    case BUFFER_EMPTY:
                    {
                        modfd(m_epollfd, cltfd, EPOLLIN);
                        modfd(m_epollfd, fd, EPOLLIN);
                        break;
                    }

                    case IOERR:
                    case CLOSED:
                    {
                        modfd(m_epollfd, cltfd, EPOLLOUT);
                        connection->m_srv_closed = true;
                        break;
                    }

                    default:
                    {
                        break;
                    }
                }

                break;
            }

            default:
            {
                printf("other operation not support yet\n");
                break;
            }
        }
    }
    else 
    {
        return NOTHING;
    }

    return OK;
}




