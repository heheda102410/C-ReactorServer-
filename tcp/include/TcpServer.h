#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

struct Listener {
    int lfd;
    unsigned short port;
};

struct TcpServer {
    struct Listener* listener; // 监听套接字
    struct EventLoop* mainLoop; // 主线程的事件循环（反应堆模型）
    struct ThreadPool* threadPool; // 线程池
    int threadNum; // 线程数量
};

// 初始化
struct TcpServer* tcpServerInit(unsigned short port,int threadNum);

// 初始化监听
struct Listener* listenerInit(unsigned short port);

// 启动服务器（不停检测有无客户端连接）
void tcpServerRun(struct TcpServer* server);