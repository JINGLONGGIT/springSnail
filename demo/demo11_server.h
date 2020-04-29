#ifndef __DEMO11_SERVER_H_
#define __DEMO11_SERVER_H_

// 升序链表定时器
#include "global.h"

#define     BUFFER_SIZE     64

class util_timer;

// 用户数据结构
struct client_data
{
    sockaddr_in addr;               // 客户端socket地址
    int sockfd;                     // 连接socket描述符
    char buf[BUFFER_SIZE];          // 读缓存
    util_timer* timer;              // 定时器
};

// 定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;                       // 任务超时时间：绝对时间
    void (*cb_func)(client_data*);       // 任务回调函数
    client_data* user_data;              // 回调函数需要处理的用户数据
    util_timer* prev;                    // 指向前一个定时器
    util_timer* next;                    // 指向下一个定时器
};

// 定时器链表：升序双向链表，且带有头结点和尾节点
class sort_timer_lst
{
public:
    sort_timer_lst() : head(NULL), tail(NULL){}

    // 链表被销毁时，需要删除所有的定时器
    ~sort_timer_lst()
    {
        util_timer* tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    // 将目标定时器timer添加到定时器链表中
    void add_timer(util_timer* timer)
    {
        if (!timer)
        {
            return ;
        }

        if (!head)
        {
            head = tail = timer;
            return ;
        }

        /*******************************************************************************
         * 如果目标定时器的超时时间小于当前链表中所有定时器的超时时间，就把该定时器
         * 插入到链表头部，作为定时器链表的新的头结点。否则就需要调用重载函数
         * add_timer(util_timer* timer, util_timer* lst_head)
         * 将目标定时器插入到链表中合适的位置，以保证链表的升序特性
         *******************************************************************************/
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return ;
        }

        add_timer(timer, head);
    }

    // 调整定时器: 将定时器的任务发生变化时，需要调整定时器在链表中的位置(该函数只考虑被调整的定时器的超时时间被延长的情况)
    void adjust_timer(util_timer* timer)
    {
        if (!timer)
        {
            return ;
        }

        util_timer* tmp = timer->next;
        // 如果需要调整的定时器在链表尾部，或者该定时器的超时时间仍然小于下一个
        // 定时器的超时时间，则不用调整
        if (!tmp || (timer->expire < tmp->expire))
        {
            return ;
        }

        // 如果需要调整的定时器是链表的头结点，则需要将该定时器取出并重新插入链表
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else 
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    // 将目标定时器timer从链表中删除
    void del_timer(util_timer* timer)
    {
        if (!timer)
        {
            return;
        }

        // 链表中只有一个元素，即timer
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return ;
        }

        // 需要删除的定时器是链表头结点
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return ;
        }

        // 需要删除的定时器是链表尾节点
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return ;
        }

        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    // 心搏函数: SIGALRM信号每被触发一次就会在其信号处理函数中执行一次tick()函数
    void tick()
    {
        if (!head)
        {
            return ;
        }

        printf("timer tick\n");
        time_t cur = time(NULL);            // 获取系统当前时间
        util_timer* tmp = head;

        // 定时器的核心处理逻辑：从头到尾处理每一个定时器，直到遇到一个尚未到期的定时器
        while (tmp)
        {
            if (cur < tmp->expire)
            {
                break;
            }

            // 调用定时器回调函数
            tmp->cb_func(tmp->user_data);

            head = tmp->next;
            if (!head)
            {
                head->prev = NULL;
            }

            delete tmp;
            tmp = head;
        }
    }

private:
    // 将目标定时器添加到节点lst_head后面的链表中
    void add_timer(util_timer* timer, util_timer* lst_head)
    {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;

        while (tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }

            prev = tmp;
            tmp = tmp->next;
        }

        // 插入尾部作为尾节点
        if (!tmp)
        {
            prev->next = timer;
            timer->next = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    util_timer* head;
    util_timer* tail;
};

#endif

