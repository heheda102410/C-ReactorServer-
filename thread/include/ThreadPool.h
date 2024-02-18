#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>

// 定义线程池
struct ThreadPool {
    /*
        在线程池里边的这个mainLoop主要是做备份用的，主线程里边的mainLoop主要负责和
        客户端建立连接，只负责这一件事情，只有当线程池没有子线程这种情况下，mainLoop
        才负责处理和客户端的连接，否则的话它是不管其他事情的。
    */
    struct EventLoop* mainLoop; // 主线程的反应堆模型

    int index; 
    bool isStart;
    int threadNum; // 子线程总个数
    struct WorkerThread* workerThreads;
};

// 初始化线程池
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int threadNum);

// 启动线程池 
void threadPoolRun(struct ThreadPool* pool);

// 取出线程池中的某个子线程的反应堆实例
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);