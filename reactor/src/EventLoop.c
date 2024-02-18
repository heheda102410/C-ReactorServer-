#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// 初始化
struct EventLoop* eventLoopInit() {
    return eventLoopInitEx(NULL);
}

// 写数据
void taskWakeup(struct EventLoop* evLoop) {
    const char* msg = "我是要成为海贼王的男人！";
    write(evLoop->socketPair[0],msg,strlen(msg));
}

// 读数据
int readLocalMessage(void* arg) {
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    char buffer[256];
    read(evLoop->socketPair[1],buffer,sizeof(buffer));
    return 0;
}

struct EventLoop* eventLoopInitEx(const char* threadName) {
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false; // 没有运行
    // evLoop->dispatcher = &EpollDispatcher;
    evLoop->dispatcher = &PollDispatcher;
    // evLoop->dispatcher = &SelectDispatcher;
    evLoop->dispatcherData = evLoop->dispatcher->init(); 
    
    // 任务队列(链表)
    evLoop->head = evLoop->tail = NULL;

    // 用于存储channel的map
    evLoop->channelMap = channelMapInit(128);

    evLoop->threadID = pthread_self(); // 当前线程ID
    strcpy(evLoop->threadName,threadName == NULL ? "MainThread" : threadName); // 线程的名字
    pthread_mutex_init(&evLoop->mutex, NULL); 

    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if(ret == -1) {
        perror("socketpair");
        exit(0);
    }
    // 指定规则：evLoop->socketPair[0] 发送数据，evLoop->socketPair[1]接收数据
    struct Channel* channel = channelInit(evLoop->socketPair[1],ReadEvent, 
        readLocalMessage,NULL,NULL,evLoop);
    // channel 添加到任务队列
    eventLoopAddTask(evLoop, channel,ADD);
    return evLoop;
}

// 启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop) {
    assert(evLoop != NULL);
    // 取出事件分发和检测模型
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    // 比较线程ID是否正常
    if(evLoop->threadID != pthread_self()) {
        return -1;
    }
    // 循环进行事件处理
    while(!evLoop->isQuit) {
        /*
            dispatch指向的任务函数其实是动态的，由于在初始化的时候指向的是底层的
            IO模型用的是epoll模型，那么dispatcher->dispatch(evLoop,2);
            就是调用epollDispatch，里头的epoll_wait函数会随之被调用，每
            调用一次epoll_wait函数，就会得到一些被激活的文件描述符
            然后通过for循环，对被激活的文件描述符做一系列的处理 
            如果是poll,就是调用pollDispatch，里头的poll函数会随之被调用，每
            调用一次poll函数，就会得到一些被激活的文件描述符
            然后通过for循环，对被激活的文件描述符做一系列的处理 
            如果是select,就是调用selectDispatch，里头的select函数会随之被调用，每
            调用一次select函数，就会得到一些被激活的文件描述符
            然后通过for循环，对被激活的文件描述符做一系列的处理 
        */
        dispatcher->dispatch(evLoop,2); // 超时时长 2s
        eventLoopProcessTask(evLoop);
    }
    return 0;   
}

// 处理被激活的文件描述符fd
int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
    if (fd < 0 || evLoop == NULL)
    {
        return -1;
    }
    // 取出channel
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);
    if (event & ReadEvent && channel->readCallback)
    {
        channel->readCallback(channel->arg);
    }
    if (event & WriteEvent && channel->writeCallback)
    {
        channel->writeCallback(channel->arg);
    }
    return 0;
}

