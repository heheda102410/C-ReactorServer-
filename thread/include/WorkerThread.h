#pragma once
#include <pthread.h>
#include "EventLoop.h"


/*
    工作线程：
        线程ID，
        线程名字(可选)，
        互斥锁（线程同步），
        条件变量（线程阻塞），
        EventLoop（在每个子线程里边都有一个反应堆模型）
*/

// 定义子线程对应的结构体
struct WorkerThread {
    pthread_t threadID;// 线程ID
    char name[24];// 线程名字
    pthread_mutex_t mutex;// 互斥锁（线程同步）
    pthread_cond_t cond;// 条件变量（线程阻塞）
    struct EventLoop* evLoop;// 事件循环(反应堆模型)
    // 在每个子线程里边都有一个反应堆模型
};

// 初始化
int workerThreadInit(struct WorkerThread* thread, int index);

// 启动线程
void workerThreadRun(struct WorkerThread* thread);