#include "s2_ws.h"
#include <stdio.h>
#include <string.h>

static void base64(const uint8_t *data, size_t length, char *out)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, n = 0;
    for (i = 0; i < length; i += 3) {
        unsigned value = (unsigned)data[i] << 16;
        if (i + 1 < length) value |= (unsigned)data[i + 1] << 8;
        if (i + 2 < length) value |= data[i + 2];
        out[n++] = alphabet[value >> 18];
        out[n++] = alphabet[(value >> 12) & 63];
        out[n++] = i + 1 < length ? alphabet[(value >> 6) & 63] : '=';
        out[n++] = i + 2 < length ? alphabet[value & 63] : '=';
    }
    out[n] = '\0';
}

static unsigned lower(unsigned c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static bool equal(const char *value, size_t length, const char *expected)
{
    size_t i;
    if (length != strlen(expected)) return false;
    for (i = 0; i < length; i++)
        if (lower((unsigned char)value[i]) != lower((unsigned char)expected[i])) return false;
    return true;
}

static bool hasToken(const char *value, size_t length, const char *token)
{
    size_t i = 0;
    while (i < length) {
        size_t start, end;
        while (i < length && (value[i] == ' ' || value[i] == '\t' || value[i] == ',')) i++;
        start = i;
        while (i < length && value[i] != ',') i++;
        end = i;
        while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t')) end--;
        if (equal(value + start, end - start, token)) return true;
    }
    return false;
}

static bool visible(const char *value)
{
    if (value == NULL || *value == '\0') return false;
    while (*value) {
        unsigned c = (unsigned char)*value++;
        if (c < 33 || c > 126) return false;
    }
    return true;
}

static bool utf8(const uint8_t *data, size_t length)
{
    size_t i = 0;
    while (i < length) {
        unsigned c = data[i++], count, minimum, code, k;
        if (c < 0x80) continue;
        if (c >= 0xc2 && c <= 0xdf) { count = 1; minimum = 0x80; code = c & 31; }
        else if (c >= 0xe0 && c <= 0xef) { count = 2; minimum = 0x800; code = c & 15; }
        else if (c >= 0xf0 && c <= 0xf4) { count = 3; minimum = 0x10000; code = c & 7; }
        else return false;
        if (length - i < count) return false;
        for (k = 0; k < count; k++) {
            c = data[i++];
            if ((c & 0xc0) != 0x80) return false;
            code = (code << 6) | (c & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}

static void endConnection(S2_WS *ws)
{
    if (ws->ended) return;
    ws->open = false;
    ws->upgradePending = false;
    ws->ended = true;
    ws->rxLength = ws->messageLength = 0;
    ws->fragmented = false;
    memset(ws->expectedAccept, 0, sizeof(ws->expectedAccept));
    ws->platform.closed(ws->platform.user);
}

static S2_WS_RESULT sendFrame(S2_WS *ws, unsigned opcode, const uint8_t *data, size_t length)
{
    uint8_t mask[4];
    size_t header = 2, i;
    if (!ws->platform.random(ws->platform.user, mask, sizeof(mask))) {
        endConnection(ws);
        return S2_WS_IO_ERROR;
    }
    ws->tx[0] = (uint8_t)(0x80u | opcode);
    if (length < 126) ws->tx[1] = (uint8_t)(0x80u | length);
    else {
        ws->tx[1] = 0xfe;
        ws->tx[2] = (uint8_t)(length >> 8);
        ws->tx[3] = (uint8_t)length;
        header = 4;
    }
    memcpy(ws->tx + header, mask, sizeof(mask));
    header += sizeof(mask);
    for (i = 0; i < length; i++) ws->tx[header + i] = data[i] ^ mask[i % 4];
    if (!ws->platform.send(ws->platform.user, ws->tx, header + length)) {
        endConnection(ws);
        return S2_WS_IO_ERROR;
    }
    return S2_WS_OK;
}

static S2_WS_RESULT protocolError(S2_WS *ws, unsigned closeCode)
{
    uint8_t reason[2] = {(uint8_t)(closeCode >> 8), (uint8_t)closeCode};
    if (ws->open) (void)sendFrame(ws, 8, reason, sizeof(reason));
    endConnection(ws);
    return S2_WS_PROTOCOL_ERROR;
}

S2_WS_RESULT S2_WS_Initialize(S2_WS *ws, const S2_WS_PLATFORM *platform)
{
    if (ws == NULL || platform == NULL || platform->random == NULL || platform->sha1 == NULL ||
        platform->send == NULL || platform->text == NULL || platform->closed == NULL)
        return S2_WS_INVALID_ARGUMENT;
    S2_WS_PLATFORM copy = *platform;
    memset(ws, 0, sizeof(*ws));
    ws->platform = copy;
    ws->initialized = true;
    return S2_WS_OK;
}

S2_WS_RESULT S2_WS_BuildUpgrade(S2_WS *ws, const char *authority, const char *path,
    const char *bearer, char *out, size_t capacity)
{
    uint8_t nonce[16], hash[20];
    char key[25], hashInput[61];
    int n;
    if (ws == NULL || !ws->initialized || ws->open || ws->ended || ws->upgradePending ||
        !visible(authority) || !visible(path) || path[0] != '/' || strchr(path, '#') != NULL ||
        !visible(bearer) || out == NULL || capacity == 0) return S2_WS_INVALID_ARGUMENT;
    out[0] = '\0';
    if (!ws->platform.random(ws->platform.user, nonce, sizeof(nonce))) {
        endConnection(ws);
        return S2_WS_IO_ERROR;
    }
    base64(nonce, sizeof(nonce), key);
    (void)snprintf(hashInput, sizeof(hashInput), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    if (!ws->platform.sha1(ws->platform.user, (const uint8_t *)hashInput, strlen(hashInput), hash)) {
        endConnection(ws);
        return S2_WS_IO_ERROR;
    }
    n = snprintf(out, capacity,
        "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\nAuthorization: Bearer %s\r\n\r\n",
        path, authority, key, bearer);
    if (n < 0 || (size_t)n >= capacity) {
        memset(out, 0, capacity); /* Do not leave a truncated token-bearing request. */
        return S2_WS_INVALID_ARGUMENT;
    }
    base64(hash, sizeof(hash), ws->expectedAccept);
    ws->upgradePending = true;
    return S2_WS_OK;
}

S2_WS_RESULT S2_WS_AcceptUpgrade(S2_WS *ws, const char *headers, size_t length)
{
    size_t i, start;
    bool upgrade = false, connection = false, accept = false;
    if (ws == NULL || !ws->initialized || ws->ended || ws->open || !ws->upgradePending ||
        headers == NULL) return S2_WS_INVALID_ARGUMENT;
    if (length < 16 || length > S2_WS_HEADERS_MAX || memcmp(headers + length - 4, "\r\n\r\n", 4))
        return protocolError(ws, 1002);
    /* Counted input, never depend on caller-supplied NUL termination. */
    for (i = 0; i < length; i++) {
        unsigned c = (unsigned char)headers[i];
        if (c == '\r') {
            if (++i >= length || headers[i] != '\n') return protocolError(ws, 1002);
        } else if (c == '\n' || c == 127 || (c < 32 && c != '\t')) return protocolError(ws, 1002);
    }
    i = 0;
    while (i + 1 < length && headers[i] != '\r') i++;
    if (i < 12 || memcmp(headers, "HTTP/1.1 101", 12) || headers[12] != ' ')
        return protocolError(ws, 1002);
    start = i + 2;
    while (start < length - 2) {
        size_t colon = start, end = start, value, valueEnd;
        while (end < length && headers[end] != '\r') end++;
        while (colon < end && headers[colon] != ':') colon++;
        if (colon == start || colon == end) return protocolError(ws, 1002);
        for (i = start; i < colon; i++) {
            unsigned c = (unsigned char)headers[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || strchr("!#$%&'*+-.^_`|~", (int)c)))
                return protocolError(ws, 1002);
        }
        value = colon + 1;
        while (value < end && (headers[value] == ' ' || headers[value] == '\t')) value++;
        valueEnd = end;
        while (valueEnd > value && (headers[valueEnd - 1] == ' ' || headers[valueEnd - 1] == '\t')) valueEnd--;
        if (equal(headers + start, colon - start, "Upgrade")) {
            upgrade = upgrade || hasToken(headers + value, valueEnd - value, "websocket");
        } else if (equal(headers + start, colon - start, "Connection")) {
            connection = connection || hasToken(headers + value, valueEnd - value, "Upgrade");
        } else if (equal(headers + start, colon - start, "Sec-WebSocket-Accept")) {
            if (accept || valueEnd - value != 28 || memcmp(headers + value, ws->expectedAccept, 28))
                return protocolError(ws, 1002);
            accept = true;
        } else if (equal(headers + start, colon - start, "Sec-WebSocket-Extensions") ||
                   equal(headers + start, colon - start, "Sec-WebSocket-Protocol")) return protocolError(ws, 1002);
        start = end + 2;
    }
    if (start != length - 2 || !upgrade || !connection || !accept) return protocolError(ws, 1002);
    ws->open = true;
    ws->upgradePending = false;
    return S2_WS_OK;
}

bool S2_WS_IsOpen(const S2_WS *ws)
{
    return ws != NULL && ws->initialized && ws->open && !ws->ended;
}

void S2_WS_Reset(S2_WS *ws)
{
    if (ws != NULL && ws->initialized) endConnection(ws);
}

S2_WS_RESULT S2_WS_SendText(S2_WS *ws, const char *text, size_t length)
{
    if (!S2_WS_IsOpen(ws)) return S2_WS_CLOSED;
    if (text == NULL || length > S2_WS_MESSAGE_MAX || !utf8((const uint8_t *)text, length))
        return S2_WS_INVALID_ARGUMENT;
    return sendFrame(ws, 1, (const uint8_t *)text, length);
}

S2_WS_RESULT S2_WS_Ping(S2_WS *ws)
{
    if (!S2_WS_IsOpen(ws)) return S2_WS_CLOSED;
    return sendFrame(ws, 9, NULL, 0);
}

static bool validClose(unsigned code)
{
    return code >= 1000 && code < 5000 && code != 1004 && code != 1005 && code != 1006 &&
           code != 1015 && !(code >= 1016 && code < 3000);
}

S2_WS_RESULT S2_WS_Feed(S2_WS *ws, const uint8_t *bytes, size_t length)
{
    size_t input;
    if (!S2_WS_IsOpen(ws)) return S2_WS_CLOSED;
    if (bytes == NULL && length != 0) return S2_WS_INVALID_ARGUMENT;
    for (input = 0; input < length; input++) {
        uint8_t first, second, *payload;
        unsigned opcode;
        bool fin;
        size_t header = 2, size;
        if (ws->rxLength == sizeof(ws->rx)) return protocolError(ws, 1009);
        ws->rx[ws->rxLength++] = bytes[input];
        if (ws->rxLength < 2) continue;
        first = ws->rx[0]; second = ws->rx[1];
        opcode = first & 15; fin = (first & 0x80) != 0; size = second & 127;
        if ((first & 0x70) || (second & 0x80)) return protocolError(ws, 1002);
        if (opcode != 0 && opcode != 1 && opcode != 8 && opcode != 9 && opcode != 10)
            return protocolError(ws, opcode == 2 ? 1003 : 1002);
        if (opcode >= 8 && (!fin || size > 125)) return protocolError(ws, 1002);
        if (size == 127) {
            /* With a 4096-byte bound no legal minimally encoded 64-bit frame fits. */
            if (ws->rxLength < 10) continue;
            if ((ws->rx[2] & 0x80) != 0) return protocolError(ws, 1002);
            if (ws->rx[2] == 0 && ws->rx[3] == 0 && ws->rx[4] == 0 && ws->rx[5] == 0 &&
                ws->rx[6] == 0 && ws->rx[7] == 0) return protocolError(ws, 1002);
            return protocolError(ws, 1009);
        }
        if (size == 126) {
            if (ws->rxLength < 4) continue;
            size = ((size_t)ws->rx[2] << 8) | ws->rx[3]; header = 4;
            if (size < 126) return protocolError(ws, 1002);
        }
        if (size > S2_WS_MESSAGE_MAX) return protocolError(ws, 1009);
        if (ws->rxLength < header + size) continue;
        payload = ws->rx + header;
        if (opcode == 9) {
            if (sendFrame(ws, 10, payload, size) != S2_WS_OK) return S2_WS_IO_ERROR;
        } else if (opcode == 10) {
            /* Heartbeat timeout tracking belongs to the transport task. */
        } else if (opcode == 8) {
            if (size == 1) return protocolError(ws, 1002);
            if (size >= 2 && !validClose((unsigned)payload[0] * 256 + payload[1])) return protocolError(ws, 1002);
            if (size >= 2 && !utf8(payload + 2, size - 2)) return protocolError(ws, 1007);
            if (sendFrame(ws, 8, payload, size) != S2_WS_OK) return S2_WS_IO_ERROR;
            endConnection(ws);
            return S2_WS_CLOSED;
        } else {
            if ((opcode == 1 && ws->fragmented) || (opcode == 0 && !ws->fragmented))
                return protocolError(ws, 1002);
            if (size > S2_WS_MESSAGE_MAX - ws->messageLength) return protocolError(ws, 1009);
            memcpy(ws->message + ws->messageLength, payload, size);
            ws->messageLength += size;
            ws->fragmented = !fin;
            if (fin) {
                if (!utf8(ws->message, ws->messageLength)) return protocolError(ws, 1007);
                if (!ws->platform.text(ws->platform.user, (const char *)ws->message, ws->messageLength))
                    return protocolError(ws, 1011);
                ws->messageLength = 0;
            }
        }
        ws->rxLength = 0;
    }
    return S2_WS_OK;
}
