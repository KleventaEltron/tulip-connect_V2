#include "s2_session.h"
#include "files/cJSON.h"
#include <stdio.h>
#include <string.h>

static void erase(void *data,size_t length)
{
    volatile uint8_t *p=data;
    while(length--) *p++=0;
}
void S2_SESSION_Clear(S2_SESSION *c) { if(c!=NULL) erase(c,sizeof(*c)); }
static S2_SESSION_RESULT failed(S2_SESSION *c,S2_SESSION_RESULT result)
{
    erase(&c->details,sizeof(c->details));c->state=S2_SESSION_FAILED;c->error=result;
    return result;
}
void S2_SESSION_Abort(S2_SESSION *c,S2_SESSION_RESULT reason)
{
    if(c!=NULL)failed(c,reason==S2_SESSION_OK?S2_SESSION_HTTP_ERROR:reason);
}
static bool visible(const char *s,size_t maximum)
{
    size_t i;
    if(s==NULL) return false;
    for(i=0;i<=maximum;i++) {
        unsigned ch=(unsigned char)s[i];
        if(ch==0) return i!=0;
        if(ch<33 || ch>126 || ch=='"' || ch=='\\') return false;
    }
    return false;
}
static bool uuid(const char *s)
{
    size_t i;
    if(s==NULL) return false;
    for(i=0;i<36;i++) {
        unsigned ch=(unsigned char)s[i];
        if(i==8 || i==13 || i==18 || i==23) { if(ch!='-') return false; }
        else if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F'))) return false;
    }
    return s[36]==0;
}
static int b64value(unsigned ch)
{
    if(ch>='A'&&ch<='Z')return (int)(ch-'A');
    if(ch>='a'&&ch<='z')return (int)(ch-'a'+26);
    if(ch>='0'&&ch<='9')return (int)(ch-'0'+52);
    return ch=='+'?62:ch=='/'?63:-1;
}
static bool token(const char *s)
{
    size_t n=0,padding=0,i;
    if(s==NULL)return false;
    while(n<S2_SESSION_TOKEN_SIZE && s[n]!=0)n++;
    if(n<44 || n>=S2_SESSION_TOKEN_SIZE || n%4!=0)return false;
    if(s[n-1]=='=')padding++;
    if(s[n-2]=='=')padding++;
    for(i=0;i<n-padding;i++)if(b64value((unsigned char)s[i])<0)return false;
    if(padding==1 && (b64value((unsigned char)s[n-2])&3)!=0)return false;
    if(padding==2 && (b64value((unsigned char)s[n-3])&15)!=0)return false;
    return n/4*3-padding>=32;
}
static bool sameHost(const char *a,const char *b)
{
    while(*a && *b) {
        unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x+='a'-'A';
        if(y>='A'&&y<='Z')y+='a'-'A';
        if(x!=y)return false;
    }
    return *a==*b;
}
static bool url(const char *s,const char *scheme,char host[254],uint16_t *port,char *path)
{
    size_t i=0,start,label=0,j=0;
    unsigned number=443;
    bool letter=false;
    if(!visible(s,S2_SESSION_URL_SIZE-1) || strncmp(s,scheme,strlen(scheme))!=0)return false;
    start=strlen(scheme);i=start;
    while(s[i]!=0 && s[i]!=':' && s[i]!='/') {
        unsigned ch=(unsigned char)s[i++];
        bool alpha=(ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z');
        if(j>=253)return false;
        if(ch=='.') {if(label==0 || host[j-1]=='-')return false;label=0;}
        else {if(!alpha && !(ch>='0'&&ch<='9') && ch!='-')return false;
            if((label==0&&ch=='-') || ++label>63)return false;
            letter=letter||alpha;
        }
        host[j++]=(char)ch;
    }
    if(j==0 || label==0 || host[j-1]=='-' || !letter)return false;
    host[j]=0;
    if(s[i]==':') {
        i++;start=i;number=0;
        while(s[i]>='0'&&s[i]<='9') {number=number*10+(unsigned)(s[i++]-'0');if(number>65535)return false;}
        if(i==start || number==0)return false;
    }
    if(s[i]!=0 && s[i]!='/')return false;
    if(strchr(s+i,'#')!=NULL)return false;
    if(path!=NULL) {
        if(s[i]==0)strcpy(path,"/");else strcpy(path,s+i);
    }
    *port=(uint16_t)number;return true;
}

static bool utf8(const uint8_t *data,size_t length)
{
    size_t i=0;
    while(i<length) {
        unsigned ch=data[i++],count,minimum,code,k;
        if(ch<0x80)continue;
        if(ch>=0xc2 && ch<=0xdf) {count=1;minimum=0x80;code=ch&31;}
        else if(ch>=0xe0 && ch<=0xef) {count=2;minimum=0x800;code=ch&15;}
        else if(ch>=0xf0 && ch<=0xf4) {count=3;minimum=0x10000;code=ch&7;}
        else return false;
        if(length-i<count)return false;
        for(k=0;k<count;k++) {
            ch=data[i++];if((ch&0xc0)!=0x80)return false;
            code=(code<<6)|(ch&63);
        }
        if(code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff))return false;
    }
    return true;
}
static bool jsonNumber(const char *s,size_t n,size_t *position)
{
    size_t i=*position;
    if(s[i]=='-') {if(++i==n)return false;}
    if(s[i]=='0') {i++;if(i<n && s[i]>='0' && s[i]<='9')return false;}
    else {
        if(s[i]<'1' || s[i]>'9')return false;
        while(i<n && s[i]>='0' && s[i]<='9')i++;
    }
    if(i<n && s[i]=='.') {
        if(++i==n || s[i]<'0' || s[i]>'9')return false;
        while(i<n && s[i]>='0' && s[i]<='9')i++;
    }
    if(i<n && (s[i]=='e' || s[i]=='E')) {
        i++;if(i<n && (s[i]=='+' || s[i]=='-'))i++;
        if(i==n || s[i]<'0' || s[i]>'9')return false;
        while(i<n && s[i]>='0' && s[i]<='9')i++;
    }
    if(i<n && strchr(",]} \t\r\n",s[i])==NULL)return false;
    *position=i-1;return true;
}
/* Bound recursion/allocation before invoking the project's shared cJSON parser.
 * Reject raw controls, decoded NUL, excessive depth and structural complexity. */
