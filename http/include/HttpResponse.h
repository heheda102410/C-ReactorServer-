#pragma once
#include "Buffer.h"

// 定义状态码枚举
enum HttpStatusCode {
    Unknown = 0,
    OK = 200,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    BadRequest = 400,
    NotFound = 404
};

// 定义响应的结构体
struct ResponseHeader {
    char key[32];
    char value[128];
};

// 定义一个函数指针，用来组织要回复给客户端的数据块
typedef void (*responseBody) (const char* fileName,struct Buffer* sendBuf,int socket);

// 定义结构体
struct HttpResponse {
    // 状态行:状态码，状态描述
    enum HttpStatusCode statusCode;
    char statusMsg[128];
    // 响应头 - 键值对
    struct ResponseHeader* headers;
    int headerNum;
    /*
        服务器回复给客户端的数据取决于客户端向服务器请求了什么类型的资源,
        有可能它请求的是一个目录,有可能请求的是一个文件,这个文件有可能是
        一个文本文件,也可能是一个图片,还可能是mp3...需要根据客户端的请求
        去回复相应的数据.所以如何去组织这个需要回复的数据块呢?
        - 定义一个函数指针，用来组织要回复给客户端的数据块

        fileName:分成两类,一类是目录类型,一类是非目录类型的文件
        如果是目录就去遍历目录,如果是文件,就读取其内容
        在进行套接字的通信过程中,如果要接收数据,它就是用来存储客户端
        发过来的数据块;
        如果要回复数据(给客户端发数据),发送的数据要先存储到sendBuf
        里边,再发送给客户端.
        socket:就是用来通信的文件描述符
        通过这个用于通信的文件描述符,就能够把写入到sendBuf里边的数据发送给客户端.
        sendBuf里边的数据就是我们组织好的Http响应的数据块
    */
    responseBody sendDataFunc;
    // 文件名
    char fileName[128];
};

// 初始化
struct HttpResponse* httpResponseInit();

// 销毁
void httpResponseDestroy(struct HttpResponse* response);

// 添加响应头
void httpResponseAddHeader(struct HttpResponse* response,const char* key,const char* value);

// 组织http响应数据
void httpResponsePrepareMsg(struct HttpResponse* response,struct Buffer* sendBuf,int socket);