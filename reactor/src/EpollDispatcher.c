#include "Dispatcher.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
 
#define Max 520
 
struct EpollData {
    int epfd;//epoll树的根节点
    struct epoll_event* events;// 数组指针
};
 
static void* epollInit();
static int epollAdd(struct Channel* channel,struct EventLoop* evLoop);
static int epollRemove(struct Channel* channel,struct EventLoop* evLoop);
static int epollModify(struct Channel* channel,struct EventLoop* evLoop);
static int epollDispatch(struct EventLoop* evLoop,int timeout); // 单位:s
static int epollClear(struct EventLoop* evLoop);
static int epollCtl(struct Channel* channel,struct EventLoop* evLoop,int op);
 
// EpollDispatcher 是 Dispatcher 的一个实例
struct Dispatcher EpollDispatcher = {
    // 把函数地址指定给它
    epollInit,
    epollAdd,
    epollRemove,
    epollModify,
    epollDispatch,
    epollClear
};
 
static void* epollInit() {
    struct EpollData* data = (struct EpollData*)malloc(sizeof(struct EpollData));
    data->epfd = epoll_create(10);
    if(data->epfd == -1) {
        perror("epoll_create");
        exit(0);
    }
    data->events=(struct epoll_event*)calloc(Max,sizeof(struct epoll_event));
    return data;
}
 
static int epollCtl(struct Channel* channel,struct EventLoop* evLoop,int op) {
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    struct epoll_event ev;
    // 先把要检测的文件描述符存储到类型为epoll_event 的ev中
    ev.data.fd = channel->fd;
    // 指定要检测的这个fd的事件
    /*
        channel里边的事件，它的读和写对应的值使我们自己定义的
        epoll里边的，EPOLLIN和EPOLLOUT是linux操作系统定义的
    */
    int events = 0;
    if(channel->events & ReadEvent) {
        events |= EPOLLIN;
    }
    if(channel->events & WriteEvent) {
        events |= EPOLLOUT;
    }
    ev.events = events;
    int ret = epoll_ctl(data->epfd,op,channel->fd,&ev);
    return ret;
}
 
static int epollAdd(struct Channel* channel,struct EventLoop* evLoop) {
    int ret = epollCtl(channel,evLoop,EPOLL_CTL_ADD);
    if(ret == -1) {
        perror("epoll_ctl add");
        exit(0);
    }
    return ret;
}
static int epollRemove(struct Channel* channel,struct EventLoop* evLoop) {
    int ret = epollCtl(channel,evLoop,EPOLL_CTL_DEL);
    if(ret == -1) {
        perror("epoll_ctl delete");
        exit(0);
    }
    // 通过 channel 释放对应的 TcpConnection 资源
    channel->destroyCallback(channel->arg); // (已续写)
    return ret;
}
static int epollModify(struct Channel* channel,struct EventLoop* evLoop) {
    int ret = epollCtl(channel,evLoop,EPOLL_CTL_MOD);
    if(ret == -1) {
        perror("epoll_ctl modify");
        exit(0);
    }
    return ret;
}
static int epollDispatch(struct EventLoop* evLoop,int timeout) {
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    int count = epoll_wait(data->epfd,data->events,Max,timeout * 1000);
    for(int i=0;i<count;++i) {
        int events = data->events[i].events;
        int fd = data->events[i].data.fd;
        if(events & EPOLLERR || events & EPOLLHUP) {
            // 对方断开了连接，删除 fd
            // epollRemove(Channel, evLoop);
            continue;
        }
        if(events & EPOLLIN) {
            // 已续写...
            eventActivate(evLoop,fd,ReadEvent);
        }
        if(events & EPOLLOUT) {
            // 已续写...
            eventActivate(evLoop,fd,WriteEvent);
        }
    }
    return 0;
}
static int epollClear(struct EventLoop* evLoop) {
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    free(data->events);
    close(data->epfd);
    free(data);
    return 0;
}