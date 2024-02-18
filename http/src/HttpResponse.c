#include "HttpResponse.h"
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ResHeaderSize 16
// 初始化
struct HttpResponse* httpResponseInit() {
    struct HttpResponse* response = (struct HttpResponse*)malloc(sizeof(struct HttpResponse));
    // 状态行:状态码，状态描述
    response->statusCode = Unknown;
    bzero(response->statusMsg,sizeof(response->statusMsg));

    // 响应头 - 键值对
    int size = sizeof(struct ResponseHeader) * ResHeaderSize;
    response->headers = (struct ResponseHeader*)malloc(size);
    bzero(response->headers, size);
    response->headerNum = 0;

    // 函数指针
    response->sendDataFunc = NULL;

    // 文件名
    bzero(response->fileName,sizeof(response->fileName));
    return response;
}

// 销毁
void httpResponseDestroy(struct HttpResponse* response) {
    if(response!=NULL) {
        free(response->headers);
        free(response);
    }
}

// 添加响应头
void httpResponseAddHeader(struct HttpResponse* response,const char* key,const char* value){
    if(response == NULL || key == NULL || value == NULL) {
        return;
    }
    strcpy(response->headers[response->headerNum].key,key);
    strcpy(response->headers[response->headerNum].value,value);
    response->headerNum++;
}

// 组织http响应数据
void httpResponsePrepareMsg(struct HttpResponse* response,struct Buffer* sendBuf,int socket) {
    // 状态行
    char tmp[1024] = {0};
    sprintf(tmp,"HTTP/1.1 %d %s\r\n",response->statusCode,response->statusMsg);
    bufferAppendString(sendBuf,tmp);
    
    // 响应头
    for(int i=0;i<response->headerNum;++i) {
        // memset(tmp,0,sizeof(tmp));  ?????????
        sprintf(tmp,"%s: %s\r\n",response->headers[i].key,response->headers[i].value);
        bufferAppendString(sendBuf,tmp);
    }

    // 空行
    bufferAppendString(sendBuf,"\r\n");
    
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuf,socket);
#endif
    // 回复的数据
    response->sendDataFunc(response->fileName,sendBuf,socket);
}