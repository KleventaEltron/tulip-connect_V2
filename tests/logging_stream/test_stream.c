static void drain(void) {
    unsigned i;for(i=0;loggingTransmitBusy();i++) {
        now++;loggingNetworkTasks();if(pending>19)pending-=19;else pending=0;
        assert(!loggingIoFailed());assert(i<100000);
    }
    wire[wireLength]=0;assert(loggingTx.state==LOG_TX_DONE);
}
static void emit(const char *name) {
    printf("%s ",name);for(size_t i=0;i<wireLength;i++)printf("%02x",(unsigned char)wire[i]);putchar('\n');wireLength=0;pending=0;
}
static void response(const char *s) {
    strcpy(input,s);inputLength=strlen(input);inputOffset=0;resetRxBuffer();loggingFault=false;connected=true;resetPeer=false;
    guardedRx.guard=0x12345678;
    for(unsigned i=0;i<5000;i++) {
        now++;
        if(readServerResponseDone()||loggingIoFailed())break;
        if(inputOffset==inputLength)connected=false;
    }
    assert(guardedRx.guard==0x12345678);
}
int main(void) {
    char method[]="POST",path[]="api/v1/log";
    mask=0xffff;memset(builds,0,sizeof(builds));
    assert(TULIP_REQUEST_TESTER(method,path));drain();
    for(unsigned i=0;i<17;i++)assert(builds[i]==1);
    emit("REALTIME_HEX");
    mask=0;memset(builds,0,sizeof(builds));assert(TULIP_REQUEST_TESTER(method,path));drain();emit("ALONE_HEX");
    mask=0x8008;memset(builds,0,sizeof(builds));assert(TULIP_REQUEST_TESTER(method,path));drain();emit("SPARSE_HEX");
    assert(getUpdateSettingsFromServer());drain();emit("GET_HEX");
    for(unsigned i=0;i<4096;i++)ee[i]=(uint16_t)(i+1000);
    assert(sendUpdatedSettingsList());drain();emit("SETTINGS_HEX");
    response("HTTP/1.1 200 OK\r\ncontent-length: 25\r\n\r\n{\"update_settings\":false}");
    assert(!loggingIoFailed()&&loggingRxComplete&&!areUpdateSettingsAvailable());
    response("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n");assert(!loggingIoFailed()&&buffer_len==2);
    response("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n{}");assert(loggingIoFailed());
    response("HTTP/1.1 503 Error\r\nContent-Length: 2\r\n\r\n{}");assert(loggingIoFailed());
    response("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");assert(!loggingIoFailed());assert(!parse_modbus_settings()&&loggingIoFailed()&&applied==0);
    strcpy(readBuffer,"{\"settings_updates\":[{\"modbus_address\":\"0x0305\",\"value\":\"65535\"},{\"modbus_address\":\"0x0306\",\"value\":\"nonsense\"}]}");buffer_len=strlen(readBuffer);loggingFault=false;
    assert(!parse_modbus_settings()&&loggingIoFailed()&&applied==0);
    strcpy(readBuffer,"{\"settings_updates\":[{\"modbus_address\":\"0x0305\",\"value\":\"-5\"}]}");buffer_len=strlen(readBuffer);loggingFault=false;
    assert(parse_modbus_settings()&&applied==1&&lastAddress==0x0305&&lastValue==65531);
    strcpy(readBuffer,"{\"settings_updates\":[]}trailing");buffer_len=strlen(readBuffer);loggingFault=false;
    assert(!parse_modbus_settings()&&loggingIoFailed()&&applied==1);
    memset(readBuffer,'[',32);memset(readBuffer+32,']',32);readBuffer[64]=0;buffer_len=64;loggingFault=false;
    assert(!parse_modbus_settings()&&loggingIoFailed()&&applied==1);
    puts("Actual logging producers/receiver: 17-device paced upload, settings order, short writes/backpressure, framed replies and atomic batch validation passed.");return 0;
}
