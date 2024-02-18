#include "Dispatcher.h"
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
/*
    对于poll来说，它的最大上限并不是1024，select的最大上限才是1024
    poll的文件描述符的最大上限取决于当前操作系统的配置，你的内存越大，
    那么它能检测的文件描述符得到数量就越多。
    但是有一点要说明，如果你让poll检测了很多个文件描述符，那么它的效率
    是非常低的，就是检测的文件描述符的数量越多，它的效率就越低
    如果你要检测的文件描述符特别多，建议用epoll,因为不管检测的文件描述符
    是上万还是上千，其实它的效率都是非常高的
    那什么时候使用select或者polls呢？其实是在需要检测的文件描述的数量并不多，
    且它们大多都是在激活的状态下，此时，效率是最高的
    总结为两点：
        1.检测的文件描述符数量并不多
        2.待检测出的文件描述符，大多都是激活的
    什么叫激活呢？假如说你登录了qq，你一天不说话，这叫激活吗？
    这不叫啊，就是如果你一天都在处于聊天状态。这叫激活所谓的聊天状态，
    就是进行数据的接收和发送。如果你只在这儿挂着，说明你只是保持了一个
    在线状态，你并没有做其他的事情，你现在是占用了网络资源。所以这种情况
    ，不算激活。
*/
#define Max 1024 
struct PollData {
    int maxfd;
    struct pollfd fds[Max];
};
 
static void* pollInit();
static int pollAdd(struct Channel* channel,struct EventLoop* evLoop);
static int pollRemove(struct Channel* channel,struct EventLoop* evLoop);
static int pollModify(struct Channel* channel,struct EventLoop* evLoop);
static int pollDispatch(struct EventLoop* evLoop,int timeout); // 单位:s
static int pollClear(struct EventLoop* evLoop);
 
// PollDispatcher 是 Dispatcher 的一个实例
struct Dispatcher PollDispatcher = {
    // 把函数地址指定给它
    pollInit,
    pollAdd,
    pollRemove,
    pollModify,
    pollDispatch,
    pollClear
};
 
static void* pollInit() {
    struct PollData* data = (struct PollData*)malloc(sizeof(struct PollData));
    data->maxfd = 0;
    for (int i = 0; i < Max; ++i) {
        data->fds[i].fd = -1;
        data->fds[i].events = 0;
        data->fds[i].revents = 0;
    }
    return data;
}
 
static int pollAdd(struct Channel* channel,struct EventLoop* evLoop) {
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int events = 0;
    if (channel->events & ReadEvent) {
        events |= POLLIN;
    }
    if (channel->events & WriteEvent) {
        events |= POLLOUT;
    }
    int i=0;
    for(;i<Max;++i) {
        if(data->fds[i].fd == -1) {
            data->fds[i].fd = channel->fd;
            data->fds[i].events = events;
            data->maxfd = i > data->maxfd ? i : data->maxfd;
            break;
        }
    }
    if(i >= Max) {
        return -1;
    }
    return 0;
}
 
static int pollRemove(struct Channel* channel,struct EventLoop* evLoop) {
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int i=0;
    for(;i<Max;++i) {
        if(data->fds[i].fd == channel->fd) {
            data->fds[i].fd = -1;
            data->fds[i].events = 0;
            data->fds[i].revents = 0;
            break;
        }
    }
    // 通过 channel 释放对应的 TcpConnection 资源
    channel->destroyCallback(channel->arg); // (已续写)
    
    if(i >= Max) {
        return -1;
    }
    return 0;
}
static int pollModify(struct Channel* channel,struct EventLoop* evLoop) {
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int events = 0;
    if (channel->events & ReadEvent) {
        events |= POLLIN;
    }
    if (channel->events & WriteEvent) {
        events |= POLLOUT;
    }
    int i=0;
    for(;i<Max;++i) {
        if(data->fds[i].fd == channel->fd) {
            data->fds[i].events = events;
            break;
        }
    }
    if(i >= Max) {
        return -1;
    }
    return 0;
}
static int pollDispatch(struct EventLoop* evLoop,int timeout) {
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    int count = poll(data->fds,data->maxfd + 1,timeout * 1000);
    if(count == -1) {
        perror("poll");
        exit(0);
    }
    for(int i=0;i<=data->maxfd;++i) {
        if(data->fds[i].fd == -1) {
            continue;
        }
        if(data->fds[i].revents & POLLIN) {
            // 已续写...
            eventActivate(evLoop,data->fds[i].fd,ReadEvent);
        }
        if(data->fds[i].revents & POLLOUT) {
            // 已续写...
            eventActivate(evLoop,data->fds[i].fd,WriteEvent);
        }
    }
    return 0;
}
static int pollClear(struct EventLoop* evLoop) {
    struct PollData* data = (struct PollData*)evLoop->dispatcherData;
    free(data);
    return 0;
}