// 添加任务到任务队列
int eventLoopAddTask(struct EventLoop* evLoop,struct Channel* channel,int type) {
    /*
        为什么在上面添加链表节点的时候需要加互斥锁？
        - 因为有可能是当前线程去添加，也有可能是主线程去添加
        如果当前的线程是主线程，那么我们能够让主线程进行节点的处理吗？
        - 肯定不能，因为你当前主线程它只能负责和客户端建立连接，如果这个连接建立好了，
          剩下的事情都是需要由这个子线程来完成的。所以主线程肯定不会给你去处理任务队列
          里边的节点。
          在主线程里边，其实它是有一个反应堆模型的，在当前的这个子线程里边也有一个
          反应堆模型。每个反应堆模型里边都有一个Dispatcher。关于这个Dispatcher就是
          epoll、poll、或者select，所以主线程去处理的话，这个任务就放到主线程的那个
          Dispatcher里边了，这样很显然是不对的。故在子线程的任务队列里边有了任务之后，
          还需要交给子线程的Dispatcher去处理。
          因此这个节点的处理，还需要判断当前线程到底是什么线程。如果它是主线程不能让它去
          处理，如果是子线程，直接让它去处理。
    */
    // 加锁，保护共享资源
    pthread_mutex_lock(&evLoop->mutex);
    // 创建新节点,后添加到任务队列中去
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    // 链表为空
    if(evLoop->head == NULL) {
        evLoop->head = evLoop->tail = node;
    }else {
        evLoop->tail->next = node; // 添加
        evLoop->tail = node; // 后移
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // 处理节点
    /**
     * 这个描述假设了一个前提条件，就是当前的EventLoop反应堆属于子线程
     * 细节：
     *  1.对于链表节点的添加：可能是当前线程也可能是其他线程（主线程）
     *      1）.修改fd的事件，当前子线程发起，当前子线程处理
     *      2）.添加新的fd（意味着和一个新的客户端建立连接，这是由主线程做的，故添加任务节点这个操作肯定是由主线程做的），
     *          添加任务节点的操作是由主线程发起的
     *  2.不能让主线程处理任务队列里边的节点，需要由当前的子线程去处理
     *      
     *      
    */
    if(evLoop->threadID == pthread_self()) {
        // 当前子线程
        eventLoopProcessTask(evLoop);
    }else{
        // 主线程 -- 告诉子线程处理任务队列中的任务
        // 1.子线程在工作 2.子线程被阻塞了：select、poll、epoll
        taskWakeup(evLoop);
    }
    /*
        小细节：假设说添加任务的是主线程，那么程序就会执行taskWakeup这个
               函数，主线程执行这个函数，对于子线程来说有两种情况，第一种
               情况它正在干活，对于子线程没有影响，充其量就是它检测的那个
               集合里边多出来了一个被激活的文件描述符。如果说此时子线程
               被select、poll、或epoll_wait阻塞了,调用taskWakeup可以
               解除其阻塞。
               如果解除阻塞了，我们希望子线程干什么事情呢？
               因为主线程是在子线程的任务队列里边添加了一个任务， 那么我们
               让子线程解除阻塞是需要让子线程去处理任务队列里边的任务。
               因此需要在eventLoopRun函数中调用eventLoopProcessTask(evLoop);
               因为这个反应堆模型只要开始运行（eventLoopRun）就会不停的调用dispatch函数，
               这个dispatch是一个函数指针，底层指向的是poll模型的poll函数，
               select模型的select函数，epoll模型的epoll_wait函数，如果
               当前的子线程正在被刚才的提到的这三个函数里边的其中一个阻塞着，
               此时正好被主线程唤醒了。需要在循环进行事件处理中添加一句
               eventLoopProcessTask(evLoop);
        
        总结：
            关于任务队列的处理有两个路径：
            第一个路径：子线程往任务队列里边添加一个任务，比如说修改文件描述符里边的事件，
            肯定是子线程自己修改自己检测的文件描述符的事件，修改完了之后，子线程就直接调用
            eventLoopProcessTask(evLoop);这个函数去处理任务队列里边的任务。
            第二个路径：主线程在子线程的任务队列里边添加了一个任务，主线程是处理不了的，
            并且主线程现在也不知道子线程是在工作还是在阻塞，所以主线程就默认子线程现在
            正在阻塞，因此主线程就调用了一个唤醒函数（taskWakeup），调用这个函数保证
            子线程肯定是在运行的，而子线程是eventLoopRun函数的dispatch函数的调用位置
            解除了阻塞，然后调用eventLoopProcessTask(evLoop);
            int eventLoopRun(struct EventLoop* evLoop) {
                ...
                // 循环进行事件处理
                while(!evLoop->isQuit) {
                    dispatcher->dispatch(evLoop,2); // 超时时长 2s
                    eventLoopProcessTask(evLoop);
                }
                ...
            }

    */
    return 0;
}

// 处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop) {
    pthread_mutex_lock(&evLoop->mutex);
    // 取出头节点
    struct ChannelElement* head = evLoop->head;
    while (head!=NULL) {
        struct Channel* channel = head->channel;
        if(head->type == ADD) {
            // 添加
            eventLoopAdd(evLoop,channel);
        }
        else if(head->type == DELETE) {
            // 删除
            eventLoopRemove(evLoop,channel);
        }
        else if(head->type == MODIFY) {
            // 修改
            eventLoopModify(evLoop,channel);
        }
        struct ChannelElement* tmp = head;
        head = head->next;
        // 释放节点
        free(tmp);
    }
    evLoop->head = evLoop->tail = NULL;
    pthread_mutex_unlock(&evLoop->mutex);
    return 0;
}

