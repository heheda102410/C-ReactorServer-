#pragma once
#include "Channel.h"
#include "EventLoop.h"

// 声明(不管这个结构体有无被定义出来,先告诉编译器有这么一种类型)
struct EventLoop;
struct Dispatcher {
    /*  
        init 初始化epoll、select、或者poll需要的数据块
        最后需要把这个数据块的内存地址给到函数的调用者
        所以它的返回值肯定是一个指针
        另外epoll、select、或者poll它们需要的数据块对应的内存类型一样吗？
        不一样，如果想要一种类型来兼容三种不同的类型，怎么做到呢？
        在C语言里就是使用泛型，故返回值类型为void*
    */
    void* (*init)();
    // 添加
    int (*add)(struct Channel* channel,struct EventLoop* evLoop);
    // 删除
    int (*remove)(struct Channel* channel,struct EventLoop* evLoop);
    // 修改
    int (*modify)(struct Channel* channel,struct EventLoop* evLoop);
    // 事件检测
    int (*dispatch)(struct EventLoop* evLoop,int timeout); // 单位:s
    // 清除数据（关闭fd或者释放内存）
    int (*clear)(struct EventLoop* evLoop);
};