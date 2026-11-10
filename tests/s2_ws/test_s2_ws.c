#include "s2_ws.h"
#include "s2_sha1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static S2_WS ws;
static unsigned sent, texts, closes;
static bool randomFails, shaFails, sendFails, textFails;
static uint8_t lastFrame[S2_WS_MESSAGE_MAX + 8], lastText[S2_WS_MESSAGE_MAX];
static size_t frameLength, textLength;
static const char response[] = "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\nConnection: keep-alive, Upgrade\r\n"
    "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
static bool randomBytes(void *user, uint8_t *out, size_t length)
{
    static const uint8_t mask[] = {0x37,0xfa,0x21,0x3d};
    (void)user;
    if (randomFails) return false;
    if (length == 16) memcpy(out, "the sample nonce", 16);
    else { assert(length == 4); memcpy(out, mask, 4); }
    return true;
}
static bool sha1(void *user, const uint8_t *data, size_t length, uint8_t out[20])
{
    static const uint8_t digest[] = {0xb3,0x7a,0x4f,0x2c,0xc0,0x62,0x4f,0x16,0x90,0xf6,
        0x46,0x06,0xcf,0x38,0x59,0x45,0xb2,0xbe,0xc4,0xea};
    const char expected[] = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    (void)user;
    assert(length == strlen(expected) && memcmp(data, expected, length) == 0);
    if (shaFails) return false;
    assert(S2_SHA1_WebSocketAccept(NULL, data, length, out));
    assert(memcmp(out, digest, sizeof(digest)) == 0);
    return true;
}
static bool sendFrame(void *user, const uint8_t *frame, size_t length)
{
    (void)user;
    sent++;
    assert(length <= sizeof(lastFrame));
    memcpy(lastFrame, frame, length); frameLength = length;
    return !sendFails;
}
static bool text(void *user, const char *message, size_t length)
{
    (void)user;
    texts++;
    assert(length <= sizeof(lastText));
    memcpy(lastText, message, length); textLength = length;
    return !textFails;
}
static void closed(void *user) { (void)user; closes++; }
static void initialize(void)
{
    S2_WS_PLATFORM platform = {NULL, randomBytes, sha1, sendFrame, text, closed};
    randomFails = shaFails = sendFails = textFails = false;
    sent = texts = closes = 0;
    assert(S2_WS_Initialize(&ws, &platform) == S2_WS_OK);
}
static void request(void)
{
    char request[512];
    assert(S2_WS_BuildUpgrade(&ws, "cem.local:443", "/session?x=1", "token", request, sizeof(request)) == S2_WS_OK);
    assert(strstr(request, "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n") != NULL);
    assert(strstr(request, "Authorization: Bearer token\r\n\r\n") != NULL);
    assert(!S2_WS_IsOpen(&ws));
}
static void openConnection(void)
{
    initialize(); request();
    assert(S2_WS_AcceptUpgrade(&ws, response, sizeof(response) - 1) == S2_WS_OK);
    assert(S2_WS_IsOpen(&ws));
}
static void checkClose(unsigned expected)
{
    assert(sent == 1 && lastFrame[0] == 0x88 && frameLength == 8);
    assert((((unsigned)(lastFrame[6] ^ lastFrame[2]) << 8) |
        (lastFrame[7] ^ lastFrame[3])) == expected);
    assert(!S2_WS_IsOpen(&ws) && closes == 1);
    S2_WS_Reset(&ws); assert(closes == 1);
}
static void badFrame(const uint8_t *frame, size_t length, unsigned closeCode)
{
    openConnection();
    assert(S2_WS_Feed(&ws, frame, length) == S2_WS_PROTOCOL_ERROR);
    checkClose(closeCode);
    assert(S2_WS_Ping(&ws) == S2_WS_CLOSED);
    assert(S2_WS_Feed(&ws, frame, length) == S2_WS_CLOSED);
}
static void badResponse(const char *headers, size_t length)
{
    initialize(); request();
    assert(S2_WS_AcceptUpgrade(&ws, headers, length) == S2_WS_PROTOCOL_ERROR);
    assert(!S2_WS_IsOpen(&ws) && closes == 1 && sent == 0);
}
int main(void)
{
    static uint8_t frame[S2_WS_MESSAGE_MAX + 12];
    char output[512], modified[512];
    initialize();
    assert(S2_WS_Initialize(NULL, &ws.platform) == S2_WS_INVALID_ARGUMENT);
    assert(S2_WS_AcceptUpgrade(&ws, response, sizeof(response) - 1) == S2_WS_INVALID_ARGUMENT);
    assert(S2_WS_BuildUpgrade(&ws, "cem\r\nInjected: x", "/", "token", output, sizeof(output)) == S2_WS_INVALID_ARGUMENT);
    assert(S2_WS_BuildUpgrade(&ws, "cem", "/#fragment", "token", output, sizeof(output)) == S2_WS_INVALID_ARGUMENT);
    assert(S2_WS_BuildUpgrade(&ws, "cem", "/", "token\n", output, sizeof(output)) == S2_WS_INVALID_ARGUMENT);
    assert(S2_WS_BuildUpgrade(&ws, "cem", "/", "token", output, 8) == S2_WS_INVALID_ARGUMENT);
    assert(output[0] == 0 && !ws.upgradePending);
    request();
    assert(S2_WS_BuildUpgrade(&ws, "cem", "/", "token", output, sizeof(output)) == S2_WS_INVALID_ARGUMENT);
    assert(S2_WS_AcceptUpgrade(&ws, response, sizeof(response) - 1) == S2_WS_OK);
    /* RFC6455 masking example. */
    assert(S2_WS_SendText(&ws, "Hello", 5) == S2_WS_OK);
    assert(frameLength == 11 && memcmp(lastFrame, "\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11) == 0);
    /* Every possible read boundary for a frame, including byte-at-a-time input. */
    for (size_t split = 0; split <= 7; split++) {
        const uint8_t hello[] = {0x81,5,'H','e','l','l','o'};
        openConnection();
        assert(S2_WS_Feed(&ws, hello, split) == S2_WS_OK);
        assert(S2_WS_Feed(&ws, hello + split, sizeof(hello) - split) == S2_WS_OK);
        assert(texts == 1 && textLength == 5 && memcmp(lastText, "Hello", 5) == 0);
    }
    openConnection();
    const uint8_t fragment[] = {0x01,1,0xe2, 0x89,1,'P', 0x80,2,0x82,0xac};
    for (size_t i = 0; i < sizeof(fragment); i++) assert(S2_WS_Feed(&ws, fragment + i, 1) == S2_WS_OK);
    assert(texts == 1 && textLength == 3 && memcmp(lastText, "\xe2\x82\xac", 3) == 0);
    assert(sent == 1 && lastFrame[0] == 0x8a && (lastFrame[6] ^ lastFrame[2]) == 'P');
    const uint8_t multiple[] = {0x81,1,'a',0x81,1,'b',0x8a,0};
    assert(S2_WS_Feed(&ws, multiple, sizeof(multiple)) == S2_WS_OK && texts == 3);
    assert(S2_WS_Ping(&ws) == S2_WS_OK && lastFrame[0] == 0x89);
    /* Exact bound accepted, 16-bit length, outbound masking and no heap. */
    openConnection(); memset(frame, 'x', sizeof(frame));
    frame[0] = 0x81; frame[1] = 126; frame[2] = 16; frame[3] = 0;
    assert(S2_WS_Feed(&ws, frame, S2_WS_MESSAGE_MAX + 4) == S2_WS_OK);
    assert(texts == 1 && textLength == S2_WS_MESSAGE_MAX);
    assert(S2_WS_SendText(&ws, (const char *)frame + 4, S2_WS_MESSAGE_MAX) == S2_WS_OK);
    assert(frameLength == S2_WS_MESSAGE_MAX + 8 && lastFrame[1] == 0xfe && lastFrame[2] == 16 && lastFrame[3] == 0);
    assert(S2_WS_SendText(&ws, (const char *)frame, S2_WS_MESSAGE_MAX + 1) == S2_WS_INVALID_ARGUMENT);
    /* Extended payload also survives one byte per read. */
    openConnection();
    for (size_t i = 0; i < S2_WS_MESSAGE_MAX + 4; i++) assert(S2_WS_Feed(&ws, frame + i, 1) == S2_WS_OK);
    assert(textLength == S2_WS_MESSAGE_MAX);
    const uint8_t masked[] = {0x81,0x80}; badFrame(masked,sizeof(masked),1002);
    const uint8_t rsv[] = {0xc1,0}; badFrame(rsv,sizeof(rsv),1002);
    const uint8_t orphan[] = {0x80,0}; badFrame(orphan,sizeof(orphan),1002);
    const uint8_t nested[] = {0x01,0,0x81,0}; badFrame(nested,sizeof(nested),1002);
    const uint8_t binary[] = {0x82,0}; badFrame(binary,sizeof(binary),1003);
    const uint8_t reserved[] = {0x83,0}; badFrame(reserved,sizeof(reserved),1002);
    const uint8_t longPing[] = {0x89,126}; badFrame(longPing,sizeof(longPing),1002);
    const uint8_t splitPing[] = {0x09,0}; badFrame(splitPing,sizeof(splitPing),1002);
    const uint8_t nonminimal[] = {0x81,126,0,1}; badFrame(nonminimal,sizeof(nonminimal),1002);
    const uint8_t tooLarge[] = {0x81,126,16,1}; badFrame(tooLarge,sizeof(tooLarge),1009);
    const uint8_t size64[] = {0x81,127,0,0,0,0,0,1,0,0}; badFrame(size64,sizeof(size64),1009);
    const uint8_t short64[] = {0x81,127,0,0,0,0,0,0,0,1}; badFrame(short64,sizeof(short64),1002);
    const uint8_t invalidUtf8[] = {0x81,2,0xc0,0xaf}; badFrame(invalidUtf8,sizeof(invalidUtf8),1007);
    const uint8_t surrogate[] = {0x81,3,0xed,0xa0,0x80}; badFrame(surrogate,sizeof(surrogate),1007);
    const uint8_t largeUnicode[] = {0x81,4,0xf4,0x90,0x80,0x80}; badFrame(largeUnicode,sizeof(largeUnicode),1007);
    const uint8_t closeOne[] = {0x88,1,0}; badFrame(closeOne,sizeof(closeOne),1002);
    const uint8_t closeReserved[] = {0x88,2,3,0xed}; badFrame(closeReserved,sizeof(closeReserved),1002);
    const uint8_t closeBadUtf8[] = {0x88,3,3,0xe8,0xff}; badFrame(closeBadUtf8,sizeof(closeBadUtf8),1007);
    openConnection();
    const uint8_t goodClose[] = {0x88,2,3,0xe8,0x81,1,'x'};
    assert(S2_WS_Feed(&ws, goodClose, sizeof(goodClose)) == S2_WS_CLOSED && texts == 0);
    checkClose(1000);
    /* Aggregate fragmented length is bounded even if each frame fits. */
    openConnection(); frame[0] = 0x01;
    assert(S2_WS_Feed(&ws, frame, S2_WS_MESSAGE_MAX + 4) == S2_WS_OK);
    const uint8_t excess[] = {0x80,1,'x'};
    assert(S2_WS_Feed(&ws, excess, sizeof(excess)) == S2_WS_PROTOCOL_ERROR); checkClose(1009);
    /* Failure injection ends once and prevents subsequent sends/delivery. */
    openConnection(); randomFails = true;
    assert(S2_WS_Ping(&ws) == S2_WS_IO_ERROR && closes == 1 && sent == 0);
    openConnection(); sendFails = true;
    assert(S2_WS_Ping(&ws) == S2_WS_IO_ERROR && closes == 1);
    openConnection(); textFails = true;
    const uint8_t empty[] = {0x81,0};
    assert(S2_WS_Feed(&ws, empty, sizeof(empty)) == S2_WS_PROTOCOL_ERROR); checkClose(1011);
    initialize(); randomFails = true;
    assert(S2_WS_BuildUpgrade(&ws, "cem", "/", "token", output, sizeof(output)) == S2_WS_IO_ERROR && closes == 1);
    initialize(); shaFails = true;
    assert(S2_WS_BuildUpgrade(&ws, "cem", "/", "token", output, sizeof(output)) == S2_WS_IO_ERROR && closes == 1);
    openConnection(); S2_WS_Reset(&ws); S2_WS_Reset(&ws); assert(closes == 1 && sent == 0);
    /* Upgrade response validation. */
    strcpy(modified, response); modified[9] = '2'; badResponse(modified, strlen(modified));
    strcpy(modified, response); *strstr(modified, "s3pPL") = 'X'; badResponse(modified, strlen(modified));
    strcpy(modified, response); modified[30] = '\0'; badResponse(modified, sizeof(response) - 1);
    badResponse(response, sizeof(response) - 2);
    snprintf(modified, sizeof(modified), "%.*sSec-WebSocket-Extensions: permessage-deflate\r\n\r\n", (int)sizeof(response) - 3, response);
    badResponse(modified, strlen(modified));
    snprintf(modified, sizeof(modified), "%.*sSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", (int)sizeof(response) - 3, response);
    badResponse(modified, strlen(modified));
    const char incomplete[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n";
    badResponse(incomplete, sizeof(incomplete) - 1);
    strcpy(modified, response);
    memcpy(strstr(modified, "Upgrade\r\n"), "UpgradX", 7);
    badResponse(modified, strlen(modified));
    initialize(); request();
    const char repeated[] = "HTTP/1.1 101 Switching Protocols\r\n"
        "upgrade: WebSocket\r\nConnection: keep-alive\r\nConnection: uPgRaDe\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
    assert(S2_WS_AcceptUpgrade(&ws, repeated, sizeof(repeated) - 1) == S2_WS_OK);
    assert(S2_WS_Initialize(&ws, &ws.platform) == S2_WS_OK); /* Reuse callbacks safely. */
    request();
    assert(S2_WS_AcceptUpgrade(&ws, response, sizeof(response) - 1) == S2_WS_OK);
    printf("WebSocket upgrade, framing, boundaries, UTF-8 and failure tests passed. Context: %zu bytes.\n", sizeof(ws));
    return 0;
}
