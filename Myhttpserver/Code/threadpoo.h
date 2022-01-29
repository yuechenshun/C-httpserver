#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <queue>
#include <pthread.h>
#include "webfunc.h"
#include "http.h"
using namespace std;

const int NUM = 2;

// 任务队列类
template <typename T>
class TaskQueue
{
public:
    TaskQueue() { pthread_mutex_init(&mutex, NULL); }
    ~TaskQueue() { pthread_mutex_destroy(&mutex); }
    // 向任务队列中添加任务
    void addTask(T *task)
    {
        pthread_mutex_lock(&mutex);
        TaskQ.push(task);
        pthread_mutex_unlock(&mutex);
    }
    // 获取任务队列中的任务
    T *takeTask()
    {
        pthread_mutex_lock(&mutex);
        if (TaskQ.empty())
        {
            printf("当前任务队列为空, 获取任务失败!\n");
            return NULL;
        }
        T* task = TaskQ.front();
        TaskQ.pop();
        pthread_mutex_unlock(&mutex);
        return task;
    }
    // 获取当前任务个数
    inline int taskNumber()
    {
        return TaskQ.size();
    }

private:
    queue<T *> TaskQ;
    pthread_mutex_t mutex;
};

// 线程池类
template <typename T>
class ThreadPool
{
public:
    ThreadPool(int min, int max);
    ~ThreadPool();

    void add(T *task);

private:
    TaskQueue<T> *taskQ;       // 任务队列
    pthread_t managerID;       // 管理者线程ID
    pthread_t *workerIDs;      // 工作线程ID数组
    int minNum;                // 线程池最小线程数
    int maxNum;                // 线程池最大线程数
    int busyNum;               // 线程池中正在工作的线程数
    int liveNum;               // 线程池中目前存活的线程数
    int exitNum;               // 需要销毁的线程数
    pthread_mutex_t mutexPool; // 线程池锁
    pthread_cond_t notEmpty;   // 条件变量,用于消费者

    bool shutdown;

private:
    static void *worker(void *arg);
    static void *manager(void *arg);
    void threadExit();
};

template <typename T>
ThreadPool<T>::ThreadPool(int min, int max) : minNum(min), maxNum(max),
                                              busyNum(0), liveNum(min),
                                              exitNum(0), shutdown(false)
{
    taskQ = new TaskQueue<T>;
    do
    {
        workerIDs = new pthread_t[max];
        if (workerIDs == NULL)
        {
            printf("工作线程数组创建失败\n");
            break;
        }
        memset(workerIDs, 0, sizeof(pthread_t) * max);

        if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
            pthread_cond_init(&notEmpty, NULL) != 0)
        {
            printf("线程池锁或条件变量初始化失败\n");
            break;
        }
        // 创建管理者线程
        int ret, count = 0;
        ret = pthread_create(&managerID, NULL, manager, this);
        if (ret != 0)
        {
            printf("管理线程创建失败!\n");
            break;
        }
        // 创建工作线程
        for (int i = 0; i < min; ++i)
        {
            ret = pthread_create(&workerIDs[i], NULL, worker, this);
            if (ret != 0)
            {
                printf("第%d个工作线程创建失败!\n", i + 1);
                break;
            }
            count++;
        }
        if (count < min - 1)
        {
            printf("工作线程数组创建失败!\n");
            break;
        }
        printf("线程池创建成功!\n");
        return;
    } while (false);
    printf("线程池创建失败!\n");
    if (workerIDs != NULL)
        delete[] workerIDs;
    if (taskQ != NULL)
        delete taskQ;
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    printf("开始销毁线程池\n");
    shutdown = true;
    // 回收管理者线程
    pthread_join(managerID, NULL);
    // 唤醒阻塞在条件变量上的工作线程并销毁
    for (int i = 0; i < liveNum; i++)
    {
        pthread_cond_signal(&notEmpty);
    }
    // 回收堆区资源
    if (workerIDs != NULL)
        delete[] workerIDs;
    if (taskQ != NULL)
        delete taskQ;
    // 销毁锁和条件变量
    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);
}

