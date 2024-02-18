#include "TcpConnection.h"
#include "HttpRequest.h"
#include <stdlib.h>
#include <stdio.h>
#include "Log.h"

// 接收客户端数据
int processRead(void* arg) {
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // 接收数据
    int count = bufferSocketRead(conn->readBuf,conn->channel->fd);
    Debug("接收到的http请求数据: %s",conn->readBuf->data + conn->readBuf->readPos);
    if(count > 0) {
        // 接收到了Http请求，解析Http请求
        int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO 
        writeEventEnable(conn->channel,true);
        eventLoopAddTask(conn->evLoop,conn->channel,MODIFY);
#endif
        bool flag = parseHttpRequest(conn->request,conn->readBuf,conn->response,conn->writeBuf,socket);
        if(!flag) {
            // 解析失败,回复一个简单的html
            char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
            bufferAppendString(conn->writeBuf,errMsg);
        }
    }
    else{
#ifdef MSG_SEND_AUTO
        // 断开连接
        eventLoopAddTask(conn->evLoop,conn->channel,DELETE);
#endif
    }
#ifndef MSG_SEND_AUTO
    // 断开连接
    eventLoopAddTask(conn->evLoop,conn->channel,DELETE);
#endif
    return 0;
}

int processWrite(void* arg) {
    Debug("开始发送数据了(基于写事件发送)......");
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // 发送数据
    int count = bufferSendData(conn->writeBuf,conn->channel->fd);
    if(count > 0) {
        // 判断数据是否被全部发送出去了
        if(bufferReadableSize(conn->writeBuf) == 0){
            // 1.不再检测写事件 -- 修改channel中保存的事件
            writeEventEnable(conn->channel,false);
            // 2.修改dispatcher检测的集合 -- 添加任务节点
            eventLoopAddTask(conn->evLoop,conn->channel,MODIFY);    
            // 3.删除这个节点
            eventLoopAddTask(conn->evLoop,conn->channel,DELETE);
        }
    }
    return 0;
}

// 初始化
struct TcpConnection* tcpConnectionInit(int fd,struct EventLoop* evLoop) {
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evLoop;
    struct Channel* channel = channelInit(fd,ReadEvent,processRead,processWrite,tcpConnectionDestroy,conn);
    conn->channel = channel;
    conn->readBuf = bufferInit(10240); // 10k
    conn->writeBuf = bufferInit(10240); // 10k
    sprintf(conn->name,"TcpConnection-%d",fd);

    // http协议
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();

    // 把channel添加到事件循环对应的任务队列里边
    eventLoopAddTask(evLoop,conn->channel,ADD);
    Debug("和客户端建立连接, threadName: %s, threadID:%ld, connName: %s",
        evLoop->threadName, evLoop->threadID, conn->name);
    return conn;
}

// 释放资源
int tcpConnectionDestroy(void* arg) {
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if(conn!=NULL) {
        if (conn->readBuf && bufferReadableSize(conn->readBuf) == 0 &&
            conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0) {
            destroyChannel(conn->evLoop,conn->channel);
            bufferDestroy(conn->readBuf);
            bufferDestroy(conn->writeBuf);
            httpRequestDestroy(conn->request);
            httpResponseDestroy(conn->response);
            free(conn);
        }
    }
    Debug("连接断开, 释放资源, gameover, connName: %s", conn->name);
    return 0;
}