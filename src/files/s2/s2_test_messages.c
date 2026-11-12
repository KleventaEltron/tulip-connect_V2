#include "s2_test_messages.h"
#include "files/cJSON.h"
#include <stdio.h>
#include <string.h>
static void erase(void *p,size_t n) { volatile uint8_t *b=p;while(n--)*b++=0; }
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
    if(s==NULL || n==0 || n>S2_TEST_MESSAGE_MAX || !utf8((const uint8_t*)s,n))return false;
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
static const char *str(const cJSON *j,const char *name) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,name);
    return cJSON_IsString(v)?v->valuestring:NULL;
}
static bool id(const char *s) {
    size_t i;if(!s)return false;
    for(i=0;i<=64;i++) {
        unsigned c=(unsigned char)s[i];if(!c)return i>=2;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c==':'))return false;
    }
    return false;
}
static bool fields(const cJSON *j,const char *a,const char *b,const char *c,const char *d) {
    const cJSON *v;for(v=j->child;v;v=v->next) {
        const char *s=v->string;
        if(!s || (strcmp(s,a)&&strcmp(s,b)&&strcmp(s,c)&&strcmp(s,d)))return false;
    }
    return true;
}
static bool uuid(S2_TEST_MESSAGES *c,char out[37]) {
    uint8_t b[16];unsigned i,k=0;
    static const char hex[]="0123456789abcdef";
    if(!c->random(NULL,b,sizeof(b)))return false;
    b[6]=(uint8_t)((b[6]&15)|64);b[8]=(uint8_t)((b[8]&63)|128);
    for(i=0;i<16;i++) {
        if(i==4||i==6||i==8||i==10)out[k++]='-';
        out[k++]=hex[b[i]>>4];out[k++]=hex[b[i]&15];
    }
    out[k]=0;erase(b,sizeof(b));return true;
}
static char *reserve(S2_TEST_MESSAGES *c) {
    if(c->count==S2_TEST_QUEUE_DEPTH) {c->failed=true;return NULL;}
    return c->queue[(c->head+c->count++)%S2_TEST_QUEUE_DEPTH];
}
static bool ack(S2_TEST_MESSAGES *c,const char *subject,const char *status) {
    char *q=reserve(c);if(!q)return false;
    (void)snprintf(q,S2_TEST_OUTPUT_MAX,"{\"message_type\":\"ReceptionStatus\",\"subject_message_id\":\"%s\",\"status\":\"%s\"}",subject,status);return true;
}
static bool details(S2_TEST_MESSAGES *c) {
    char *q=reserve(c);int n;if(!q||!uuid(c,c->detailsId))return false;
    n=snprintf(q,S2_TEST_OUTPUT_MAX,"{\"message_type\":\"ResourceManagerDetails\",\"message_id\":\"%s\",\"resource_id\":\"%s\","
        "\"name\":\"Tulip communication test - simulated resource\",\"roles\":[{\"role\":\"ENERGY_CONSUMER\",\"commodity\":\"ELECTRICITY\"}],"
        "\"instruction_processing_delay\":0,\"available_control_types\":[\"NOT_CONTROLABLE\"],\"provides_forecast\":false,"
        "\"provides_power_measurement_types\":[\"ELECTRIC.POWER.L1\"]}",c->detailsId,c->resource);
    return n>0&&(size_t)n<S2_TEST_OUTPUT_MAX;
}
void S2_TEST_Reset(S2_TEST_MESSAGES *c) { if(c)erase(c,sizeof(*c)); }
bool S2_TEST_Start(S2_TEST_MESSAGES *c,const char *resource,bool (*random)(void *,uint8_t *,size_t)) {
    char *q;if(!c||!id(resource)||!random)return false;
    S2_TEST_Reset(c);c->random=random;strcpy(c->resource,resource);c->active=true;
    if(!uuid(c,c->handshakeId)) {c->failed=true;return false;}
    q=reserve(c);if(!q)return false;
    (void)snprintf(q,S2_TEST_OUTPUT_MAX,"{\"message_type\":\"Handshake\",\"message_id\":\"%s\",\"role\":\"RM\",\"supported_protocol_versions\":[\"%s\"]}",c->handshakeId,S2_TEST_VERSION);
    return true;
}
bool S2_TEST_Receive(void *user,const char *text,size_t length) {
    S2_TEST_MESSAGES *c=user;cJSON *j;const char *type,*message,*value;bool ok=false;
    if(!c||!c->active||c->failed)return false;
    j=parse(text,length);if(!j) {c->failed=true;return false;}
    c->received++;type=str(j,"message_type");message=str(j,"message_id");
    if(!type)goto done;
    if(!strcmp(type,"ReceptionStatus")) {
        const char *subject=str(j,"subject_message_id");value=str(j,"status");
        const cJSON *label=cJSON_GetObjectItemCaseSensitive(j,"diagnostic_label");
        if(label&&!cJSON_IsString(label))goto done;
        if(!id(subject)||!value||!fields(j,"message_type","subject_message_id","status","diagnostic_label"))goto done;
        if(strcmp(value,"OK")&&strcmp(value,"INVALID_DATA")&&strcmp(value,"INVALID_MESSAGE")&&strcmp(value,"INVALID_CONTENT")&&strcmp(value,"TEMPORARY_ERROR")&&strcmp(value,"PERMANENT_ERROR"))goto done;
        if(!strcmp(subject,c->handshakeId)||!strcmp(subject,c->detailsId)||!strcmp(subject,c->measurementId)) {
            if(strcmp(value,"OK"))goto done;
            if(!strcmp(subject,c->handshakeId))c->handshakeAck=true;
            else if(!strcmp(subject,c->detailsId))c->detailsAck=true;
            else c->measurementAck=true;
        }
        ok=true;goto done; /* Never acknowledge an acknowledgement. */
    }
    if(!id(message))goto done;
    if(!strcmp(type,"Handshake")) {
        const cJSON *versions=cJSON_GetObjectItemCaseSensitive(j,"supported_protocol_versions"),*v;
        bool version=false;
        value=str(j,"role");
        if(!fields(j,"message_type","message_id","role","supported_protocol_versions")||!value||strcmp(value,"CEM")) {ok=ack(c,message,"INVALID_MESSAGE");goto done;}
        if(versions) {
            if(!cJSON_IsArray(versions)||!versions->child) {ok=ack(c,message,"INVALID_MESSAGE");goto done;}
            for(v=versions->child;v;v=v->next) {
                if(!cJSON_IsString(v)) {ok=ack(c,message,"INVALID_MESSAGE");goto done;}
                if(!strcmp(v->valuestring,S2_TEST_VERSION))version=true;
            }
            if(!version) {ok=ack(c,message,"INVALID_CONTENT");goto done;}
        }
        if(c->cemHandshake&&strcmp(message,c->cemId)) {ok=ack(c,message,"INVALID_CONTENT");goto done;}
        c->cemHandshake=true;strcpy(c->cemId,message);ok=ack(c,message,"OK");
    } else if(!strcmp(type,"HandshakeResponse")) {
        value=str(j,"selected_protocol_version");
        if(!fields(j,"message_type","message_id","selected_protocol_version","selected_protocol_version")||!value) {ok=ack(c,message,"INVALID_MESSAGE");goto done;}
        if(!c->cemHandshake||strcmp(value,S2_TEST_VERSION)||(c->negotiated&&strcmp(message,c->responseId))) {ok=ack(c,message,"INVALID_CONTENT");goto done;}
        ok=ack(c,message,"OK");
        if(ok&&!c->negotiated) {c->negotiated=true;strcpy(c->responseId,message);ok=details(c);}
    } else if(!strcmp(type,"SelectControlType")) {
        value=str(j,"control_type");
        if(!fields(j,"message_type","message_id","control_type","control_type")||!value)ok=ack(c,message,"INVALID_MESSAGE");
        else if(!c->negotiated||strcmp(value,"NOT_CONTROLABLE")) {c->rejectedControl=true;c->rejected++;ok=ack(c,message,"INVALID_CONTENT");}
        else {c->selected=true;ok=ack(c,message,"OK");}
    } else {c->rejected++;ok=ack(c,message,"INVALID_CONTENT");}
 done:
    wipeJson(j);cJSON_Delete(j);if(!ok)c->failed=true;return ok;
}
const char *S2_TEST_Peek(const S2_TEST_MESSAGES *c) { return c&&c->count?c->queue[c->head]:NULL; }
void S2_TEST_Sent(S2_TEST_MESSAGES *c) {
    if(c&&c->count) {erase(c->queue[c->head],S2_TEST_OUTPUT_MAX);c->head=(c->head+1)%S2_TEST_QUEUE_DEPTH;c->count--;c->sent++;}
}

bool S2_TEST_Measurement(S2_TEST_MESSAGES *c,const char *utc) {
    char *q;size_t i;
    if(!c||!c->active||c->failed||!c->negotiated||!utc||strlen(utc)!=20||c->measurementId[0])return false;
    for(i=0;i<20;i++) {
        char expected=i==4||i==7?'-':i==10?'T':i==13||i==16?':':i==19?'Z':0;
        if(expected?utc[i]!=expected:(utc[i]<'0'||utc[i]>'9'))return false;
    }
    if(!uuid(c,c->measurementId))return false;
    q=reserve(c);if(!q)return false;
    (void)snprintf(q,S2_TEST_OUTPUT_MAX,"{\"message_type\":\"PowerMeasurement\",\"message_id\":\"%s\",\"measurement_timestamp\":\"%s\",\"values\":[{\"commodity_quantity\":\"ELECTRIC.POWER.L1\",\"value\":0}]}",c->measurementId,utc);
    return true;
}
