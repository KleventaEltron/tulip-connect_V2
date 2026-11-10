#include "s2_transport.h"
#include "s2_sha1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static S2_TRANSPORT transport;
static bool trusted, blockWrite, badRead, badWrite, eof, rejectText;
static unsigned closes, writes, reads, messages;
static size_t readChunk, writeChunk, inputLength, inputOffset, outputLength;
static uint8_t input[8192], output[16384];
static const char response[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
    "Connection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
static bool verified(void *user) { (void)user; return trusted; }
static int readBytes(void *user, uint8_t *out, size_t capacity)
{
    size_t length = inputLength - inputOffset;
    (void)user; reads++;
    assert(capacity <= S2_TRANSPORT_IO_BUDGET);
    if (badRead) return (int)capacity + 1;
    if (eof) return -1;
    /* A real peer responds only after the complete upgrade request. */
    if (outputLength < 4 || strstr((const char *)output, "\r\n\r\n") == NULL) return 0;
    if (length > capacity) length = capacity;
    if (length > readChunk) length = readChunk;
    memcpy(out, input + inputOffset, length); inputOffset += length;
    return (int)length;
}
static int writeBytes(void *user, const uint8_t *data, size_t length)
{
    (void)user; writes++;
    assert(length <= S2_TRANSPORT_IO_BUDGET);
    if (badWrite) return (int)length + 1;
    if (blockWrite) return 0;
    if (length > writeChunk) length = writeChunk;
    assert(outputLength + length < sizeof(output));
    memcpy(output + outputLength, data, length); outputLength += length;
    output[outputLength] = 0;
    return (int)length;
}
static void closeBackend(void *user) { (void)user; closes++; }
static bool randomBytes(void *user, uint8_t *out, size_t length)
{
    (void)user;
    if (length == 16) memcpy(out, "the sample nonce", 16);
    else { assert(length == 4); memset(out, 0x55, 4); }
    return true;
}
static bool sha1(void *user, const uint8_t *data, size_t length, uint8_t out[20])
{
    static const uint8_t digest[] = {0xb3,0x7a,0x4f,0x2c,0xc0,0x62,0x4f,0x16,0x90,0xf6,
        0x46,0x06,0xcf,0x38,0x59,0x45,0xb2,0xbe,0xc4,0xea};
    (void)user; assert(length == 60);
    assert(S2_SHA1_WebSocketAccept(NULL, data, length, out));
    assert(memcmp(out, digest, 20) == 0); return true;
}
static bool text(void *user, const char *data, size_t length)
{
    (void)user; assert(length == 2 && memcmp(data,"{}",2)==0); messages++;
    return !rejectText;
}
static void initialize(void)
{
    const S2_TRANSPORT_PLATFORM platform = {NULL, verified, readBytes, writeBytes,
        closeBackend, randomBytes, sha1, text};
    trusted = true; blockWrite = badRead = badWrite = eof = rejectText = false;
    closes = writes = reads = messages = 0;
    inputOffset = outputLength = 0;
    memset(output, 0, sizeof(output));
    memcpy(input, response, sizeof(response) - 1); inputLength = sizeof(response) - 1;
    readChunk = writeChunk = S2_TRANSPORT_IO_BUDGET;
    assert(S2_TRANSPORT_Initialize(&transport, &platform));
}
static void start(uint32_t now)
{
    assert(S2_TRANSPORT_Start(&transport,"cem.local","/session","token",now));
    assert(S2_TRANSPORT_GetState(&transport) == S2_TRANSPORT_UPGRADING);
}
static void openConnection(void)
{
    initialize(); start(0); S2_TRANSPORT_Tasks(&transport,true,1);
    assert(S2_TRANSPORT_GetState(&transport) == S2_TRANSPORT_OPEN);
}
int main(void)
{
    /* No backend before Start, and no unauthenticated bytes sent on bad trust. */
    initialize(); S2_TRANSPORT_Tasks(&transport,true,100); assert(writes == 0 && reads == 0);
    trusted = false;
    assert(!S2_TRANSPORT_Start(&transport,"cem.local","/session","token",0));
    assert(closes == 1 && writes == 0 && reads == 0);
    S2_TRANSPORT_Stop(&transport); assert(closes == 1);
    /* Split upgrades and partial writes retain all bytes exactly once. */
    initialize(); readChunk = 1; writeChunk = 3; start(0);
    for (uint32_t now = 1; now < 1000 && transport.state != S2_TRANSPORT_OPEN; now++)
        S2_TRANSPORT_Tasks(&transport,true,now);
    assert(transport.state == S2_TRANSPORT_OPEN && closes == 0);
    assert(strstr((const char *)output,"Authorization: Bearer token\r\n\r\n") != NULL);
    size_t requestLength = outputLength;
    assert(!S2_TRANSPORT_Start(&transport,"other","/","token",1000) && closes == 0);
    assert(S2_TRANSPORT_SendText(&transport,"{}",2) == S2_WS_OK);
    for (unsigned i = 0; i < 10; i++) S2_TRANSPORT_Tasks(&transport,true,1000+i);
    assert(outputLength == requestLength + 8);
    assert(output[requestLength] == 0x81 && output[requestLength+1] == 0x82);
    assert((output[requestLength+6] ^ 0x55) == '{' && (output[requestLength+7] ^ 0x55) == '}');
    /* Upgrade and first frame may share one read. */
    initialize(); input[inputLength++] = 0x81; input[inputLength++] = 2;
    input[inputLength++] = '{'; input[inputLength++] = '}'; start(0);
    S2_TRANSPORT_Tasks(&transport,true,1); assert(messages == 1 && transport.state == S2_TRANSPORT_OPEN);
    /* Network loss discards pending work and closes only once. */
    assert(S2_TRANSPORT_SendText(&transport,"{}",2) == S2_WS_OK);
    unsigned before = writes;
    S2_TRANSPORT_Tasks(&transport,false,2);
    assert(closes == 1 && writes == before && transport.queueLength == 0);
    S2_TRANSPORT_Stop(&transport); assert(closes == 1);
    for (size_t i = 0; i < sizeof(transport.queue); i++) assert(transport.queue[i] == 0);
    /* A close reply drains before the backend is closed. */
    openConnection(); inputLength = inputOffset = 0;
    input[inputLength++] = 0x88; input[inputLength++] = 2;
    input[inputLength++] = 3; input[inputLength++] = 0xe8;
    S2_TRANSPORT_Tasks(&transport,true,2); assert(transport.state == S2_TRANSPORT_DRAINING && closes == 0);
    size_t closeOffset = outputLength;
    S2_TRANSPORT_Tasks(&transport,true,3); assert(closes == 1 && outputLength == closeOffset + 8);
    assert(output[closeOffset] == 0x88);
    /* A peer close with pending application output aborts without interleaving. */
    openConnection(); blockWrite = true;
    assert(S2_TRANSPORT_SendText(&transport,"{}",2) == S2_WS_OK);
    inputOffset = inputLength = 0; input[inputLength++] = 0x88; input[inputLength++] = 0;
    S2_TRANSPORT_Tasks(&transport,true,2);
    assert(closes == 1 && transport.state == S2_TRANSPORT_FAILED && transport.queueLength == 0);
    /* Blocked close drains have a bounded deadline. */
    openConnection(); inputOffset = inputLength = 0;
    input[inputLength++] = 0x88; input[inputLength++] = 0;
    S2_TRANSPORT_Tasks(&transport,true,2); blockWrite = true;
    S2_TRANSPORT_Tasks(&transport,true,2001); assert(closes == 0);
    S2_TRANSPORT_Tasks(&transport,true,2002); assert(closes == 1);
    /* Upgrade and no-progress write timeouts, including unsigned wrap. */
    initialize(); inputLength = 0; start(UINT32_MAX - 5u);
    S2_TRANSPORT_Tasks(&transport,true,9993); assert(closes == 0);
    S2_TRANSPORT_Tasks(&transport,true,9994); assert(closes == 1 && transport.state == S2_TRANSPORT_FAILED);
    openConnection(); blockWrite = true;
    assert(S2_TRANSPORT_SendText(&transport,"{}",2) == S2_WS_OK);
    S2_TRANSPORT_Tasks(&transport,true,10);
    S2_TRANSPORT_Tasks(&transport,true,10010); assert(closes == 1 && transport.state == S2_TRANSPORT_FAILED);
    /* Lost trust, EOF, malformed backend counts, malformed HTTP. */
    openConnection(); trusted = false; S2_TRANSPORT_Tasks(&transport,true,2); assert(closes == 1);
    openConnection(); eof = true; S2_TRANSPORT_Tasks(&transport,true,2); assert(closes == 1);
    openConnection(); badRead = true; S2_TRANSPORT_Tasks(&transport,true,2); assert(closes == 1);
    openConnection(); badWrite = true;
    assert(S2_TRANSPORT_SendText(&transport,"{}",2) == S2_WS_OK);
    S2_TRANSPORT_Tasks(&transport,true,2); assert(closes == 1);
    initialize(); input[9] = '4'; start(0); S2_TRANSPORT_Tasks(&transport,true,1);
    assert(closes == 1 && transport.state == S2_TRANSPORT_FAILED);
    /* Oversized unterminated HTTP headers cannot overrun the fixed buffer. */
    initialize(); memset(input,'x',sizeof(input)); inputLength = sizeof(input); start(0);
    for (unsigned now = 1; now < 20 && closes == 0; now++)
        S2_TRANSPORT_Tasks(&transport,true,now);
    assert(closes == 1 && transport.state == S2_TRANSPORT_FAILED);
    /* A fresh verified backend can be attached after failure. */
    inputOffset = 0; memcpy(input,response,sizeof(response)-1); inputLength = sizeof(response)-1;
    start(100); S2_TRANSPORT_Tasks(&transport,true,101);
    assert(transport.state == S2_TRANSPORT_OPEN);
    /* Queue exhaustion closes instead of accepting a partial frame. */
    openConnection();
    static char maximum[S2_WS_MESSAGE_MAX]; memset(maximum,'x',sizeof(maximum));
    assert(S2_TRANSPORT_SendText(&transport,maximum,sizeof(maximum)) == S2_WS_OK);
    assert(S2_TRANSPORT_SendText(&transport,maximum,sizeof(maximum)) == S2_WS_OK);
    assert(S2_TRANSPORT_SendText(&transport,"x",1) == S2_WS_IO_ERROR);
    assert(transport.state == S2_TRANSPORT_FAILED && closes == 1 && transport.queueLength == 0);
    S2_TRANSPORT_Stop(&transport); assert(closes == 1 && transport.queueLength == 0);
    printf("S2 transport partial I/O, TLS gate, upgrade boundaries, deadlines and cleanup tests passed. Context: %zu bytes.\n",sizeof(transport));
    return 0;
}
