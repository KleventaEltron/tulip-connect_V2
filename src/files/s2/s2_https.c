#include "s2_https.h"
#include <stdio.h>
#include <string.h>

static void erase(void *buffer,size_t length)
{
    volatile uint8_t *p=buffer;
    while(length--) *p++=0;
}
static void clearBuffers(S2_HTTPS *c)
{
    erase(&c->request,sizeof(c->request));erase(c->tx,sizeof(c->tx));
    erase(c->rx,sizeof(c->rx));erase(c->headers,sizeof(c->headers));
    erase(c->body,sizeof(c->body));erase(c->line,sizeof(c->line));
}
static void fail(S2_HTTPS *c,S2_HTTPS_ERROR error)
{
    c->error=error;c->state=S2_HTTPS_FAILED;c->reusable=false;
    if(c->session!=NULL) {
        if(c->session->state!=S2_SESSION_FAILED)
            S2_SESSION_Abort(c->session,error==S2_HTTPS_ERROR_TLS?S2_SESSION_TLS_ERROR:S2_SESSION_HTTP_ERROR);
        c->sessionResult=c->session->error;
    }
    if(c->tls!=NULL)S2_TLS_Close(c->tls);
    clearBuffers(c);
}
static unsigned lower(unsigned ch) {return ch>='A'&&ch<='Z'?ch+('a'-'A'):ch;}
static bool equal(const char *a,const char *b)
{
    while(*a && *b)if(lower((unsigned char)*a++)!=lower((unsigned char)*b++))return false;
    return *a==*b;
}
static bool nameCharacter(unsigned ch)
{
    return (ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||
        (ch!=0 && strchr("!#$%&'*+-.^_`|~",(int)ch)!=NULL);
}
static bool tokenIn(const char *value,const char *wanted)
{
    size_t i=0;
    while(value[i]) {
        size_t start,end,n;
        while(value[i]==' '||value[i]=='\t'||value[i]==',')i++;
        start=i;while(value[i] && value[i]!=',')i++;
        end=i;while(end>start && (value[end-1]==' '||value[end-1]=='\t'))end--;
        n=strlen(wanted);
        if(end-start==n) {
            size_t k;for(k=0;k<n;k++)if(lower((unsigned char)value[start+k])!=lower((unsigned char)wanted[k]))break;
            if(k==n)return true;
        }
    }
    return false;
}
static char *field(char *line)
{
    char *colon=strchr(line,':'),*p,*end;
    if(colon==NULL||colon==line)return NULL;
    for(p=line;p<colon;p++)if(!nameCharacter((unsigned char)*p))return NULL;
    *colon=0;p=colon+1;while(*p==' '||*p=='\t')p++;
    end=p+strlen(p);while(end>p && (end[-1]==' '||end[-1]=='\t'))*--end=0;
    return p;
}
static bool decimal(const char *text,size_t *value)
{
    size_t n=0;
    if(*text==0)return false;
    for(;*text;text++) {
        if(*text<'0'||*text>'9')return false;
        n=n*10u+(unsigned)(*text-'0');
        if(n>S2_SESSION_JSON_MAX)return false;
    }
    *value=n;return true;
}
static bool jsonMediaType(const char *value)
{
    const char *semi=strchr(value,';');
    size_t n=semi!=NULL?(size_t)(semi-value):strlen(value),i;
    while(n>0 && (value[n-1]==' '||value[n-1]=='\t'))n--;
    if(n!=16)return false;
    for(i=0;i<n;i++)if(lower((unsigned char)value[i])!=(unsigned char)"application/json"[i])return false;
    return true;
}
/* Parse only an engine-generated absolute URL and bind it to the verified TLS
 * hostname/port. Never follow a Location header or forward credentials elsewhere. */
static const char *requestPath(const S2_SESSION_REQUEST *q,const char *host,uint16_t port)
{
    const char *p,*end,*hostEnd;
    unsigned parsedPort=443;
    size_t i,n;
    if(q->method==NULL || strcmp(q->method,"POST")!=0 || strncmp(q->url,"https://",8)!=0)return NULL;
    p=q->url+8;end=strchr(p,'/');if(end==NULL)return NULL;
    hostEnd=memchr(p,':',(size_t)(end-p));if(hostEnd==NULL)hostEnd=end;
    n=(size_t)(hostEnd-p);if(n!=strlen(host))return NULL;
    for(i=0;i<n;i++)if(lower((unsigned char)p[i])!=lower((unsigned char)host[i]))return NULL;
    if(hostEnd!=end) {
        const char *digit=hostEnd+1;
        if(digit==end)return NULL;
        parsedPort=0;
        for(;digit<end;digit++) {
            if(*digit<'0'||*digit>'9')return NULL;
            parsedPort=parsedPort*10u+(unsigned)(*digit-'0');if(parsedPort>65535)return NULL;
        }
    }
    if(parsedPort!=port || strchr(end,'#')!=NULL)return NULL;
    for(p=end;*p;p++)if((unsigned char)*p<33 || (unsigned char)*p>126)return NULL;
    return end;
}

bool S2_HTTPS_Begin(S2_HTTPS *c,S2_TLS *tls,uint16_t port,S2_SESSION *session,uint32_t now)
{
    const char *path;
    int length;
    if(c==NULL || tls==NULL || session==NULL || port==0 ||
        c->state==S2_HTTPS_SENDING || c->state==S2_HTTPS_READING ||
        tls->state!=S2_TLS_VERIFIED || (session->state!=S2_SESSION_INITIATE && session->state!=S2_SESSION_CONFIRM))return false;
    if(c->tls!=NULL && c->tls!=tls)S2_TLS_Close(c->tls);
    erase(c,sizeof(*c));c->tls=tls;c->session=session;c->parseState=S2_HTTPS_HEADERS;
    if(!S2_TLS_GetPlatform(tls,&c->backend) || !c->backend.verified(c->backend.user) ||
        !S2_TLS_PeerMatches(tls,tls->hostname,port)) {
        fail(c,S2_HTTPS_ERROR_TLS);return false;
    }
    c->sessionResult=S2_SESSION_Request(session,&c->request,now);
    if(c->sessionResult!=S2_SESSION_OK) {fail(c,S2_HTTPS_ERROR_REQUEST);return false;}
    path=requestPath(&c->request,tls->hostname,port);
    if(path==NULL) {fail(c,S2_HTTPS_ERROR_ARGUMENT);return false;}
    length=snprintf((char*)c->tx,sizeof(c->tx),
        "POST %s HTTP/1.1\r\nHost: %s:%u\r\nAuthorization: Bearer %s\r\n"
        "Content-Type: application/json\r\nAccept: application/json\r\nAccept-Encoding: identity\r\n"
        "Content-Length: %lu\r\nConnection: keep-alive\r\n\r\n%s",
        path,tls->hostname,(unsigned)port,c->request.bearer,
        (unsigned long)strlen(c->request.body),c->request.body);
    erase(&c->request,sizeof(c->request));
    if(length<0 || (size_t)length>=sizeof(c->tx)) {fail(c,S2_HTTPS_ERROR_LIMIT);return false;}
    c->txLength=(size_t)length;c->state=S2_HTTPS_SENDING;return true;
}

static bool headers(S2_HTTPS *c)
{
    char *line=c->headers,*end=strstr(line,"\r\n");
    bool typeSeen=false,encodingSeen=false,keepAlive=false;
    if(end==NULL)return false;
    *end=0;
    if(strlen(line)<12 || (strncmp(line,"HTTP/1.1 ",9)!=0 && strncmp(line,"HTTP/1.0 ",9)!=0) ||
        line[9]<'1'||line[9]>'5'||line[10]<'0'||line[10]>'9'||line[11]<'0'||line[11]>'9'||
        (line[12]!=0 && line[12]!=' '))return false;
    c->httpStatus=(unsigned)(line[9]-'0')*100u+(unsigned)(line[10]-'0')*10u+(unsigned)(line[11]-'0');
    c->http10=line[7]=='0';c->hasLength=c->chunked=c->jsonType=c->connectionClose=false;
    c->contentLength=0;line=end+2;
    while(*line) {
        char *value;
        end=strstr(line,"\r\n");if(end==NULL)return false;*end=0;
        if(*line==0)break;
        value=field(line);if(value==NULL)return false;
        if(equal(line,"Content-Length")) {
            if(c->hasLength || !decimal(value,&c->contentLength))return false;
            c->hasLength=true;
        } else if(equal(line,"Transfer-Encoding")) {
            if(c->chunked || !equal(value,"chunked"))return false;
            c->chunked=true;
        } else if(equal(line,"Content-Encoding")) {
            if(encodingSeen || !equal(value,"identity"))return false;
            encodingSeen=true;
        } else if(equal(line,"Content-Type")) {
            if(typeSeen)return false;
            typeSeen=true;c->jsonType=jsonMediaType(value);
        } else if(equal(line,"Connection")) {
            c->connectionClose=c->connectionClose || tokenIn(value,"close");
            keepAlive=keepAlive || tokenIn(value,"keep-alive");
        }
        line=end+2;
    }
    if(c->chunked && (c->hasLength || c->http10))return false;
    if(c->httpStatus<200) {
        if(c->httpStatus==101 || c->chunked || (c->hasLength && c->contentLength!=0) || ++c->interimCount>4)return false;
        c->headerLength=0;erase(c->headers,sizeof(c->headers));return true;
    }
    if(c->httpStatus>=300 && c->httpStatus<400 && c->httpStatus!=304)return false;
    if(c->httpStatus==200 && !c->jsonType)return false;
    c->connectionClose=c->connectionClose || (c->http10 && !keepAlive);
    c->reusable=!c->connectionClose;
    if(c->httpStatus==204 || c->httpStatus==304) {
        if(c->chunked || (c->httpStatus==204 && c->hasLength && c->contentLength!=0))return false;
        c->parseState=S2_HTTPS_COMPLETE;
    } else if(c->chunked)c->parseState=S2_HTTPS_CHUNK_LINE;
    else if(c->hasLength)c->parseState=c->contentLength==0?S2_HTTPS_COMPLETE:S2_HTTPS_LENGTH_BODY;
    else {
        if(!c->connectionClose)return false;
        c->reusable=false;c->parseState=S2_HTTPS_EOF_BODY;
    }
    erase(c->headers,sizeof(c->headers));return true;
}
static int hex(unsigned ch)
{
    if(ch>='0'&&ch<='9')return (int)(ch-'0');
    ch=lower(ch);return ch>='a'&&ch<='f'?(int)(ch-'a'+10):-1;
}
static bool chunkSize(S2_HTTPS *c)
{
    size_t i=0,n=0;
    while(c->line[i] && c->line[i]!=';') {
        int digit=hex((unsigned char)c->line[i++]);if(digit<0)return false;
        n=n*16u+(unsigned)digit;if(n>S2_SESSION_JSON_MAX-c->bodyLength)return false;
    }
    if(i==0)return false;
    c->chunkRemaining=n;c->parseState=n==0?S2_HTTPS_TRAILERS:S2_HTTPS_CHUNK_BODY;return true;
}
static bool trailer(S2_HTTPS *c)
{
    if(c->lineLength==0) {c->parseState=S2_HTTPS_COMPLETE;return true;}
    if(++c->trailerCount>16 || field(c->line)==NULL)return false;
    if(equal(c->line,"Content-Length")||equal(c->line,"Transfer-Encoding")||equal(c->line,"Host")||
        equal(c->line,"Authorization")||equal(c->line,"Content-Type")||equal(c->line,"Content-Encoding")||
        equal(c->line,"Connection"))return false;
    return true;
}
static bool appendBody(S2_HTTPS *c,uint8_t byte)
{
    if(c->bodyLength==S2_SESSION_JSON_MAX)return false;
    c->body[c->bodyLength++]=(char)byte;return true;
}
static bool feed(S2_HTTPS *c,const uint8_t *data,size_t length)
{
    size_t i;
    for(i=0;i<length;i++) {
        unsigned byte=data[i];
        switch(c->parseState) {
            case S2_HTTPS_HEADERS:
                if(c->headerLength==S2_HTTPS_HEADERS_MAX || ++c->headerTotal>S2_HTTPS_HEADERS_MAX)return false;
                if(byte==0 || byte>126 || (byte<32 && byte!='\r' && byte!='\n' && byte!='\t'))return false;
                if((byte=='\n' && (c->headerLength==0 || c->headers[c->headerLength-1]!='\r')) ||
                    (c->headerLength>0 && c->headers[c->headerLength-1]=='\r' && byte!='\n'))return false;
                c->headers[c->headerLength++]=(char)byte;c->headers[c->headerLength]=0;
                if(c->headerLength>=4 && memcmp(c->headers+c->headerLength-4,"\r\n\r\n",4)==0 && !headers(c))return false;
                break;
            case S2_HTTPS_LENGTH_BODY:
                if(!appendBody(c,(uint8_t)byte))return false;
                if(c->bodyLength==c->contentLength)c->parseState=S2_HTTPS_COMPLETE;
                break;
            case S2_HTTPS_EOF_BODY:
                if(!appendBody(c,(uint8_t)byte))return false;
                break;
            case S2_HTTPS_CHUNK_LINE:
            case S2_HTTPS_TRAILERS:
                if(c->parseState==S2_HTTPS_TRAILERS && ++c->trailerTotal>S2_HTTPS_TRAILERS_MAX)return false;
                if(c->lineCR) {
                    if(byte!='\n')return false;
                    c->line[c->lineLength]=0;c->lineCR=false;
                    if(c->parseState==S2_HTTPS_CHUNK_LINE) {if(!chunkSize(c))return false;}
                    else if(!trailer(c))return false;
                    c->lineLength=0;erase(c->line,sizeof(c->line));
                } else if(byte=='\r')c->lineCR=true;
                else {
                    if(byte=='\n'||(byte<32 && byte!='\t')||byte>126||c->lineLength==S2_HTTPS_LINE_MAX)return false;
                    c->line[c->lineLength++]=(char)byte;
                }
                break;
            case S2_HTTPS_CHUNK_BODY:
                if(!appendBody(c,(uint8_t)byte))return false;
                if(--c->chunkRemaining==0)c->parseState=S2_HTTPS_CHUNK_CR;
                break;
            case S2_HTTPS_CHUNK_CR:
                if(byte!='\r')return false;
                c->parseState=S2_HTTPS_CHUNK_LF;break;
            case S2_HTTPS_CHUNK_LF:
                if(byte!='\n')return false;
                c->parseState=S2_HTTPS_CHUNK_LINE;break;
            case S2_HTTPS_COMPLETE:
            default:
                return false; /* No HTTP pipelining or bytes after this response. */
        }
    }
    return true;
}
static void complete(S2_HTTPS *c,uint32_t now)
{
    c->sessionResult=S2_SESSION_Response(c->session,c->httpStatus,c->body,c->bodyLength,true,now);
    if(c->sessionResult!=S2_SESSION_OK) {fail(c,S2_HTTPS_ERROR_SESSION);return;}
    clearBuffers(c);c->state=S2_HTTPS_DONE;
    if(!c->reusable && c->tls->state==S2_TLS_VERIFIED)c->backend.close(c->backend.user);
}
void S2_HTTPS_Tasks(S2_HTTPS *c,uint32_t now)
{
    int count;
    size_t remaining;
    if(c==NULL || c->tls==NULL)return;
    if(c->state==S2_HTTPS_DONE) {S2_TLS_Tasks(c->tls,now);return;}
    if(c->state!=S2_HTTPS_SENDING && c->state!=S2_HTTPS_READING)return;
    S2_SESSION_Tasks(c->session,now);
    if(c->session->state==S2_SESSION_FAILED) {fail(c,S2_HTTPS_ERROR_SESSION);return;}
    S2_TLS_Tasks(c->tls,now);
    if(!c->backend.verified(c->backend.user)) {fail(c,S2_HTTPS_ERROR_TLS);return;}
    if(c->state==S2_HTTPS_SENDING) {
        remaining=c->txLength-c->txOffset;if(remaining>S2_TLS_IO_SIZE)remaining=S2_TLS_IO_SIZE;
        count=c->backend.write(c->backend.user,c->tx+c->txOffset,remaining);
        if(count<0 || (size_t)count>remaining) {fail(c,S2_HTTPS_ERROR_IO);return;}
        erase(c->tx+c->txOffset,(size_t)count);c->txOffset+=(size_t)count;
        if(c->txOffset!=c->txLength)return;
        c->state=S2_HTTPS_READING;
    }
    count=c->backend.read(c->backend.user,c->rx,sizeof(c->rx));
    if(count==S2_TLS_READ_EOF && c->parseState==S2_HTTPS_EOF_BODY) {
        c->parseState=S2_HTTPS_COMPLETE;c->reusable=false;complete(c,now);return;
    }
    if(count<0 || (size_t)count>sizeof(c->rx)) {fail(c,S2_HTTPS_ERROR_IO);return;}
    if(!feed(c,c->rx,(size_t)count)) {fail(c,S2_HTTPS_ERROR_PROTOCOL);return;}
    erase(c->rx,sizeof(c->rx));
    if(c->parseState==S2_HTTPS_COMPLETE)complete(c,now);
}
void S2_HTTPS_Stop(S2_HTTPS *c)
{
    if(c==NULL)return;
    if(c->state==S2_HTTPS_SENDING || c->state==S2_HTTPS_READING) {
        if(c->session!=NULL)S2_SESSION_Abort(c->session,S2_SESSION_HTTP_ERROR);
    }
    if(c->tls!=NULL)S2_TLS_Close(c->tls);
    erase(c,sizeof(*c));
}