static bool jsonBounds(const char *s,size_t n)
{
    size_t i,depth=0,complexity=0;
    bool quoted=false,escaped=false;
    if(s==NULL || n==0 || n>S2_SESSION_JSON_MAX || !utf8((const uint8_t*)s,n))return false;
    for(i=0;i<n;i++) {
        unsigned ch=(unsigned char)s[i];
        if(ch==0)return false;
        if(quoted) {
            if(ch<32)return false;
            if(escaped) {
                if(ch=='u') {
                    if(i+4>=n)return false;
                    if(memcmp(s+i+1,"0000",4)==0)return false;
                }
                escaped=false;
            } else if(ch=='\\')escaped=true;else if(ch=='"')quoted=false;
        } else {
            if(ch=='"')quoted=true;
            else if(ch=='-' || (ch>='0' && ch<='9')) {if(!jsonNumber(s,n,&i))return false;}
            else if(ch=='{' || ch=='[') {if(++depth>16 || ++complexity>128)return false;}
            else if(ch=='}' || ch==']') {if(depth==0)return false;depth--;}
            else if(ch==',' || ch==':') {if(++complexity>128)return false;}
            else if(ch<32 && ch!='\t' && ch!='\r' && ch!='\n')return false;
        }
    }
    return !quoted && !escaped && depth==0;
}
static bool uniqueKeys(const cJSON *item)
{
    const cJSON *a,*b;
    for(a=item->child;a!=NULL;a=a->next) {
        if(cJSON_IsObject(item))for(b=a->next;b!=NULL;b=b->next)
            if(a->string!=NULL && b->string!=NULL && strcmp(a->string,b->string)==0)return false;
        if(!uniqueKeys(a))return false;
    }
    return true;
}
static void wipeJson(cJSON *item)
{
    cJSON *child;
    for(child=item->child;child!=NULL;child=child->next)wipeJson(child);
    if(item->valuestring!=NULL)erase(item->valuestring,strlen(item->valuestring));
}
static cJSON *parse(const char *s,size_t n)
{
    const char *end;
    cJSON *j;
    if(!jsonBounds(s,n))return NULL;
    j=cJSON_ParseWithLengthOpts(s,n,&end,false);
    if(j==NULL)return NULL;
    while(end<s+n && (*end==' ' || *end=='\r' || *end=='\n' || *end=='\t'))end++;
    if(end!=s+n || !cJSON_IsObject(j) || !uniqueKeys(j)) {
        wipeJson(j);cJSON_Delete(j);return NULL;
    }
    return j;
}
static const char *string(cJSON *j,const char *key)
{
    cJSON *value=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsString(value)?value->valuestring:NULL;
}