// 工作线程执行函数
template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    ThreadPool<T> *threadpool = static_cast<ThreadPool<T> *>(arg);
    while (true)
    {
        pthread_mutex_lock(&threadpool->mutexPool);
        // 任务队列为空,阻塞线程
        if (threadpool->taskQ->taskNumber() == 0 && !threadpool->shutdown)
        {
            pthread_cond_wait(&threadpool->notEmpty, &threadpool->mutexPool);
            if (threadpool->exitNum > 0)
            {
                printf("开始销毁线程...\n");
                threadpool->exitNum--;
                if (threadpool->liveNum > threadpool->minNum)
                {
                    threadpool->liveNum--;
                }
                pthread_mutex_unlock(&threadpool->mutexPool);
                threadpool->threadExit();
            }
        }
        // 如果线程池关闭则退出线程
        if (threadpool->shutdown)
        {
            pthread_mutex_unlock(&threadpool->mutexPool);
            threadpool->threadExit();
        }
        // 从任务队列中取出一个任务
        T *task = threadpool->taskQ->takeTask();
        threadpool->busyNum++;
        pthread_mutex_unlock(&threadpool->mutexPool);
        printf("线程%ld开始工作\n", pthread_self());

        // 执行任务
        task->work();
        delete task;
        printf("线程%ld结束工作\n", pthread_self());
        pthread_mutex_lock(&threadpool->mutexPool);
        threadpool->busyNum--;
        pthread_mutex_unlock(&threadpool->mutexPool);
    }
    return NULL;
}

// 管理者线程工作函数
template <typename T>
void *ThreadPool<T>::manager(void *arg)
{
    ThreadPool<T> *threadpool = static_cast<ThreadPool *>(arg);
    while (!threadpool->shutdown)
    {
        sleep(3);
    }
    // 取出线程池中的任务数量和当前线程数量、
    pthread_mutex_lock(&threadpool->mutexPool);
    int queueSize = threadpool->taskQ->taskNumber();
    int live = threadpool->liveNum;
    int busy = threadpool->busyNum;
    pthread_mutex_unlock(&threadpool->mutexPool);

    // 当任务个数大于活着的线程个数且小于最大线程数时,添加线程
    if (queueSize > live && live < threadpool->maxNum)
    {
        int count = 0;
        pthread_mutex_lock(&threadpool->mutexPool);
        for (int i = 0; count < NUM && threadpool->liveNum < threadpool->maxNum; i++)
        {
            if (threadpool->workerIDs[i] == 0)
            {
                pthread_create(&threadpool->workerIDs[i], NULL, worker, threadpool);
                count++;
                threadpool->liveNum++;
            }
        }
        pthread_mutex_unlock(&threadpool->mutexPool);
    }

    // 当工作线程数 * 2 小于存活线程数且大于最小线程数时销毁线程
    if (busy * 2 < live && live > threadpool->minNum)
    {
        pthread_mutex_lock(&threadpool->mutexPool);
        threadpool->exitNum = NUM;
        pthread_mutex_unlock(&threadpool->mutexPool);
        for (int i = 0; i < NUM; i++)
        {
            pthread_cond_signal(&threadpool->notEmpty);
        }
    }
    return NULL;
}

// 线程退出函数
template <typename T>
void ThreadPool<T>::threadExit()
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < this->maxNum; i++)
    {
        if (workerIDs[i] == tid)
        {
            workerIDs[i] = 0;
            printf("threadExit() 函数调用, %ld线程已经退出\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}

// 往线程池中添加任务
template <typename T>
void ThreadPool<T>::add(T *task)
{
    if (this->shutdown)
    {
        return;
    }
    // 添加任务并唤醒工作线程
    taskQ->addTask(task);
    pthread_cond_signal(&this->notEmpty);
}

#endif