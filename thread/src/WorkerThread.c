#include "WorkerThread.h"
#include <stdio.h>
// 初始化
int workerThreadInit(struct WorkerThread* thread, int index) {
    thread->threadID = 0;// 线程ID
    sprintf(thread->name, "SubThread-%d", index);// 线程名字

    // 指定为NULL，表示使用默认属性
    pthread_mutex_init(&thread->mutex, NULL);// 互斥锁
    pthread_cond_init(&thread->cond, NULL);// 条件变量

    thread->evLoop = NULL;// 事件循环(反应堆模型)
    return 0;
}

// 子线程的回调函数
void* subThreadRunning(void* arg) {
    struct WorkerThread* thread = (struct WorkerThread*)arg;
    // 还有子线程里边的evLoop是共享资源，需要添加互斥锁
    pthread_mutex_lock(&thread->mutex);// 加锁
    thread->evLoop = eventLoopInitEx(thread->name);
    pthread_mutex_unlock(&thread->mutex);// 解锁

    pthread_cond_signal(&thread->cond);// 发送信号（唤醒主线程，通知主线程解除阻塞）
    eventLoopRun(thread->evLoop);// 启动反应堆模型
    return NULL;
}

// 启动线程
void workerThreadRun(struct WorkerThread* thread) {
    // 创建子线程
    pthread_create(&thread->threadID, NULL, subThreadRunning, thread);

    /*
        在这里阻塞主线程的原因是：在于子线程的反应堆模型是否被真正的创建出来了？
        因此，可以判断一下thread->evLoop是否为NULL，如果等于NULL，说明
        子线程反应堆模型还没有被初始化完毕，没有初始化完毕，我们就阻塞主线程
    */
    // 阻塞主线程，让当前函数不会直接结束
    pthread_mutex_lock(&thread->mutex);
    while(thread->evLoop == NULL) { // 多次判断
        pthread_cond_wait(&thread->cond, &thread->mutex);// 子线程的回调函数（subThreadRunning）里调用pthread_cond_signal(&thread->cond)可以解除这里的阻塞
    }
    pthread_mutex_unlock(&thread->mutex);
}