#pragma once
#include "Channel.h"
struct ChannelMap {
    struct Channel** list;
    int size;// 记录指针指向的数组的元素总个数
};

// 初始化
struct ChannelMap* channelMapInit(int size);

// 清空map
void ChannelMapClear(struct ChannelMap* map);

// 重新分配内存空间
bool makeMapRoom(struct ChannelMap* map,int newSize,int unitSize);