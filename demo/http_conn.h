#ifndef _HTTPCONNECTION_H_
#define _HTTPCONNECTION_H_

#include "global.h"
#include "locker.h"

enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

#ifdef NEVER
// HTTP请求方法
enum METHOD
{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATCH
};

// 主状态机当前状态
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,        // 当前正在分析请求行
    CHECK_STATE_HEADER,                 // 当前正在分析头部字段
    CHECK_STATE_CONTENT
};

// 服务器HTTP请求结果
enum HTTP_CODE
{
    NO_REQUEST,                 // 请求不完整，需要继续读取客户数据
    GET_REQUEST,                // 获得了一个完整的客户请求
    BAD_REQUEST,                // 客户请求有语法错误
    NO_RESOURCE,
    FORBIDDEN_REQUEST,          // 客户对资源没有足够的访问空间
    FILE_REQUEST,
    INTERNAL_ERROR,             // 服务器内部错误
    CLOSED_CONNECTION           // 客户端已关闭连接
};

// 行的读取状态
enum LINE_STATUS
{
    LINE_OK = 0,        // 读取到一个完整的行
    LINE_BAD,           // 行出错
    LINE_OPEN           // 行数据不完整
};
#endif /* NEVER */

// HTTP管理类
class http_conn
{
public:
    static const int FILENAME_LEN = 200;            // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;       // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;      // 写缓冲区的大小

public:
    http_conn(){}
    ~http_conn(){}

public:
    // 初始化新接收的连接
    void init(int sockfd, const sockaddr_in& addr);
    // 关闭连接
    void close_conn(bool real_close = true);
    // 非阻塞读操作
    bool read();
    // 非阻塞写操作
    bool write();
    // 处理客户请求: 由工作线程调用
    void process();

private:
    // 初始化连接
    void init();
    // 解析HTTP请求
    HTTP_CODE process_read();
    // 填充HTTP应答
    bool process_write(HTTP_CODE ret);

    // 下面这一组函数被process_read调用以分析HTTP对象
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char * text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line(){ return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    // 下面这一组函数被process_write调用以填充HTTP应答
    void unmap();
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_Linger();
    bool add_blank_line();

public:
    static int m_epollfd;               // 内核事件表描述符(所有的事件需要注册到该内核事件表中，所以定义为静态的)
    static int m_user_count;            // 用户数量

private:
    int m_sockfd;                               // 读取HTTP连接的socket
    sockaddr_in m_address;                      // 客户端socket地址
    char m_read_buf[READ_BUFFER_SIZE];          // 读缓冲区
    int m_read_idx;                             // 标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置
    int m_checked_idx;                          // 当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                           // 当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE];        // 写缓冲区
    int m_write_idx;                            // 写缓冲区中待发送的字节数

    CHECK_STATE m_check_state;                  // 主状态机当前所处的状态
    METHOD m_method;                            // 请求方法

    char m_real_file[FILENAME_LEN];             // 客户请求的目标文件的完整路径，其内容等于doc_root+m_url, doc_root是网站根目录
    char* m_url;                                // 客户请求目标文件的文件名
    char* m_version;                            // HTTP协议版本号，我们仅支持HTTP/1.1
    char* m_host;                               // 主机名
    int m_content_length;                       // HTTP请求的消息体长度
    bool m_linger;                              // HTTP请求是否要保持连接

    char* m_file_address;                       // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;                    // 目标文件的状态. 可以用来判断目标文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                       // 采用writev来执行写操作
    int m_iv_count;                             // 表示被写内存块的数量
};

#endif

