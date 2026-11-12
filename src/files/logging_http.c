#include "logging_http.h"
#include <stdio.h>
#include <string.h>
void LOG_HTTP_TxReset(LOG_HTTP_TX *c) { if(c)memset(c,0,sizeof(*c)); }
bool LOG_HTTP_TxBegin(LOG_HTTP_TX *c,const char *header,size_t n,LOG_HTTP_PRODUCER producer,void *user,uint32_t now) {
    if(!c||!header||!n||(c->state!=LOG_TX_IDLE&&c->state!=LOG_TX_DONE&&c->state!=LOG_TX_FAILED))return false;
    LOG_HTTP_TxReset(c);c->state=LOG_TX_HEADER;c->data=(const uint8_t*)header;c->length=n;
    c->produce=producer;c->user=user;c->chunked=producer!=NULL;c->progress=now;return true;
}
void LOG_HTTP_TxTasks(LOG_HTTP_TX *c,LOG_HTTP_WRITE write,void *user,uint32_t now) {
    int n;size_t length;
    if(!c||c->state==LOG_TX_IDLE||c->state==LOG_TX_DONE||c->state==LOG_TX_FAILED)return;
    if(!write||(uint32_t)(now-c->progress)>=LOG_HTTP_STALL_MS) {c->state=LOG_TX_FAILED;return;}
    if(c->state==LOG_TX_NEXT) {
        const uint8_t *data=NULL;size_t size=0;
        n=c->produce(c->user,&data,&size);
        if(n<0||(n>0&&(!data||!size))) {c->state=LOG_TX_FAILED;return;}
        if(n==0) {c->data=(const uint8_t*)"0\r\n\r\n";c->length=5;c->offset=0;c->state=LOG_TX_FINAL;}
        else {
            c->data=data;c->length=size;c->offset=0;
            n=snprintf(c->framing,sizeof(c->framing),"%lX\r\n",(unsigned long)size);
            if(n<=0||(size_t)n>=sizeof(c->framing)) {c->state=LOG_TX_FAILED;return;}
            c->state=LOG_TX_SIZE;
        }
        return; /* Producer called once; no unbounded work per tick. */
    }
    if(c->state==LOG_TX_SIZE||c->state==LOG_TX_CRLF) {
        const uint8_t *p=(const uint8_t*)(c->state==LOG_TX_SIZE?c->framing:"\r\n");
        length=strlen((const char*)p)-c->offset;
        n=write(user,p+c->offset,length);
        if(n<0||(size_t)n>length) {c->state=LOG_TX_FAILED;return;}
        if(n>0) {c->offset+=(size_t)n;c->progress=now;}
        if(c->offset==strlen((const char*)p)) {c->offset=0;c->state=c->state==LOG_TX_SIZE?LOG_TX_BODY:LOG_TX_NEXT;}
        return;
    }
    length=c->length-c->offset;if(length>LOG_HTTP_IO_BUDGET)length=LOG_HTTP_IO_BUDGET;
    n=write(user,c->data+c->offset,length);
    if(n<0||(size_t)n>length) {c->state=LOG_TX_FAILED;return;}
    if(n>0) {c->offset+=(size_t)n;c->progress=now;}
    if(c->offset==c->length) {
        c->offset=0;
        switch(c->state) {
            case LOG_TX_HEADER:c->state=c->chunked?LOG_TX_NEXT:LOG_TX_DONE;break;
            case LOG_TX_BODY:c->state=LOG_TX_CRLF;break;
            case LOG_TX_FINAL:c->state=LOG_TX_DONE;break;
            default:c->state=LOG_TX_FAILED;break;
        }
    }
}
static unsigned lower(unsigned c) {return c>='A'&&c<='Z'?c+32:c;}
static bool fieldChar(unsigned c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
        (c!=0&&strchr("!#$%&'*+-.^_`|~",(int)c)!=NULL);
}
static bool eq(const char *p,size_t n,const char *s) {
    size_t i;if(n!=strlen(s))return false;for(i=0;i<n;i++)if(lower((unsigned char)p[i])!=(unsigned char)s[i])return false;return true;
}
static bool number(const char *p,size_t n,unsigned base,size_t *out) {
    size_t i,value=0;if(!n)return false;
    for(i=0;i<n;i++) {
        unsigned c=lower((unsigned char)p[i]),digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:99;
        if(digit>=base||value>(SIZE_MAX-digit)/base)return false;
        value=value*base+digit;
    }
    *out=value;return true;
}
static bool token(const char *p,size_t n,const char *wanted) {
    size_t at=0;
    while(at<n) {
        size_t begin,end;while(at<n&&(p[at]==' '||p[at]=='\t'||p[at]==','))at++;
        begin=at;while(at<n&&p[at]!=',')at++;end=at;
        while(end>begin&&(p[end-1]==' '||p[end-1]=='\t'))end--;
        if(eq(p+begin,end-begin,wanted))return true;
    }
    return false;
}
static size_t lineEnd(const char *b,size_t n,size_t at) {
    size_t i;for(i=at;i+1<n;i++)if(b[i]=='\r'&&b[i+1]=='\n')return i;return SIZE_MAX;
}
LOG_HTTP_RESULT LOG_HTTP_Response(char *b,size_t n,bool eof,LOG_HTTP_RESPONSE *out) {
    size_t at,end,header,cl=0,decoded=0;bool hasCL=false,chunked=false,close=false,keep=false,http10;
    LOG_HTTP_RESPONSE r={0};
    if(!b||!out||!n)return eof?LOG_HTTP_BAD:LOG_HTTP_MORE;
    end=lineEnd(b,n,0);
    if(end==SIZE_MAX)return eof||n>2048?LOG_HTTP_BAD:LOG_HTTP_MORE;
    if(end<12||(memcmp(b,"HTTP/1.1 ",9)&&memcmp(b,"HTTP/1.0 ",9))||b[9]<'1'||b[9]>'5'||b[10]<'0'||b[10]>'9'||b[11]<'0'||b[11]>'9'||(end>12&&b[12]!=' '))return LOG_HTTP_BAD;
    http10=b[7]=='0';r.status=(unsigned)(b[9]-'0')*100u+(unsigned)(b[10]-'0')*10u+(unsigned)(b[11]-'0');at=end+2;
    for(;;) {
        size_t colon,value,last,i;
        end=lineEnd(b,n,at);
        if(end==SIZE_MAX)return eof||n>2048?LOG_HTTP_BAD:LOG_HTTP_MORE;
        if(end>2048)return LOG_HTTP_BAD;
        if(end==at) {at+=2;break;}
        for(colon=at;colon<end&&b[colon]!=':';colon++) {}
        if(colon==at||colon==end)return LOG_HTTP_BAD;
        for(i=at;i<colon;i++)if(!fieldChar((unsigned char)b[i]))return LOG_HTTP_BAD;
        value=colon+1;while(value<end&&(b[value]==' '||b[value]=='\t'))value++;last=end;
        while(last>value&&(b[last-1]==' '||b[last-1]=='\t'))last--;
        for(i=value;i<last;i++)if(((unsigned char)b[i]<32&&b[i]!='\t')||(unsigned char)b[i]==127)return LOG_HTTP_BAD;
        if(eq(b+at,colon-at,"content-length")) {
            if(hasCL||!number(b+value,last-value,10,&cl))return LOG_HTTP_BAD;
            hasCL=true;
        } else if(eq(b+at,colon-at,"transfer-encoding")) {
            if(chunked||!eq(b+value,last-value,"chunked"))return LOG_HTTP_BAD;
            chunked=true;
        } else if(eq(b+at,colon-at,"connection")) {
            close=close||token(b+value,last-value,"close");keep=keep||token(b+value,last-value,"keep-alive");
        } else if(eq(b+at,colon-at,"content-encoding")&&!eq(b+value,last-value,"identity"))return LOG_HTTP_BAD;
        at=end+2;
    }
    if(hasCL&&chunked)return LOG_HTTP_BAD;
    if(r.status<200)return LOG_HTTP_BAD; /* No Expect/upgrade requested by logging. */
    header=at;r.bodyOffset=header;r.chunked=chunked;
    r.reusable=!close&&(!http10||keep)&&(chunked||hasCL||r.status==204||r.status==304);
    if(r.status==204||r.status==304) {
        if(chunked||(hasCL&&cl)||n!=header)return LOG_HTTP_BAD;
        decoded=0;
    } else if(hasCL) {
        if(cl>4096||n-header>cl)return LOG_HTTP_BAD;
        if(n-header<cl)return eof?LOG_HTTP_BAD:LOG_HTTP_MORE;
        decoded=cl;
    } else if(chunked) {
        /* First pass validates all framing; don't mutate incomplete buffers. */
        for(;;) {
            size_t size,semi,i;
            end=lineEnd(b,n,at);if(end==SIZE_MAX)return eof?LOG_HTTP_BAD:LOG_HTTP_MORE;
            if(end-at>128)return LOG_HTTP_BAD;
            for(semi=at;semi<end&&b[semi]!=';';semi++) {}
            if(!number(b+at,semi-at,16,&size)||size>4096||decoded>4096-size)return LOG_HTTP_BAD;
            for(i=semi;i<end;i++)if((unsigned char)b[i]<32||(unsigned char)b[i]>126)return LOG_HTTP_BAD;
            at=end+2;
            if(!size) {
                size_t trailers=0,start=at;
                for(;;) {
                    end=lineEnd(b,n,at);if(end==SIZE_MAX)return eof?LOG_HTTP_BAD:LOG_HTTP_MORE;
                    if(end-start>512||++trailers>16)return LOG_HTTP_BAD;
                    if(end==at) {at=end+2;break;}
                    {
                        const char *colon=memchr(b+at,':',end-at);size_t i;
                        if(!colon||colon==b+at)return LOG_HTTP_BAD;
                        for(i=at;b+i<colon;i++)if(!fieldChar((unsigned char)b[i]))return LOG_HTTP_BAD;
                        if(eq(b+at,(size_t)(colon-b-at),"content-length")||eq(b+at,(size_t)(colon-b-at),"transfer-encoding")||eq(b+at,(size_t)(colon-b-at),"connection"))return LOG_HTTP_BAD;
                        for(i=(size_t)(colon-b)+1;i<end;i++)if(((unsigned char)b[i]<32&&b[i]!='\t')||(unsigned char)b[i]==127)return LOG_HTTP_BAD;
                    }
                    at=end+2;
                }
                if(at!=n)return LOG_HTTP_BAD;
                break;
            }
            if(size>n-at||n-at-size<2)return eof?LOG_HTTP_BAD:LOG_HTTP_MORE;
            if(b[at+size]!='\r'||b[at+size+1]!='\n')return LOG_HTTP_BAD;
            decoded+=size;at+=size+2;
        }
        at=header;decoded=0;
        for(;;) {
            size_t size,semi;end=lineEnd(b,n,at);
            for(semi=at;semi<end&&b[semi]!=';';semi++) {}
            if(!number(b+at,semi-at,16,&size))return LOG_HTTP_BAD;
            at=end+2;if(!size)break;
            memmove(b+decoded,b+at,size);decoded+=size;at+=size+2;
        }
    } else {
        if(!close&&!http10)return LOG_HTTP_BAD;
        if(!eof)return LOG_HTTP_MORE;
        decoded=n-header;r.reusable=false;
    }
    if(!chunked&&decoded)memmove(b,b+header,decoded);
    b[decoded]=0;r.bodyLength=decoded;*out=r;return LOG_HTTP_DONE;
}
