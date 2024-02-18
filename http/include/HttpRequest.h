#pragma once
#include "Buffer.h"
#include "HttpResponse.h"
#include <stdbool.h>

// 请求头键值对
struct RequestHeader{
    char* key;
    char* value;
};

// 当前的解析状态
enum HttpRequestState {
    ParseReqLine,    // 当前解析的是请求行
    ParseReqHeaders, // 当前解析的是请求头
    ParseReqBody,    // 当前解析的是请求体
    ParseReqDone     // 完成解析
};

// 定义 http 请求结构体
struct HttpRequest {
    // 当前解析状态
    enum HttpRequestState curState;  
    // 请求行
    char* method;
    char* url;
    char* version;  
    // 请求头
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
};

// 初始化
struct HttpRequest* httpRequestInit();

// 重置
void httpRequestReset(struct HttpRequest* req);
void httpRequestResetEx(struct HttpRequest* req);
// 销毁
void httpRequestDestroy(struct HttpRequest* req);

// 获取处理状态
enum HttpRequestState httpRequestState(struct HttpRequest* req);

// 添加请求头
void httpRequestAddHeader(struct HttpRequest* req,const char* key,const char* value);

// 根据key得到请求头的value
char* httpRequestGetHeader(struct HttpRequest* req,const char* key);

// 解析请求行
bool parseHttpRequestLine(struct HttpRequest* req,struct Buffer* readBuf);

// 解析请求头
bool parseHttpRequestHeader(struct HttpRequest* req,struct Buffer* readBuf);

// 解析http请求协议
bool parseHttpRequest(struct HttpRequest* req,struct Buffer* readBuf,
                      struct HttpResponse* response,struct Buffer* sendBuf,int socket);

// 处理http请求协议
bool processHttpRequest(struct HttpRequest* req,struct HttpResponse* response);

// 发送文件
void sendFile(const char* fileName,struct Buffer* sendBuf,int cfd);
// 判断文件扩展名，返回对应的 Content-Type(Mime-Type)
const char* getFileType(const char* name);

// 发送目录
void sendDir(const char* dirName,struct Buffer* sendBuf,int cfd); 

// 解码字符串
void decodeMsg(char* to,char* from);
int hexToDec(char c);









// GET /pic/1.jpg HTTP/1.1
// Host: 192.168.1.100:8080
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36 
// Accept: image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8
// Accept-Encoding: gzip, deflate, br
// Accept-Language: zh-CN,zh;q=0.9
