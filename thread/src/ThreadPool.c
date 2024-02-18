#include "ThreadPool.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

/*
    threadPoolInit函数、threadPoolRun函数、takeWorkerEventLoop函数都需要
    在主线程里边进行依次调用。首先调用threadPoolInit函数得到pool实例，之后调用
    threadPoolRun函数启动线程池（意味着里边的子线程会启动）。接着调用takeWorkerEventLoop函数
    取出线程池中的某个子线程的反应堆实例，再把这个实例给到调用者，调用者就可以通过这个实例，
    往它的任务队列里边添加任务，这个任务添加到evLoop的任务队列里边去了之后，就开始处理
    任务队列。然后再根据任务队列里边的节点类型来处理Dispatcher的检测集合。有三种情况：
    情况1.往检测集合里边添加新的节点 
    情况2.从检测集合里边删除一个节点
    情况3.修改检测集合里边某个文件描述符对应的事件
    Dispatcher这个检测集合被处理完之后，这个反应堆模型开始进行一个循环，它需要循环调用底层的poll/epoll_wait/select
    函数来检测这个集合里边是否有激活的文件描述符。如果有激活的文件描述符，那么就通过这个文件描述符找到
    它所对应的channel。找到这个channel之后，再基于激活的事件调用事件对应的回调函数。这个函数调用完成
    之后，对应的事件也就处理完毕。这就走完了整个的处理流程
*/

// 初始化线程池（主线程）
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int threadNum) {
    struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
    pool->mainLoop = mainLoop; // 主线程的反应堆模型

    pool->index = 0;
    pool->isStart = false;
    pool->threadNum = threadNum; // 子线程总个数
    pool->workerThreads = (struct WorkerThread*)malloc(sizeof(struct WorkerThread) * threadNum);// 子线程数组
    return pool;
}

// 启动线程池 （主线程）
void threadPoolRun(struct ThreadPool* pool) {
    /*
        线程池被创建出来之后，接下来就需要让线程池运行起来，
        其实就是让线程池里的若干个子线程运行起来
    */
    // 确保线程池未运行  
    assert(pool && !pool->isStart);
    // 比较主线程的ID和当前线程ID是否相等 
    // 相等=>确保执行线程为主线程；不相等=>exit(0)
    if(pool->mainLoop->threadID != pthread_self()) {
        exit(0);
    }
    pool->isStart = true; // 标记为启动
    if(pool->threadNum > 0) { // 线程数量大于零
        for(int i=0;i<pool->threadNum;++i) {
            workerThreadInit(&pool->workerThreads[i], i);// 初始化子线程
            workerThreadRun(&pool->workerThreads[i]); // 启动子线程
        }
    }
}

/*
    这个函数是主线程调用的，因为主线程是拥有线程池的
    因此主线程可以遍历线程池里边的子线程，从中挑选
    一个子线程，得到它的反应堆模型，再将处理的任务放到
    反应堆模型里边
*/
// 取出线程池中的某个子线程的反应堆实例 
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool) {
    assert(pool->isStart); // 确保线程池是运行的
    // 比较主线程的ID和当前线程ID是否相等 
    // 相等=>确保执行线程为主线程；不相等=>exit(0)
    if(pool->mainLoop->threadID != pthread_self()) { 
        exit(0);
    }
    // 从线程池中找到一个子线程，然后取出里边的反应堆实例
    struct EventLoop* evLoop = pool->mainLoop; // 初始化
    if(pool->threadNum > 0) { // 线程数量大于零
        evLoop = pool->workerThreads[pool->index].evLoop;
        /*
            整个处理流程需要确保每个任务都能被雨露均沾地分配给各个子线程，
            避免所有任务都由同一个线程处理，还确保了index在合适的取值范围。
        */
        pool->index = ++pool->index % pool->threadNum;
    }
    return evLoop;
}