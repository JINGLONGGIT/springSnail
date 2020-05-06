#ifndef __THREADPOOL_H_
#define __THREADPOOL_H_

// 半同步/半反应堆线程池
#include "global.h"
#include "locker.h"

// 线程池类
template<typename T>
class threadpool
{
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();

public:
    bool append(T* request);

private:
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;            // 线程池中的线程数
    int m_max_requests;             // 请求队列中允许的最大请求数
    pthread_t* m_threads;           // 描述线程组的数组，大小为m_thread_number
    std::list<T*> m_workqueue;      // 请求队列
    CLocker m_queuelocker;          // 保护请求队列的互斥锁
    CSem m_queuestat;               // 是否有任务需要处理
    bool m_stop;                    // 是否结束线程
};

/**************************************************************
 * 函数名称：threadpool<T>::threadpool
 * 函数功能：线程池构造函数
 * 输入参数: int thread_number     线程池中线程的数量
 *           int max_requests      请求队列中最多允许等待的请求的数量
 * 输出参数：无
 * 返 回 值：无
 **************************************************************/
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : 
    m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }

    // 创建m_thread_number个线程，并将它们都设置为脱离线程
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    for (int i = 0; i < m_thread_number; i++)
    {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete [] m_threads;
            throw std::exception();
        }

        // 设置为脱离线程
        if (pthread_detach(m_threads[i]))
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

// 往队列中添加任务
template<typename T>
bool threadpool<T>::append(T* request)
{
    // 操作工作队列时需要加锁，因为它被所有的线程共享
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 工作线程运行的函数，不断从工作队列中取出任务并执行
template<typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }

        request->process();
    }
}

#endif

