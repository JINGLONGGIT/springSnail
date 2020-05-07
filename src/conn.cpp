/*********************************************************************************
 * File Name: conn.cpp
 * Description: 客户数据读写操作
 * Author: jinglong
 * Date: 2020年5月7日 14:36
 * History: 
 *********************************************************************************/

#include "conn.h"

Conn::Conn()
{
    m_srvfd = -1;
    // 建立客户端缓冲区
    m_clt_buf = new char[BUFF_SIZE];
    if (!m_clt_buf)
    {
        throw std::exception();
    }

    // 建立服务端缓冲区
    m_srv_buf = new char[BUFF_SIZE];
    if (!m_srv_buf)
    {
        throw std::exception();
    }

    reset();
}

Conn::~Conn()
{
    delete [] m_clt_buf;
    delete [] m_srv_buf;
}

void Conn::reset()
{
    m_clt_read_idx = 0;
    m_clt_write_idx = 0;
    m_srv_read_idx = 0;
    m_srv_write_idx = 0;
    m_srv_closed = false;
    m_cltfd = -1;
    memset(m_clt_buf, '\0', sizeof(m_clt_buf));
    memset(m_srv_buf, '\0', sizeof(m_srv_buf));
}

void Conn::init_clt(int sockfd, const sockaddr_in & clnt_addr)
{
    m_cltfd = sockfd;
    m_clt_addr = clnt_addr;
}

void Conn::init_srv(int sockfd, const sockaddr_in & srv_addr)
{
    m_srvfd = sockfd;
    m_srv_addr = srv_addr;
}

// 读取客户端数据
RET_CODE Conn::read_clt()
{
    int bytes_read = 0;
    while (true)
    {
        if (m_clt_read_idx >= BUFF_SIZE)
        {
            printf("the client read buffer is full, let server write\n");
            return BUFFER_FULL;
        }

        bytes_read = recv(m_cltfd, m_clt_buf + m_clt_read_idx, BUFF_SIZE - m_clt_read_idx, 0);
        if (bytes_read == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                break;
            }
         
            return IOERR;
        }
        else if (bytes_read == 0)
        {
            return CLOSED;
        }

        m_clt_read_idx += bytes_read;
    }

    return ((m_clt_read_idx - m_clt_write_idx) > 0) ? OK : NOTHING;
}

// 读取服务端数据
RET_CODE Conn::read_srv()
{
    int bytes_read = 0;
    while (true)
    {
        if (m_srv_read_idx >= BUFF_SIZE)
        {
            printf("the server read buffer is full, let client write\n");
            return BUFFER_FULL;
        }

        bytes_read = recv(m_srvfd, m_srv_buf + m_srv_read_idx, BUFF_SIZE - m_srv_read_idx, 0);
        if (bytes_read == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                break;
            }

            return IOERR;
        }
        else if (bytes_read == 0)
        {
            printf("the server should not close the persist connection\n");
            return CLOSED;
        }

        m_srv_read_idx += bytes_read;
    }

    return ((m_srv_read_idx - m_srv_write_idx) > 0) ? OK : NOTHING;
}

RET_CODE Conn::write_clt()
{
    int bytes_write = 0;
    while (true)
    {
        if (m_srv_read_idx <= m_srv_write_idx)
        {
            m_srv_read_idx = 0;
            m_srv_write_idx = 0;
            return BUFFER_EMPTY;
        }

        bytes_write = send(m_cltfd, m_srv_buf + m_srv_write_idx, m_srv_read_idx - m_srv_write_idx, 0);
        if (bytes_write == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                return TRY_AGAIN;
            }
            
            printf("write client socket failed\n");
            return IOERR;
        }
        else if (bytes_write == 0)
        {
            return CLOSED;
        }

        m_srv_write_idx += bytes_write;
    }
}

RET_CODE Conn::write_srv()
{
    int bytes_write = 0;
    while (true)
    {
        if (m_clt_read_idx <= m_clt_write_idx)
        {
            m_clt_read_idx = 0;
            m_clt_write_idx = 0;
            return BUFFER_EMPTY;
        }

        bytes_write = send(m_srvfd, m_clt_buf + m_clt_write_idx, m_clt_read_idx - m_clt_write_idx, 0);
        if (bytes_write == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                return TRY_AGAIN;
            }
            
            printf("write server socket failed\n");
            return IOERR;
        }
        else if (bytes_write == 0)
        {
            return CLOSED;
        }

        m_clt_write_idx += bytes_write;
    }
}

