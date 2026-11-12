#include "logging_http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char wire[70000],body[20000];static size_t used,writeMax;static bool blocked,badWrite;static unsigned pieces,produceCalls;
static int writeBytes(void *u,const uint8_t *p,size_t n) {
    (void)u;assert(n<=LOG_HTTP_IO_BUDGET);if(blocked)return 0;if(badWrite)return (int)n+1;
    if(n>writeMax)n=writeMax;assert(used+n<sizeof(wire));memcpy(wire+used,p,n);used+=n;wire[used]=0;return (int)n;
}
static int producer(void *u,const uint8_t **p,size_t *n) {
    (void)u;produceCalls++;if(pieces++==0) {*p=(const uint8_t*)body;*n=strlen(body);return 1;}return 0;
}
static void received(const char *source,const char *expected,unsigned status,bool reusable) {
    char buffer[5000];LOG_HTTP_RESPONSE response;size_t length=strlen(source),split;
    for(split=0;split<length;split++) {
        memcpy(buffer,source,split);buffer[split]=0;
        assert(LOG_HTTP_Response(buffer,split,false,&response)==LOG_HTTP_MORE);
        assert(!memcmp(buffer,source,split));
    }
    strcpy(buffer,source);assert(LOG_HTTP_Response(buffer,length,false,&response)==LOG_HTTP_DONE);
    assert(response.status==status&&response.reusable==reusable&&response.bodyLength==strlen(expected)&&!strcmp(buffer,expected));
}
static void invalid(const char *source,bool eof) {
    char buffer[5000];LOG_HTTP_RESPONSE response;strcpy(buffer,source);
    assert(LOG_HTTP_Response(buffer,strlen(buffer),eof,&response)==LOG_HTTP_BAD);
}
int main(void) {
    LOG_HTTP_TX tx={0};uint32_t now;LOG_HTTP_RESPONSE response;char buffer[5000];
    received("HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\n{\"x\":1}","{\"x\":1}",200,true);
    received("HTTP/1.1 200 OK\r\ntRaNsFeR-EnCoDiNg: Chunked\r\nConnection: close\r\n\r\n3;foo=bar\r\nabc\r\n4\r\ndefg\r\n0\r\nX-Trace: yes\r\n\r\n","abcdefg",200,false);
    received("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n","",200,true);
    received("HTTP/1.1 204 No Content\r\n\r\n","",204,true);
    received("HTTP/1.1 503 Error\r\nContent-Length: 2\r\n\r\n{}","{}",503,true);
    strcpy(buffer,"HTTP/1.0 200 OK\r\n\r\nhello");assert(LOG_HTTP_Response(buffer,strlen(buffer),false,&response)==LOG_HTTP_MORE);
    assert(LOG_HTTP_Response(buffer,strlen(buffer),true,&response)==LOG_HTTP_DONE&&!response.reusable&&!strcmp(buffer,"hello"));
    invalid("HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\nshort",true);
    invalid("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}",false);
    invalid("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n{}",false);
    invalid("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\n",false);
    invalid("HTTP/1.1 200 OK\r\nContent-Length: 9999999999999999999999999\r\n\r\n",false);
    invalid("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}extra",false);
    invalid("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Encoding: gzip\r\n\r\n{}",false);
    invalid("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nabc",true);
    invalid("HTTP/1.1 200 OK\r\n\r\n",true);
    for(size_t size=1;size<=1024;size*=2) {
        used=pieces=produceCalls=0;writeMax=size;blocked=badWrite=false;
        memset(body,'x',8192);body[8192]=0;LOG_HTTP_TxReset(&tx);
        assert(LOG_HTTP_TxBegin(&tx,"POST /fixture HTTP/1.1\r\nHost: mock.invalid\r\nTransfer-Encoding: chunked\r\n\r\n",strlen("POST /fixture HTTP/1.1\r\nHost: mock.invalid\r\nTransfer-Encoding: chunked\r\n\r\n"),producer,NULL,0));
        for(now=0;tx.state!=LOG_TX_DONE&&tx.state!=LOG_TX_FAILED;now++) {unsigned before=produceCalls;LOG_HTTP_TxTasks(&tx,writeBytes,NULL,now);assert(produceCalls-before<=1);assert(now<14000);}
        assert(tx.state==LOG_TX_DONE&&strstr(wire,"2000\r\n")&&strstr(wire+used-7,"\r\n0\r\n\r\n"));
        if(size==1) {printf("WIRE_HEX ");for(size_t i=0;i<used;i++)printf("%02x",(unsigned char)wire[i]);putchar('\n');}
    }
    LOG_HTTP_TxReset(&tx);blocked=true;assert(LOG_HTTP_TxBegin(&tx,"GET / HTTP/1.1\r\n\r\n",18,NULL,NULL,UINT32_MAX-100));
    LOG_HTTP_TxTasks(&tx,writeBytes,NULL,(UINT32_MAX-100u)+LOG_HTTP_STALL_MS);assert(tx.state==LOG_TX_FAILED);
    blocked=false;badWrite=true;assert(LOG_HTTP_TxBegin(&tx,"GET / HTTP/1.1\r\n\r\n",18,NULL,NULL,0));LOG_HTTP_TxTasks(&tx,writeBytes,NULL,1);assert(tx.state==LOG_TX_FAILED);
    puts("Logging HTTP: bounded partial/blocked sends, 8KB streamed body, deadline/wrap, fragmented CL/chunks/trailers/EOF, bad framing and HTTP errors passed.");return 0;
}
