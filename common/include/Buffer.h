#pragma once

struct Buffer {
    // 指向内存的指针
    char* data;
    int capacity;
    int readPos;
    int writePos;
};

// 初始化
struct Buffer* bufferInit(int size);

// 销毁
void bufferDestroy(struct Buffer* buf);

// 扩容
void bufferExtendRoom(struct Buffer* buf, int size);

// 得到剩余的可写的内存容量
int bufferWriteableSize(struct Buffer* buf);

// 得到剩余的可读的内存容量
int bufferReadableSize(struct Buffer* buf);

// 写内存 1.直接写 
int bufferAppendData(struct Buffer* buf, const char* data, int size); 
int bufferAppendString(struct Buffer* buf, const char* data); 

// 写内存 2.接收套接字数据
int bufferSocketRead(struct Buffer* buf,int fd);

// 根据\r\n取出一行,找到其在数据块中的位置，返回该位置
char* bufferFindCRLF(struct Buffer* buf);

// 发送数据
int bufferSendData(struct Buffer* buf,int socket);