#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <mutex>
#include <condition_variable>

#include "mysql_conn_pool.h"

template <typename T>
class ThreadPool
{
public:
    // threadCount 线程池中的线程数 maxRequestNum 请求队列中允许的最大请求数
    ThreadPool(mysql_conn_pool *connPool, int threadCount = 8, int maxRequestNum = 10000);
    ~ThreadPool();
    bool append(T *request);

private:
    static void *worker(void *arg);
    void run();

private:
    int g_threadCount;                        // 线程池中的线程数
    int g_maxRequestNum;                      // 请求队列中允许的最大请求数
    pthread_t *g_thread;                      // 描述线程池的数组
    std::list<T *> g_workQueue;               // 请求队列
    std::mutex g_queueMutex;                  // 互斥锁
    std::condition_variable g_queueCondition; // 是否有任务待处理
    bool g_stop;                              // 是否结束线程
    mysql_conn_pool *g_connPool;              // 数据库
};

template <typename T>
ThreadPool<T>::ThreadPool(mysql_conn_pool *connPool, int threadCount, int maxRequestNum)
    : g_threadCount(threadCount), g_maxRequestNum(maxRequestNum), g_thread(NULL), g_stop(false), g_connPool(connPool)
{
    if (threadCount <= 0 || maxRequestNum <= 0)
        throw std::exception();

    g_thread = new pthread_t[g_threadCount];
    if (!g_thread)
        throw std::exception();

    for (int i = 0; i < threadCount; ++i)
    {
        if (pthread_create(g_thread + i, NULL, worker, this) != 0)
        {
            delete[] g_thread;
            throw std::exception();
        }
        // 线程分离
        if (pthread_detach(g_thread[i]))
        {
            delete[] g_thread;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_stop = true;
    }
    g_queueCondition.notify_all();
    delete[] g_thread;
}

template <typename T>
bool ThreadPool<T>::append(T *request)
{
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_workQueue.size() > g_maxRequestNum)
            return false;
        g_workQueue.push_back(request);
    }

    g_queueCondition.notify_one();

    return true;
}

template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    ThreadPool *pool = static_cast<ThreadPool *>(arg);
    pool->run();
    return pool;
}

template <typename T>
void ThreadPool<T>::run()
{
    while (true)
    {
        T *request = NULL;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCondition.wait(lock, [this]
                                  { return g_stop || !g_workQueue.empty(); });
            if (g_stop && g_workQueue.empty())
                return;
            request = g_workQueue.front();
            g_workQueue.pop_front();
        }
        if (!request)
            continue;
        MYSQL *conn = nullptr;
        connRAII curConn(&conn, g_connPool);
        request->process(conn);
    }
}

#endif