// 将任务队列中的任务添加到Dispatcher的文件描述符检测集合中
int eventLoopAdd(struct EventLoop* evLoop,struct Channel* channel) {
    int fd = channel->fd;// 取出文件描述符fd
    struct ChannelMap* channelMap = evLoop->channelMap;// channelMap存储着channel和fd之间的对应关系
    // 需要判断channelMap里边是否有fd 和 channel对应的键值对（其中，文件描述符fd对应的就是数组的下标）
    if(fd >= channelMap->size) {
        // 没有足够的空间存储键值对 fd->channel ==> 扩容
        if(!makeMapRoom(channelMap,fd,sizeof(struct Channel*))) {
            return -1;
        }
    }
    // 找到fd对应的数组元素位置，并存储
    if(channelMap->list[fd] == NULL) {
        // 把文件描述符fd和channel的对应关系存储到channelMap
        /*
            在dispatcher里边，还有dispatch函数指针，也就是dispatcher->dispatch(evLoop,timeout)
            这个是一个检测函数，通过调用dipatch函数，就可以得到激活的文件描述符，
            得到了激活的文件描述符之后，需要通过这个文件描述符找到它所对应的channel
        */
        channelMap->list[fd] = channel;
        /*
            首先从evLoop里边把dispatcher这个实例给取出来：evLoop->dispatcher
            在dispatcher里边有一系列的函数指针，其中有一个叫做add。
            这个add就是把文件描述符添加到dispatcher对应的文件描述符检测集合中，
            函数指针add，指向的底层函数可能是不一样的，这个取决于我们
            选择的dispatcher模型，它有可能是poll，有可能是epoll,
            也有可能是select。选择的IO模型不一样，add这个函数指针
            指向的函数的处理动作也就不一样
        */
        evLoop->dispatcher->add(channel,evLoop);
    } 
    return 0;
}

/*
    把任务队列里面的节点从dispatcher的检测集合中删除，调用
    dispatcher里边的remove函数，
*/
int eventLoopRemove(struct EventLoop* evLoop,struct Channel* channel) {
    int fd = channel->fd;
    // 从evLoop中取出channelMap实例
    struct ChannelMap* channelMap = evLoop->channelMap;
    /*
        假设我们要删除的这个文件描述符并不在channelMap中存储着，说明我们要操作的这个
        文件描述符并不在dispatcher的检核集合中。
        因为它在检测集合里边，在添加的时候就会把文件描述符fd和channel的映射关系也存储
        到channelMap里边去了。
        故只要它在检测集合里边，它肯定就在channelMap里边。
        如果它不在channelMap里边，那么它就肯定不在检测集合里边。
        如果它不在检测集合里边，就无需做任何事情，直接返回-1
    */
    if(fd >= channelMap->size) {
        return -1;
    }
    // 如果文件描述符fd在检测集合里，就从中把它删除
    int ret = evLoop->dispatcher->remove(channel,evLoop);
    return ret;
}

// 修改检测集合里边文件描述符事件的函数
int eventLoopModify(struct EventLoop* evLoop,struct Channel* channel) {
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if(fd >= channelMap->size || channelMap->list[fd] == NULL) {
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel,evLoop);
    return ret;
}

// 释放channel
int destroyChannel(struct EventLoop* evLoop,struct Channel* channel) {
    // 删除 channel 和 fd 的对应关系
    evLoop->channelMap->list[channel->fd] = NULL;
    // 关闭 fd
    close(channel->fd);
    // 释放 channel 内存
    free(channel);
    return 0;
}