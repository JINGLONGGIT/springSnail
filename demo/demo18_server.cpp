/*****************************************************************
 * File Name: demo18_server.cpp
 * Description: 使用有限状态机实现HTTP请求的读取和分析
 * Author: jinglong
 * Date: 2020年4月28日 14:47
 * History: 
 *****************************************************************/

#include "global.h"

#define         BUFFER_SIZE         4096        // 读缓冲大小

// 主状态机当前状态
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,        // 当前正在分析请求行
    CHECK_STATE_HEADER                  // 当前正在分析头部字段
};

// 行的读取状态
enum LINE_STATUS
{
    LINE_OK = 0,        // 读取到一个完整的行
    LINE_BAD,           // 行出错
    LINE_OPEN           // 行数据不完整
};

// 服务器HTTP请求结果
enum HTTP_CODE
{
    NO_REQUEST,                 // 请求不完整，需要继续读取客户数据
    GET_REQUEST,                // 获得了一个完整的客户请求
    BAD_REQUEST,                // 客户请求有语法错误
    FROBIDDEN_REQUEST,          // 客户对资源没有足够的访问空间
    INTERNAL_ERROR,             // 服务器内部错误
    CLOSED_CONNECTION           // 客户端已关闭连接
};

static const char *szret[] = {"I get a correct result\n", "something wrong"};

// 从状态机，用于解析出一行内容
LINE_STATUS parse_line(char *buffer, int& checked_index, int& read_index)
{
    char temp;
    /******************************************************************
     * checked_index: 指向buffer中当前正在分析的字节
     * read_index：指向buffer中客户数据的尾部的下一字节
     * buffer中第0~checked_index字节都已经分析完毕，第checked_index~(read_index - 1)
     * 字节由下面的循环挨个分析
     ******************************************************************/
    for (; checked_index < read_index; ++checked_index)
    {
        temp = buffer[checked_index];       // 获取当前要分析的字节
        // 如果当前的字节是"\r"，即回车符，则说明可能读取到一个完整的行
        if (temp == '\r')
        {
            if ((checked_index + 1) == read_index)
            {
                return LINE_OPEN;
            }
            // 如果下一个字符是"\n", 则说明我们成功读取到一个完整的行
            else if (buffer[checked_index + 1] == '\n')
            {
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            // 否则说明客户发送的HTTP请求存在语法问题
            return LINE_BAD;
        }
        // 如果当前的字节是"\n", 即换行符，则也说明可能读取到一个完整的行
        else if (temp == '\n')
        {
            if ((checked_index > 1) && (buffer[checked_index - 1] == '\r'))
            {
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

// 分析请求行
HTTP_CODE parse_requestline(char* temp, CHECK_STATE& checkstate)
{
    char *url = strpbrk(temp, "\t");
    // 如果请求行中没有空白字符或"\t"字符，则HTTP请求必有问题
    if (!url)
    {
        return BAD_REQUEST;
    }

    *url++ = '\0';
    char* method = temp;
    // 仅支持GET方法
    if (strcasecmp(method, "GET") == 0)     
    {
        printf("The request method is GET\n");
    }
    else 
    {
        return BAD_REQUEST;
    }

    url += strspn(url, "\t");
    char *version = strpbrk(url, "\t");
    if (!version)
    {
        return BAD_REQUEST;
    }

    *version++ = '\0';
    version += strspn(version, "\t");
    // 仅支持HTTP/1.1方法
    if (strcasecmp(version, "HTTP/1.1") != 0)       
    {
        return BAD_REQUEST;
    }

    // 检查URL是否合法
    if (strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        url = strchr(url, '/');
    }

    if (!url || url[0] != '/')
    {
        return BAD_REQUEST;
    }

    printf("The request URL is: %s\n", url);
    // HTTP请求行处理完毕，状态转移到头部字段的分析
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 分析头部字段
HTTP_CODE parse_headers(char *temp)
{
    // 遇到一个空行，说明我们得到了一个正确的HTTP请求
    if (temp[0] == '\0')
    {
        return GET_REQUEST;
    }
    else if (strncasecmp(temp, "Host:", 5) == 0)
    {
        temp += 5;
        temp += strspn(temp, "\t");
        printf("the request host is: %s\n", temp);
    }
    else 
    {
        printf("I can not handle this header\n");
    }

    return NO_REQUEST;
}

// 分析HTTP请求的入口函数
HTTP_CODE parse_content(char *buffer, int& checked_index, CHECK_STATE& checkstate, int& read_index, int& start_line)
{
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;

    while ((linestatus = parse_line(buffer, checked_index, read_index)) == LINE_OK)
    {
        char* temp = buffer + start_line;       // start_line是行在buffer中的起始位置
        start_line = checked_index;             // 记录下一行的起始位置

        switch (checkstate)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                retcode = parse_requestline(temp, checkstate);
                if (retcode == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER:
            {
                retcode = parse_headers(temp);
                if (retcode == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if (retcode == GET_REQUEST)
                {
                    return GET_REQUEST;
                }
                break;
            }

            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }

    // 当前没有读取到一个完整的行，还需要继续读取客户数据才能进一步分析
    if (linestatus == LINE_OPEN)
    {
        return NO_REQUEST;
    }
    else
    {
        return BAD_REQUEST;
    }
}

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

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
    if (connfd < 0)
    {
        printf("errno is %d\n", errno);
    }
    else 
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', sizeof(buffer));
        int data_read = 0;
        int read_index = 0;             // 当前已经读取了多少字节的数据
        int checked_index = 0;          // 当前已经分析完多少字节的数据
        int start_line = 0;             // 行在buffer中的起始位置
        
        // 设置主状态机的初始状态
        CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
        while (1)
        {
            data_read = recv(connfd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if (data_read == -1)
            {
                printf("reading failed\n");
                break;
            }
            else if (data_read == 0)
            {
                printf("remote client has closed the connection\n");
                break;
            }

            read_index += data_read;

            // 分析目前已经获取的所有客户数据
            HTTP_CODE result = parse_content(buffer, checked_index, checkstate, read_index, start_line);

            if (result == NO_REQUEST)
            {
                continue;
            }
            else if (result == GET_REQUEST)
            {
                send(connfd, szret[0], strlen(szret[0]), 0);
                break;
            }
            else 
            {
                send(connfd, szret[1], strlen(szret[1]), 0);
                break;
            }
        }

        close(connfd);
    }

    close(listenfd);
    return 0;
}

