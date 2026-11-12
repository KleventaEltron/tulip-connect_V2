#include "app_s2_session_tasks.h"
#include "files/network_manager.h"
#include "system/time/sys_time.h"
#include "library/tcpip/tcpip.h"
#include <string.h>
static APP_S2_SESSION_WORKSPACE *workspace;
static APP_S2_SESSION_STATUS status;
static bool initialized,registered;
static void erase(void *p,size_t n) { volatile uint8_t *b=p;while(n--)*b++=0; }
static uint32_t nowMilliseconds(void) {
    uint64_t n=SYS_TIME_Counter64Get();uint32_t hz=SYS_TIME_FrequencyGet();
    return hz?(uint32_t)((n/hz)*1000u+(n%hz)*1000u/hz):0;
}
static bool noText(void *user,const char *text,size_t length) {
    (void)user;(void)text;(void)length;return false;
}
static void clearAttempt(void) {
    S2_HTTPS_Stop(&workspace->http);
    S2_TLS_Close(&workspace->httpsTls);
    S2_SESSION_Clear(&workspace->session);
    erase(&workspace->saved,sizeof(workspace->saved));
    erase(&workspace->details,sizeof(workspace->details));
    workspace->ipv4=0;
}
static void failure(APP_S2_SESSION_ERROR error,bool permanent) {
    clearAttempt();status.error=error;status.consecutiveFailures++;
    status.retryDelayMs=0;
    if(permanent || status.consecutiveFailures>=APP_S2_SESSION_MAX_FAILURES) {
        status.state=APP_S2_SESSION_BLOCKED;return;
    }
    status.retryDelayMs=5000u << (status.consecutiveFailures-1u);
    if(status.retryDelayMs>60000u)status.retryDelayMs=60000u;
    workspace->retryAt=nowMilliseconds();status.state=APP_S2_SESSION_RETRY_WAIT;
}
static void networkDown(void) {
    if(workspace==NULL)return;
    switch(status.state) {
        case APP_S2_SESSION_DISABLED:
        case APP_S2_SESSION_BLOCKED:
        case APP_S2_SESSION_RETRY_WAIT:
        case APP_S2_SESSION_WAIT_NETWORK:
            break;
        default:
            /* Existing APP_S2 callback also closes its own channel socket. */
            failure(APP_S2_SESSION_ERROR_NETWORK,false);break;
    }
}
bool APP_S2_SESSION_Initialize(void) {
    if(initialized)return registered;
    initialized=true;registered=NETWORK_MANAGER_RegisterDownCallback(networkDown);
    memset(&status,0,sizeof(status));status.state=APP_S2_SESSION_DISABLED;
    if(!registered)status.error=APP_S2_SESSION_ERROR_REGISTRATION;
    return registered;
}
bool APP_S2_SESSION_Configure(APP_S2_SESSION_WORKSPACE *w,const APP_S2_SESSION_CONFIG *c) {
    char host[254];uint16_t port;
    if(!initialized || !registered || workspace!=NULL || w==NULL || c==NULL ||
       APP_S2_GetStatus().state!=APP_S2_DISABLED || !SYS_TIME_FrequencyGet() ||
       c->caDer==NULL || c->caLength==0 || c->caLength>APP_S2_CA_MAX || c->text==NULL ||
       c->tokens==NULL || !c->tokens->io.read || !c->tokens->io.write || !c->tokens->io.sync ||
       !S2_SESSION_Configuration(c->apiBase,c->clientNodeId,c->serverNodeId,host,&port))return false;
    if(strlen(host)>=TCPIP_DNS_CLIENT_MAX_HOSTNAME_LEN)return false;
    memset(w,0,sizeof(*w));strcpy(w->apiBase,c->apiBase);
    strcpy(w->client,c->clientNodeId);strcpy(w->server,c->serverNodeId);
    strcpy(w->hostname,host);w->port=port;w->store=*c->tokens;w->configuredIpv4=c->ipv4Override;
    w->caLength=c->caLength;
    if(!S2_TLS_Initialize(&w->httpsTls,NULL,noText) ||
       !APP_S2_Attach(&w->channel,c->consumer,c->text)) {
        S2_TLS_Close(&w->httpsTls);erase(w,sizeof(*w));return false;
    }
    memcpy(w->channel.ca,c->caDer,c->caLength);
    workspace=w;memset(&status,0,sizeof(status));status.state=APP_S2_SESSION_WAIT_NETWORK;
    return true;
}
void APP_S2_SESSION_Stop(void) {
    if(workspace!=NULL) {
        clearAttempt();APP_S2_Disable();erase(workspace,sizeof(*workspace));workspace=NULL;
    }
    status.state=APP_S2_SESSION_DISABLED;status.retryDelayMs=0;
}
static void sessionFailure(void) {
    status.sessionResult=workspace->session.error;
    switch(status.sessionResult) {
        case S2_SESSION_AUTH_ERROR: failure(APP_S2_SESSION_ERROR_AUTH,true);break;
        case S2_SESSION_UNPAIRED: failure(APP_S2_SESSION_ERROR_UNPAIRED,true);break;
        case S2_SESSION_STORAGE_ERROR: failure(APP_S2_SESSION_ERROR_STORAGE,true);break;
        case S2_SESSION_INVALID: failure(APP_S2_SESSION_ERROR_PROTOCOL,true);break;
        case S2_SESSION_EXPIRED: failure(APP_S2_SESSION_ERROR_EXPIRED,false);break;
        default: failure(APP_S2_SESSION_ERROR_HTTP,false);break;
    }
}
static bool startHttp(uint32_t now) {
    if(!S2_HTTPS_Begin(&workspace->http,&workspace->httpsTls,workspace->port,&workspace->session,now)) {
        status.httpError=workspace->http.error;
        if(workspace->session.state==S2_SESSION_FAILED)sessionFailure();
        else failure(APP_S2_SESSION_ERROR_HTTP,false);
        return false;
    }
    status.state=APP_S2_SESSION_HTTP;return true;
}
void APP_S2_SESSION_Tasks(void) {
    APP_S2_SESSION_WORKSPACE *w=workspace;uint32_t now;
    if(!initialized || !registered || w==NULL)return;
    switch(status.state) {
        case APP_S2_SESSION_DISABLED:
        case APP_S2_SESSION_BLOCKED: return;
        default: break;
    }
    if(!SYS_TIME_FrequencyGet()) {
        APP_S2_Disable();failure(APP_S2_SESSION_ERROR_CONFIGURATION,true);return;
    }
    now=nowMilliseconds();
    if(!NETWORK_MANAGER_IsReady()) {networkDown();return;}
    S2_SESSION_Tasks(&w->session,now);
    if(w->session.state==S2_SESSION_FAILED) {sessionFailure();return;}
    switch(status.state) {
        case APP_S2_SESSION_WAIT_NETWORK:
            status.state=APP_S2_SESSION_WAIT_TIME;break;
        case APP_S2_SESSION_WAIT_TIME:
            if(S2_TLS_ClockReady())status.state=APP_S2_SESSION_LOAD;
            break;
        case APP_S2_SESSION_LOAD: {
            TCPIP_DNS_RESULT dns;
            status.attempts++;status.storageResult=S2_TOKEN_STORE_Load(&w->store,&w->saved);
            if(status.storageResult!=S2_TOKEN_STORE_OK) {
                failure(status.storageResult==S2_TOKEN_STORE_EMPTY?APP_S2_SESSION_ERROR_UNPAIRED:APP_S2_SESSION_ERROR_STORAGE,true);break;
            }
            if(w->saved.count==0) {failure(APP_S2_SESSION_ERROR_UNPAIRED,true);break;}
            if(w->configuredIpv4) {
                w->ipv4=w->configuredIpv4;status.state=APP_S2_SESSION_HTTPS_CONNECT;break;
            }
            dns=TCPIP_DNS_Resolve(w->hostname,TCPIP_DNS_TYPE_A);status.dnsResult=(int)dns;
            if(dns<0) {failure(APP_S2_SESSION_ERROR_DNS,false);break;}
            w->phaseAt=nowMilliseconds();status.state=APP_S2_SESSION_DNS;break;
        }
        case APP_S2_SESSION_DNS: {
            IP_MULTI_ADDRESS ip;TCPIP_DNS_RESULT dns;
            if((uint32_t)(now-w->phaseAt)>=APP_S2_SESSION_DNS_MS) {failure(APP_S2_SESSION_ERROR_DNS,false);break;}
            memset(&ip,0,sizeof(ip));dns=TCPIP_DNS_IsResolved(w->hostname,&ip,IP_ADDRESS_TYPE_IPV4);
            status.dnsResult=(int)dns;
            switch(dns) {
                case TCPIP_DNS_RES_PENDING: break;
                case TCPIP_DNS_RES_OK:
                    if(ip.v4Add.Val==0) {failure(APP_S2_SESSION_ERROR_DNS,false);break;}
                    w->ipv4=ip.v4Add.Val;status.state=APP_S2_SESSION_HTTPS_CONNECT;break;
                default: failure(APP_S2_SESSION_ERROR_DNS,false);break;
            }
            break;
        }
        case APP_S2_SESSION_HTTPS_CONNECT:
            if(!S2_TLS_Start(&w->httpsTls,w->ipv4,w->port,w->hostname,w->channel.ca,w->caLength,now))goto tlsFailure;
            status.state=APP_S2_SESSION_HTTPS_WAIT;break;
        case APP_S2_SESSION_HTTPS_WAIT:
            S2_TLS_Tasks(&w->httpsTls,now);
            switch(w->httpsTls.state) {
                case S2_TLS_FAILED: goto tlsFailure;
                case S2_TLS_VERIFIED:
                    if(w->session.state==S2_SESSION_IDLE) {
                        S2_SESSION_STORAGE storage={&w->store,S2_TOKEN_STORE_Persist};
                        status.sessionResult=S2_SESSION_Begin(&w->session,&storage,w->apiBase,w->client,w->server,&w->saved,now);
                        erase(&w->saved,sizeof(w->saved));
                        if(status.sessionResult!=S2_SESSION_OK) {failure(APP_S2_SESSION_ERROR_CONFIGURATION,true);break;}
                    }
                    (void)startHttp(now);break;
                default: break;
            }
            break;
        case APP_S2_SESSION_HTTP:
            S2_HTTPS_Tasks(&w->http,now);
            status.httpStatus=w->http.httpStatus;
            switch(w->http.state) {
                case S2_HTTPS_FAILED:
                    status.httpError=w->http.error;
                    status.tlsError=w->httpsTls.error;status.wolfError=w->httpsTls.wolfError;
                    if(w->session.state==S2_SESSION_FAILED)sessionFailure();
                    else failure(APP_S2_SESSION_ERROR_HTTP,false);
                    break;
                case S2_HTTPS_DONE:
                    switch(w->session.state) {
                        case S2_SESSION_READY: status.state=APP_S2_SESSION_HANDOFF;break;
                        case S2_SESSION_INITIATE:
                        case S2_SESSION_CONFIRM:
                            if(w->http.reusable && w->httpsTls.state==S2_TLS_VERIFIED)(void)startHttp(nowMilliseconds());
                            else {S2_HTTPS_Stop(&w->http);status.state=APP_S2_SESSION_HTTPS_CONNECT;}
                            break;
                        default: failure(APP_S2_SESSION_ERROR_PROTOCOL,true);break;
                    }
                    break;
                default: break;
            }
            break;
        case APP_S2_SESSION_HANDOFF: {
            APP_S2_SESSION supplied;uint32_t exported,elapsed;
            S2_HTTPS_Stop(&w->http);exported=nowMilliseconds();
            status.sessionResult=S2_SESSION_TakeDetails(&w->session,&w->details,exported);
            if(status.sessionResult!=S2_SESSION_OK) {sessionFailure();break;}
            memset(&supplied,0,sizeof(supplied));supplied.ipv4=w->ipv4;
            supplied.port=w->details.port;supplied.hostname=w->details.hostname;
            supplied.path=w->details.path;supplied.commToken=w->details.commToken;
            supplied.caDer=w->channel.ca;supplied.caLength=w->caLength;
            elapsed=(uint32_t)(nowMilliseconds()-exported);
            if(elapsed>=w->details.lifetimeMs || w->details.lifetimeMs-elapsed<=1u) {
                failure(APP_S2_SESSION_ERROR_EXPIRED,false);break;
            }
            supplied.lifetimeMs=w->details.lifetimeMs-elapsed-1u;
            if(!APP_S2_ProvideSession(&supplied)) {failure(APP_S2_SESSION_ERROR_CHANNEL,true);break;}
            erase(&w->details,sizeof(w->details));S2_SESSION_Clear(&w->session);
            status.state=APP_S2_SESSION_CHANNEL;break;
        }
        case APP_S2_SESSION_CHANNEL: {
            APP_S2_STATUS channel=APP_S2_GetStatus();
            switch(channel.state) {
                case APP_S2_WS_OPEN:
                    status.consecutiveFailures=0;status.error=APP_S2_SESSION_ERROR_NONE;break;
                case APP_S2_WAIT_SESSION:
                    failure(APP_S2_SESSION_ERROR_CHANNEL,false);break;
                case APP_S2_DISABLED:
                case APP_S2_NOT_INITIALIZED:
                    failure(APP_S2_SESSION_ERROR_CHANNEL,true);break;
                default: break;
            }
            break;
        }
        case APP_S2_SESSION_RETRY_WAIT:
            if((uint32_t)(now-w->retryAt)>=status.retryDelayMs) {
                status.retryDelayMs=0;status.state=APP_S2_SESSION_WAIT_NETWORK;
            }
            break;
        case APP_S2_SESSION_DISABLED:
        case APP_S2_SESSION_BLOCKED:
        default: break;
    }
    return;
tlsFailure:
    status.tlsError=w->httpsTls.error;status.wolfError=w->httpsTls.wolfError;
    failure(APP_S2_SESSION_ERROR_TLS,w->httpsTls.error==S2_TLS_ERROR_TRUST || w->httpsTls.error==S2_TLS_ERROR_ARGUMENT);
}
APP_S2_SESSION_STATUS APP_S2_SESSION_GetStatus(void) { return status; }
