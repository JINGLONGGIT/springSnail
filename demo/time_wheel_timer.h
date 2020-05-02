#ifndef __DEMO12_SERVER_H_
#define __DEMO12_SERVER_H_

// 时间轮定时器
#include "global.h"

#define         BUFFER_SIZE         64

class tw_timer;

// 用户数据结构
struct client_data
{
    sockaddr_in addr;               // 客户端socket地址
    int sockfd;                     // 连接socket描述符
    char buf[BUFFER_SIZE];          // 读缓存
    tw_timer* timer;                // 定时器
};

// 定时器类
class tw_timer
{
public:
    tw_timer(int rot, int ts) : next(NULL), prev(NULL), rotation(rot), time_slot(ts){}

public:
    int rotation;                           // 记录定时器在时间轮转多少圈后生效
    int time_slot;                          // 记录定时器属于时间轮上的哪个槽
    void (*cb_func)(client_data*);          // 定时器回调函数
    client_data* user_data;                 // 客户数据
    tw_timer* next;                         // 指向下一个定时器
    tw_timer* prev;                         // 指向前一个定时器
};

// 时间轮
class time_wheel
{
public:
    time_wheel() : cur_slot(0)
    {
        // 初始化时间轮上每个槽的头结点
        for (int i = 0; i < N; i++)
        {
            slots[i] = NULL;
        }
    }

    ~time_wheel()
    {
        // 遍历每个槽，并销毁其中的定时器
        for (int i = 0; i < N; i++)
        {
            tw_timer* tmp = slots[i];
            while (tmp)
            {
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    // 根据定时值timeout创建一个定时器，并把它插入合适的槽中
    tw_timer* add_timer(int timeout)
    {
        if (timeout < 0)
        {
            return NULL;
        }

        int ticks = 0;          // 定时器的滴答数
        /******************************************************************************
         * 根据待插入定时器的超时值计算它将在时间轮中转动多少个滴答后被触发，并将该滴答
         * 数存储于变量ticks中。如果待插入定时器的超时值小于时间轮的槽间隔SI，则将ticks
         * 向上折合为1，否则就将ticks向下折合为timeout/SI
         ******************************************************************************/
        if (timeout < SI)
        {
            ticks = 1;
        }
        else 
        {
            ticks = timeout / SI;
        }

        int rotation = ticks / N;       // 计算待插入的定时器在时间轮转动多少圈后被触发
        int ts = (cur_slot + ticks % N) % N;        // 计算待触发的定时器应该被插入到哪个槽

        // 创建新的定时器，它在时间轮转动rotation圈后被触发，且位于第ts个槽上
        tw_timer* timer = new tw_timer(rotation, ts);

        // 如果第ts个槽中尚无任何定时器，则把新建的定时器插入其中，并将该定时器设置为该槽的头结点
        if (!slots[ts])
        {
            printf("add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot);
            slots[ts] = timer;
        }
        else 
        {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }

        return timer;
    }

    // 删除目标定时器timer
    void del_timer(tw_timer* timer)
    {
        if (!timer)
        {
            return ;
        }

        int ts = timer->time_slot;
        if (timer == slots[ts])
        {
            slots[ts] = slots[ts]->next;
            if (slots[ts])
            {
                slots[ts]->prev = NULL;
            }

            delete timer;
        }
        else 
        {
            timer->prev->next = timer->next;
            if (timer->next)
            {
                timer->next->prev = timer->prev;
            }

            delete timer;
        }
    }

    // SI时间到后调用该函数，时间轮向前滚动一个槽的间隔
    void ticks()
    {
        tw_timer* tmp = slots[cur_slot];
        printf("current slot is %d\n", cur_slot);
        while (tmp)
        {
            printf("tick the timer once\n");
            // 如果定时器的rotation值大于0，则它在这一轮不起作用
            if (tmp->rotation > 0)
            {
                tmp->rotation--;
                tmp = tmp->next;
            }
            else 
            {
                // 执行定时任务，并删除定时器
                tmp->cb_func(tmp->user_data);
                if (tmp == slots[cur_slot])
                {
                    printf("delete header in cur_slot\n");
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if (slots[cur_slot])
                    {
                        slots[cur_slot]->prev = NULL;
                    }
                    tmp = slots[cur_slot];
                }
                else 
                {
                    tmp->prev->next = tmp->next;
                    if (tmp->next)
                    {
                        tmp->next->prev = tmp->prev;
                    }
                    tw_timer* tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }

        // 更新时间轮的当前槽，以反映时间轮的转动
        cur_slot = ++cur_slot % N;
    }

private:
    static const int N = 60;            // 时间轮上的槽数目
    static const int SI = 1;            // 槽时间间隔为1s，即每1秒时间轮转一次
    tw_timer* slots[N];                 // 时间轮的槽，其中每个元素指向一个定时器链表，链表无序
    int cur_slot;                       // 时间轮的当前槽
};

#endif

