#include "s2_test_messages.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static S2_TEST_MESSAGES context;
static unsigned sequence;
static bool entropyFails;
static bool randomBytes(void *u,uint8_t *p,size_t n) {
    size_t i;(void)u;if(entropyFails)return false;
    for(i=0;i<n;i++)p[i]=(uint8_t)(sequence+i);sequence++;return true;
}
static void drain(bool output) {
    const char *p;
    while((p=S2_TEST_Peek(&context))!=NULL) {if(output)puts(p);S2_TEST_Sent(&context);}
}
static bool input(const char *p) {return S2_TEST_Receive(&context,p,strlen(p));}
int main(int argc,char **argv) {
    char line[2048];(void)argv;
    assert(S2_TEST_Start(&context,"simulated-test-resource",randomBytes));
    if(argc>1) {
        drain(true);puts("#READY");fflush(stdout);
        while(fgets(line,sizeof(line),stdin)) {
            if(!input(line))return 2;
            if(context.selected&&context.detailsAck&&!context.measurementId[0])
                assert(S2_TEST_Measurement(&context,"2026-11-12T12:00:00Z"));
            drain(true);
            if(context.handshakeAck&&context.detailsAck&&context.measurementAck&&context.selected&&context.rejectedControl)puts("#PASS");
            puts("#READY");fflush(stdout);
        }
        return 0;
    }
    assert(strstr(S2_TEST_Peek(&context),"\"role\":\"RM\""));drain(false);
    assert(input("{\"message_type\":\"Handshake\",\"message_id\":\"cem-01\",\"role\":\"CEM\"}"));
    assert(strstr(S2_TEST_Peek(&context),"\"status\":\"OK\""));drain(false);
    assert(input("{\"message_type\":\"HandshakeResponse\",\"message_id\":\"cem-02\",\"selected_protocol_version\":\"v1.0.0\"}"));
    assert(context.negotiated&&context.count==2);drain(false);
    assert(input("{\"message_type\":\"SelectControlType\",\"message_id\":\"cem-03\",\"control_type\":\"FILL_RATE_BASED_CONTROL\"}"));
    assert(!context.selected&&context.rejectedControl&&context.rejected==1&&strstr(S2_TEST_Peek(&context),"INVALID_CONTENT"));drain(false);
    assert(input("{\"message_type\":\"SelectControlType\",\"message_id\":\"cem-04\",\"control_type\":\"NOT_CONTROLABLE\"}"));assert(context.selected);drain(false);
    assert(input("{\"message_type\":\"ReceptionStatus\",\"subject_message_id\":\"unrelated-id\",\"status\":\"OK\"}"));assert(context.count==0);
    assert(!input("{\"message_type\":\"Handshake\",\"message_type\":\"SelectControlType\"}"));assert(context.failed);
    assert(S2_TEST_Start(&context,"test-rm",randomBytes));drain(false);
    assert(!input("{\"message_type\":\"Handshake\",\"message_id\":\"bad\\u0000id\",\"role\":\"CEM\"}"));
    assert(S2_TEST_Start(&context,"test-rm",randomBytes));drain(false);
    assert(input("{\"message_type\":\"HandshakeResponse\",\"message_id\":\"cem-05\",\"selected_protocol_version\":\"v1.0.0\"}"));assert(!context.negotiated);drain(false);
    for(unsigned i=0;i<S2_TEST_QUEUE_DEPTH;i++)assert(input("{\"message_type\":\"Unknown\",\"message_id\":\"cem-06\"}"));
    assert(!input("{\"message_type\":\"Unknown\",\"message_id\":\"cem-06\"}"));assert(context.failed);
    entropyFails=true;assert(!S2_TEST_Start(&context,"test-rm",randomBytes));entropyFails=false;
    S2_TEST_Reset(&context);for(size_t i=0;i<sizeof(context);i++)assert(((uint8_t*)&context)[i]==0);
    puts("Diagnostic S2 message validation, state order, no acknowledgement loop, control rejection, queue bounds and entropy failure passed.");return 0;
}