bool S2_SESSION_Configuration(const char *base,const char *client,const char *server,
    char host[254],uint16_t *port)
{
    size_t n;
    if(host==NULL || port==NULL)return false;
    host[0]=0;*port=0;
    if(!uuid(client) || !uuid(server) || !url(base,"https://",host,port,NULL))goto invalid;
    n=strlen(base);while(n!=0 && base[n-1]=='/')n--;
    if(n<3 || memcmp(base+n-3,"/v1",3)!=0 || strchr(base,'?')!=NULL)goto invalid;
    return true;
invalid:
    host[0]=0;*port=0;return false;
}

S2_SESSION_RESULT S2_SESSION_Begin(S2_SESSION *c,const S2_SESSION_STORAGE *storage,
    const char *base,const char *client,const char *server,const S2_SESSION_TOKENS *saved,uint32_t now)
{
    size_t i,n;char host[254],baseCopy[S2_SESSION_URL_SIZE],clientCopy[37],serverCopy[37];uint16_t port;
    S2_SESSION_STORAGE storageCopy;S2_SESSION_TOKENS savedCopy;
    if(c==NULL || storage==NULL || storage->persist==NULL || saved==NULL ||
        saved->count==0 || saved->count>2 ||
        !S2_SESSION_Configuration(base,client,server,host,&port))return S2_SESSION_INVALID;
    if(c->state==S2_SESSION_INITIATE || c->state==S2_SESSION_CONFIRM || c->state==S2_SESSION_READY)
        return S2_SESSION_INVALID;
    for(i=0;i<saved->count;i++)if(!token(saved->token[i]))return S2_SESSION_INVALID;
    if(saved->count==2 && strcmp(saved->token[0],saved->token[1])==0)return S2_SESSION_INVALID;
    n=strlen(base);while(n!=0 && base[n-1]=='/')n--;
    storageCopy=*storage;savedCopy=*saved;
    memcpy(baseCopy,base,n);baseCopy[n]=0;strcpy(clientCopy,client);strcpy(serverCopy,server);
    S2_SESSION_Clear(c);c->storage=storageCopy;c->tokens=savedCopy;erase(&savedCopy,sizeof(savedCopy));
    strcpy(c->base,baseCopy);strcpy(c->client,clientCopy);strcpy(c->server,serverCopy);
    c->phaseStart=now;c->state=S2_SESSION_INITIATE;return S2_SESSION_OK;
}
void S2_SESSION_Tasks(S2_SESSION *c,uint32_t now)
{
    if(c==NULL)return;
    if((c->state==S2_SESSION_INITIATE || c->state==S2_SESSION_CONFIRM) &&
        (uint32_t)(now-c->phaseStart)>=S2_SESSION_PHASE_MS)failed(c,S2_SESSION_EXPIRED);
    else if(c->state==S2_SESSION_READY && (uint32_t)(now-c->commStart)>=S2_SESSION_COMM_MS)
        failed(c,S2_SESSION_EXPIRED);
}
S2_SESSION_RESULT S2_SESSION_Request(S2_SESSION *c,S2_SESSION_REQUEST *q,uint32_t now)
{
    int n;
    if(c==NULL || q==NULL)return S2_SESSION_INVALID;
    memset(q,0,sizeof(*q));
    S2_SESSION_Tasks(c,now);
    if(c->state!=S2_SESSION_INITIATE && c->state!=S2_SESSION_CONFIRM)
        return c->state==S2_SESSION_FAILED?c->error:S2_SESSION_INVALID;
    q->method="POST";
    n=snprintf(q->url,sizeof(q->url),"%s/%s",c->base,
        c->state==S2_SESSION_INITIATE?"initiateSession":"confirmAccessToken");
    if(n<0 || (size_t)n>=sizeof(q->url))return failed(c,S2_SESSION_INVALID);
    strcpy(q->bearer,c->tokens.token[c->state==S2_SESSION_INITIATE?c->tryToken:1]);
    if(c->state==S2_SESSION_INITIATE) {
        n=snprintf(q->body,sizeof(q->body),"{\"clientNodeId\":\"%s\",\"serverNodeId\":\"%s\","
            "\"supportedS2MessageVersions\":[\"%s\"],\"supportedCommunicationProtocols\":[\"WebSocket\"]}",
            c->client,c->server,S2_SESSION_MESSAGE_VERSION);
        if(n<0 || (size_t)n>=sizeof(q->body))return failed(c,S2_SESSION_INVALID);
    }
    if(!c->requestIssued) {c->requestIssued=true;c->requestStart=now;}
    return S2_SESSION_OK;
}
S2_SESSION_RESULT S2_SESSION_Response(S2_SESSION *c,unsigned http,const char *body,size_t length,
    bool tlsVerified,uint32_t now)
{
    cJSON *j;
    const char *value;
    S2_SESSION_RESULT result=S2_SESSION_INVALID;
    S2_SESSION_TOKENS next={0};
    if(c==NULL)return S2_SESSION_INVALID;
    S2_SESSION_Tasks(c,now);
    if(c->state==S2_SESSION_FAILED)return c->error;
    if((c->state!=S2_SESSION_INITIATE && c->state!=S2_SESSION_CONFIRM) || !c->requestIssued)
        return S2_SESSION_INVALID;
    if(!tlsVerified)return failed(c,S2_SESSION_TLS_ERROR);
    if(http==401 && c->state==S2_SESSION_INITIATE && c->tryToken+1<c->tokens.count) {
        c->tryToken++;c->requestIssued=false;c->phaseStart=now;return S2_SESSION_OK;
    }
    if(http==401)return failed(c,S2_SESSION_AUTH_ERROR);
    j=parse(body,length);
    if(http!=200) {
        value=j!=NULL?string(j,"errorMessage"):NULL;
        result=value!=NULL && strcmp(value,"NoLongerPaired")==0?S2_SESSION_UNPAIRED:S2_SESSION_HTTP_ERROR;
        if(j!=NULL) {wipeJson(j);cJSON_Delete(j);}return failed(c,result);
    }
    if(j==NULL)return failed(c,S2_SESSION_INVALID);
    if(c->state==S2_SESSION_INITIATE) {
        value=string(j,"selectedCommunicationProtocol");
        if(value==NULL || strcmp(value,"WebSocket")!=0)goto done;
        value=string(j,"selectedS2MessageVersion");
        if(value==NULL || strcmp(value,S2_SESSION_MESSAGE_VERSION)!=0)goto done;
        value=string(j,"accessToken");
        if(!token(value) || strcmp(value,c->tokens.token[c->tryToken])==0)goto done;
        next.count=2;strcpy(next.token[0],c->tokens.token[c->tryToken]);strcpy(next.token[1],value);
        if(!c->storage.persist(c->storage.user,&next)) {result=S2_SESSION_STORAGE_ERROR;goto done;}
        c->tokens=next;c->state=S2_SESSION_CONFIRM;c->requestIssued=false;
        /* Conservative pending-token deadline starts at initiate dispatch. */
        c->phaseStart=c->requestStart;result=S2_SESSION_OK;
    } else {
        char host[254];uint16_t port;
        value=string(j,"communicationProtocol");if(value==NULL || strcmp(value,"WebSocket")!=0)goto done;
        value=string(j,"websocketToken");if(!token(value))goto done;strcpy(c->details.commToken,value);
        value=string(j,"websocketUrl");
        if(!url(value,"wss://",c->details.hostname,&c->details.port,c->details.path) ||
            !url(c->base,"https://",host,&port,NULL) || !sameHost(host,c->details.hostname) || port!=c->details.port)
            goto done;
        next.count=1;strcpy(next.token[0],c->tokens.token[1]);
        if(!c->storage.persist(c->storage.user,&next)) {result=S2_SESSION_STORAGE_ERROR;goto done;}
        c->tokens=next;c->commStart=c->requestStart;c->state=S2_SESSION_READY;result=S2_SESSION_OK;
    }
done:
    erase(&next,sizeof(next));wipeJson(j);cJSON_Delete(j);
    return result==S2_SESSION_OK?result:failed(c,result);
}
S2_SESSION_RESULT S2_SESSION_TakeDetails(S2_SESSION *c,S2_SESSION_DETAILS *out,uint32_t now)
{
    if(c==NULL || out==NULL || out==&c->details)return S2_SESSION_INVALID;
    memset(out,0,sizeof(*out));
    S2_SESSION_Tasks(c,now);
    if(c->state!=S2_SESSION_READY)return c->state==S2_SESSION_FAILED?c->error:S2_SESSION_INVALID;
    c->details.lifetimeMs=S2_SESSION_COMM_MS-(uint32_t)(now-c->commStart);
    *out=c->details;erase(&c->details,sizeof(c->details));c->state=S2_SESSION_CONSUMED;
    return S2_SESSION_OK;
}
