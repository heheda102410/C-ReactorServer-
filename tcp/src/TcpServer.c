#include "TcpServer.h"
#include "TcpConnection.h"
#include <arpa/inet.h> // 套接字函数的头文件
#include <stdio.h>
#include <stdlib.h>
#include "Log.h"

// 初始化
struct TcpServer* tcpServerInit(unsigned short port,int threadNum) {
    struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
    tcp->listener = listenerInit(port); // 创建listener
    tcp->mainLoop = eventLoopInit(); // 主线程的事件循环（反应堆模型）
    tcp->threadPool = threadPoolInit(tcp->mainLoop,threadNum); // 创建线程池
    tcp->threadNum = threadNum; // 线程数量
    return tcp;
}

// 初始化监听
struct Listener* listenerInit(unsigned short port) {
    // 创建一个Listner实例 -> listener
    struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));
    // 1.创建一个监听的文件描述符 -> lfd
    int lfd = socket(AF_INET,SOCK_STREAM,0); // AF_INET -> （网络层协议：Ipv4） ；SOCK_STREAM -> （传输层协议：流式协议）  ；0 -> :表示使用Tcp
    if(lfd == -1) {
        perror("socket");              
        return NULL;
    }   
    // 2.设置端口复用
    int opt = 1;
    int ret =  setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)); 
    if(ret == -1) {
        perror("setsockopt");
        return NULL;
    }
    // 3.绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);// 主机字节序（小端）转成网络字节序（大端） 端口的最大数量：2^16=65536
    addr.sin_addr.s_addr = INADDR_ANY;// 0.0.0.0 
    ret = bind(lfd,(struct sockaddr*)&addr,sizeof(addr));
    if(ret == -1) {
        perror("bind");
        return NULL;
    }
    // 4.设置监听
    ret = listen(lfd,128);
    if(ret == -1) {
        perror("listen");
        return NULL;
    }
    listener->lfd = lfd;
    listener->port = port;
    return listener;
}

int acceptConnection(void* arg) {
    struct TcpServer* server = (struct TcpServer*)arg;
    // 和客户端建立连接
    int cfd = accept(server->listener->lfd,NULL,NULL);
    if(cfd == -1) {
        perror("accept");
        return -1;
    }
    // 从线程池中去取出一个子线程的反应堆实例，去处理这个cfd
    struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);
    // 将cfd放到 TcpConnection中处理
    tcpConnectionInit(cfd, evLoop);// ...(已完，已补充)
    return 0;
}

// 启动服务器（不停检测有无客户端连接）
void tcpServerRun(struct TcpServer* server) {
    Debug("服务器程序已经启动了...");
    // 启动线程池
    threadPoolRun(server->threadPool);
    /*
        线程池启动起来之后，需要让它处理任务，对于当前的TcpServer
        来说，是有任务可以处理的。在当前服务器启动之后，需要处理的文件描述符
        有且只有一个，就是用于监听的文件描述符，因此需要把待检测的文件描述符（用于监听的）
        添加到（mainLoop）事件循环里边。
    */
    
    // 初始化一个channel实例
    struct Channel* channel = channelInit(server->listener->lfd,ReadEvent,acceptConnection,NULL,NULL,server);
    /*
        // 添加任务到任务队列
        int eventLoopAddTask(struct EventLoop* evLoop,struct Channel* channel,int type)
        如果把channel放到了mainLoop的任务队列里边，任务队列在处理的时候需要知道对这个节点做什么操作，
        是添加到检测集合里去，还是从检测集合里删除，还是修改检测集合里的文件描述符的事件
        那么对于监听的文件描述符，当然就是添加ADD
    */
    // 添加检测的任务 
    eventLoopAddTask(server->mainLoop,channel,ADD);
    // 启动反应堆模型
    eventLoopRun(server->mainLoop);
}