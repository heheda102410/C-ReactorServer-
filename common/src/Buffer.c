#define _GNU_SOURCE
#include "Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>

// 初始化
struct Buffer* bufferInit(int size) {
    struct Buffer* buffer = (struct Buffer*)malloc(sizeof(struct Buffer));
    if(buffer!=NULL) {
        buffer->data = (char*)malloc(size);
        buffer->capacity = size;
        buffer->readPos = buffer->writePos = 0;
        memset(buffer->data, 0, size);
    }
    return buffer;
}

// 销毁
void bufferDestroy(struct Buffer* buf) {
    if(buf!=NULL) {
        if(buf->data!=NULL) { //指向有效的堆内存
            free(buf->data); // 释放
        }
    }
    free(buf);
}

// 扩容
void bufferExtendRoom(struct Buffer* buf, int size) {
    // 1.内存够用 - 不需要扩容
    if(bufferWriteableSize(buf)>= size) {
        return;
    }
    // 2.内存需要合并才够用 - 不需要扩容
    //   已读的内存 + 剩余的可写的内存>= size
    else if(buf->readPos + bufferWriteableSize(buf) >= size) {
        // 得到已写但未读的内存大小
        int readableSize = bufferReadableSize(buf);
        // 移动内存实现合并
        memcpy(buf->data, buf->data + buf->readPos, readableSize);
        // 更新位置
        buf->readPos = 0;
        buf->writePos = readableSize;
    }
    // 3.内存不够用 - 需要扩容
    else{
        void* temp = realloc(buf->data, buf->capacity + size);
        if(temp ==NULL) {
            return;// 失败了
        }  
        memset(temp + buf->capacity, 0, size);// 只需要对拓展出来的大小为size的内存块进行初始化就可以了
        // 更新数据
        buf->data = temp;
        buf->capacity += size;
    }
}

// 得到剩余的可写的内存容量
int bufferWriteableSize(struct Buffer* buf) {
    return buf->capacity - buf->writePos;
}

// 得到剩余的可读的内存容量
int bufferReadableSize(struct Buffer* buf) {
    return buf->writePos - buf->readPos;
}

// 写内存 1.直接写
int bufferAppendData(struct Buffer* buf, const char* data, int size) {
    // 判断传入的buf是否为空，data指针指向的是否为有效内存，以及数据大小是否大于零
    if(buf == NULL || data == NULL || size <= 0) {
        return -1;
    }
    // 扩容(试探性的)
    bufferExtendRoom(buf,size);
    // 数据拷贝
    memcpy(buf->data + buf->writePos, data, size);
    // 更新写位置
    buf->writePos += size;
    return 0;
}

// 写内存 1.直接写 
int bufferAppendString(struct Buffer* buf, const char* data) {
    int size = strlen(data);
    int ret = bufferAppendData(buf, data, size);
    return ret;
}

// 写内存 2.接收套接字数据
/*
    当调用这个bufferSocketRead函数之后，一共接收到了多少个字节
    在这个函数里边，通过malloc申请了一块临时的堆内存（tmpbuf），
    这个堆内存是用来接收套接字数据的。当buf里边的数组容量不够了，那么就使用
    这块临时内存来存储数据，还需要把tmpbuf这块
    堆内存里边的数据再次写入到buf中。当用完了之后，需要释放内存。

    read/recv/readv:
    在接收数据的时候，
            - read/recv 只能指定一个数组
            - readv 能指定多个数组（也就是说第一个用完，用第二个...）
*/
int bufferSocketRead(struct Buffer* buf,int fd) {
    struct iovec vec[2]; // 根据自己的实际需求
    // 初始化数组元素
    int writeableSize = bufferWriteableSize(buf); // 得到剩余的可写的内存容量
    // 0号数组里的指针指向buf里边的数组，记得 要加writePos，防止覆盖数据
    vec[0].iov_base = buf->data + buf->writePos;
    vec[0].iov_len = writeableSize;

    char* tmpbuf = (char*)malloc(40960); // 申请40k堆内存
    vec[1].iov_base = buf->data + buf->writePos;
    vec[1].iov_len = 40960;
    // 至此，结构体vec的两个元素分别初始化完之后就可以调用接收数据的函数了
    int result = readv(fd, vec, 2);// 表示通过调用readv函数一共接收了多少个字节
    if(result == -1) {
        return -1;// 失败了
    }
    else if (result <= writeableSize) { 
        // 说明在接收数据的时候，全部的数据都被写入到vec[0]对应的数组里边去了，全部写入到
        // buf对应的数组里边去了，直接移动writePos就好
        buf->writePos += result;
    }
    else {
        // 进入这里，说明buf里边的那块内存是不够用的，
        // 所以数据就被写入到我们申请的40k堆内存里边，还需要把tmpbuf这块
        // 堆内存里边的数据再次写入到buf中。
        // 先进行内存的扩展，再进行内存的拷贝，可调用bufferAppendData函数
        // 注意一个细节：在调用bufferAppendData函数之前，通过调用readv函数
        // 把数据写进了buf,但是buf->writePos没有被更新，故在调用bufferAppendData函数
        // 之前，需要先更新buf->writePos
        buf->writePos = buf->capacity; // 需要先更新buf->writePos
        bufferAppendData(buf, tmpbuf, result - writeableSize);
    }
    free(tmpbuf);
    return result;
}

// 根据\r\n取出一行,找到其在数据块中的位置，返回该位置
/*
    char *strstr(const char *haystack, const char *needle);
    void *memmem(const void *haystack, size_t haystacklen,
                    const void *needle, size_t needlelen);
*/

// CRLF表示\r\n
char* bufferFindCRLF(struct Buffer* buf) {
    // strstr --> 从大字符串中去匹配子字符串（遇到\0结束）
    // memmem --> 从大数据块中去匹配子数据块（需要指定数据块大小）
    char* ptr = memmem(buf->data + buf->readPos,bufferReadableSize(buf),"\r\n",2);
    return ptr;
}

// 发送数据
int bufferSendData(struct Buffer* buf,int socket) {
    // 判断有无数据
    int readableSize = bufferReadableSize(buf);// 这些未读的数据就是待发送的数据
    if(readableSize > 0) {
        int count = send(socket,buf->data + buf->readPos,readableSize,MSG_NOSIGNAL);
        if(count > 0) {
            buf->readPos += count;
            usleep(1);
        }
        return count;
    }    
    return 0;
}