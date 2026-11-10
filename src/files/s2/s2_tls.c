#include "s2_tls.h"
#include "definitions.h"
#include "files/network_manager.h"
#include "files/crypto_random.h"
#include "s2_sha1.h"
#include "config.h"
#include "wolfssl/ssl.h"
#include "wolfssl/wolfio.h"
#include <limits.h>
#include <string.h>

#if defined(NO_ASN_TIME) || defined(NO_CERTS)
#error S2 TLS requires certificate verification and certificate time validation
#endif

bool S2_TLS_ClockReady(void)
{
    /* Zero means SNTP has not provided UTC. Certificate validity is checked by
     * wolfSSL against its existing pic32_time/SNTP hook, never bypassed here. */
    return TCPIP_SNTP_UTCSecondsGet() != 0;
}

void S2_TLS_Close(S2_TLS *t)
{
    if (t == NULL || !t->initialized) return;
    if (t->session != NULL) { wolfSSL_free(t->session); t->session = NULL; }
    if (t->context != NULL) { wolfSSL_CTX_free(t->context); t->context = NULL; }
    if (t->socket != INVALID_SOCKET) {
        TCPIP_TCP_Close((TCP_SOCKET)t->socket);
        t->socket = INVALID_SOCKET;
    }
    if (t->libraryOwned) { wolfSSL_Cleanup(); t->libraryOwned = false; }
    memset(t->writeBuffer, 0, sizeof(t->writeBuffer));
    memset(t->readBuffer, 0, sizeof(t->readBuffer));
    t->writeLength = t->readLength = t->readOffset = 0;
    t->readNeedsWrite = false;
    t->state = S2_TLS_IDLE;
}

static bool fail(S2_TLS *t, S2_TLS_ERROR error)
{
    S2_TLS_Close(t);
    t->error = error;
    t->state = S2_TLS_FAILED;
    return false;
}

static int receiveCiphertext(WOLFSSL *ssl, char *out, int length, void *user)
{
    S2_TLS *t = user;
    uint16_t ready;
    (void)ssl;
    if (length <= 0 || t->socket == INVALID_SOCKET) return WOLFSSL_CBIO_ERR_GENERAL;
    ready = TCPIP_TCP_GetIsReady((TCP_SOCKET)t->socket);
    if (ready == 0) return TCPIP_TCP_IsConnected((TCP_SOCKET)t->socket) ?
        WOLFSSL_CBIO_ERR_WANT_READ : WOLFSSL_CBIO_ERR_CONN_CLOSE;
    if (length < ready) ready = (uint16_t)length;
    return TCPIP_TCP_ArrayGet((TCP_SOCKET)t->socket, (uint8_t *)out, ready);
}

static int sendCiphertext(WOLFSSL *ssl, char *data, int length, void *user)
{
    S2_TLS *t = user;
    uint16_t ready;
    (void)ssl;
    if (length <= 0 || t->socket == INVALID_SOCKET) return WOLFSSL_CBIO_ERR_GENERAL;
    if (!TCPIP_TCP_IsConnected((TCP_SOCKET)t->socket)) return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    ready = TCPIP_TCP_PutIsReady((TCP_SOCKET)t->socket);
    if (ready == 0) return WOLFSSL_CBIO_ERR_WANT_WRITE;
    if (length < ready) ready = (uint16_t)length;
    return TCPIP_TCP_ArrayPut((TCP_SOCKET)t->socket, (const uint8_t *)data, ready);
}

bool S2_TLS_Initialize(S2_TLS *t, void *consumer, bool (*text)(void *, const char *, size_t))
{
    if (t == NULL || text == NULL) return false;
    memset(t, 0, sizeof(*t));
    t->socket = INVALID_SOCKET;
    t->consumer = consumer;
    t->text = text;
    t->initialized = true;
    return true;
}

static bool validHost(const char *host)
{
    size_t i, label = 0;
    bool hasLetter = false;
    if (host == NULL) return false;
    for (i = 0; i <= S2_TLS_HOST_MAX; i++) {
        unsigned c = (unsigned char)host[i];
        if (c == 0) return label != 0 && host[i - 1] != '-' && hasLetter;
        if (c == '.') {
            if (label == 0 || host[i - 1] == '-') return false;
            label = 0;
        } else {
            bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (!letter && !(c >= '0' && c <= '9') && c != '-') return false;
            if ((label == 0 && c == '-') || ++label > 63) return false;
            hasLetter = hasLetter || letter;
        }
    }
    return false;
}

bool S2_TLS_Start(S2_TLS *t, uint32_t ipv4, uint16_t port, const char *host,
    const uint8_t *ca, size_t caLength, uint32_t nowMs)
{
    IP_MULTI_ADDRESS address;
    TCP_OPTION_LINGER_DATA closeOptions = {0};
    if (t == NULL || !t->initialized || t->socket != INVALID_SOCKET || t->context != NULL) return false;
    t->error = S2_TLS_ERROR_NONE; t->wolfError = 0;
    t->shutdownComplete = false; t->lastNow = nowMs;
    if (!validHost(host) || ipv4 == 0 || port == 0 || ca == NULL || caLength == 0 || caLength > LONG_MAX)
        return fail(t, S2_TLS_ERROR_ARGUMENT);
    if (!NETWORK_MANAGER_IsReady()) return fail(t, S2_TLS_ERROR_NETWORK);
    if (!S2_TLS_ClockReady()) return fail(t, S2_TLS_ERROR_CLOCK);
    memcpy(t->hostname, host, strlen(host) + 1);
    if (wolfSSL_Init() != WOLFSSL_SUCCESS) return fail(t, S2_TLS_ERROR_MEMORY);
    t->libraryOwned = true;
    /* Explicit TLS 1.2, supported by the installed build; no legacy fallback. */
    t->context = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (t->context == NULL) return fail(t, S2_TLS_ERROR_MEMORY);
    wolfSSL_CTX_set_verify(t->context, SSL_VERIFY_PEER, NULL);
    if (wolfSSL_CTX_load_verify_buffer(t->context, ca, (long)caLength, SSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS)
        return fail(t, S2_TLS_ERROR_TRUST);
    wolfSSL_SetIORecv(t->context, receiveCiphertext);
    wolfSSL_SetIOSend(t->context, sendCiphertext);
    t->session = wolfSSL_new(t->context);
    if (t->session == NULL) return fail(t, S2_TLS_ERROR_MEMORY);
    if (wolfSSL_check_domain_name(t->session, t->hostname) != WOLFSSL_SUCCESS ||
        wolfSSL_UseSNI(t->session, WOLFSSL_SNI_HOST_NAME, t->hostname,
            (unsigned short)strlen(t->hostname)) != WOLFSSL_SUCCESS) return fail(t, S2_TLS_ERROR_TRUST);
    wolfSSL_SetIOReadCtx(t->session, t);
    wolfSSL_SetIOWriteCtx(t->session, t);
    memset(&address, 0, sizeof(address)); address.v4Add.Val = ipv4;
    t->socket = TCPIP_TCP_ClientOpen(IP_ADDRESS_TYPE_IPV4, port, &address);
    if (t->socket == INVALID_SOCKET) return fail(t, S2_TLS_ERROR_SOCKET);
    /* Bound cleanup to this socket; do not strand large TCP buffers on error. */
    if (!TCPIP_TCP_OptionsSet((TCP_SOCKET)t->socket, TCP_OPTION_LINGER, &closeOptions))
        return fail(t, S2_TLS_ERROR_SOCKET);
    t->phaseStarted = nowMs;
    t->state = S2_TLS_TCP_CONNECTING;
    return true;
}

void S2_TLS_Tasks(S2_TLS *t, uint32_t nowMs)
{
    int result;
    if (t == NULL || !t->initialized) return;
    t->lastNow = nowMs;
    if (t->state == S2_TLS_IDLE || t->state == S2_TLS_FAILED) return;
    if (!NETWORK_MANAGER_IsReady()) { fail(t, S2_TLS_ERROR_NETWORK); return; }
    if (!S2_TLS_ClockReady()) { fail(t, S2_TLS_ERROR_CLOCK); return; }
    if (t->state == S2_TLS_CLOSING) {
        TCP_SOCKET_INFO info;
        if ((uint32_t)(nowMs - t->phaseStarted) >= 2000u) { S2_TLS_Close(t); return; }
        if (!t->shutdownComplete) {
            result = wolfSSL_shutdown(t->session);
            if (result == WOLFSSL_SUCCESS) t->shutdownComplete = true;
            else if (result != WOLFSSL_SHUTDOWN_NOT_DONE) {
                t->wolfError = wolfSSL_get_error(t->session, result);
                if (t->wolfError != WOLFSSL_ERROR_WANT_READ && t->wolfError != WOLFSSL_ERROR_WANT_WRITE) {
                    S2_TLS_Close(t); return;
                }
            }
        }
        if (t->shutdownComplete && TCPIP_TCP_SocketInfoGet((TCP_SOCKET)t->socket, &info) && info.txPending == 0)
            S2_TLS_Close(t);
        return;
    }
    if (t->state == S2_TLS_VERIFIED) return; /* Read callbacks handle EOF after buffered data. */
    if ((uint32_t)(nowMs - t->phaseStarted) >= S2_TLS_PHASE_TIMEOUT_MS) {
        fail(t, S2_TLS_ERROR_TIMEOUT); return;
    }
    if (t->state == S2_TLS_TCP_CONNECTING) {
        if (!TCPIP_TCP_IsConnected((TCP_SOCKET)t->socket)) return;
        t->phaseStarted = nowMs;
        t->state = S2_TLS_HANDSHAKING;
    }
    result = wolfSSL_connect(t->session);
    if (result == WOLFSSL_SUCCESS) {
        /* With SSL_VERIFY_PEER, hostname checking and no override callback,
         * handshake success includes certificate validation. Do not depend on
         * wolfSSL_get_verify_result: this build omits OPENSSL_EXTRA. */
        t->state = S2_TLS_VERIFIED;
    } else {
        t->wolfError = wolfSSL_get_error(t->session, result);
        if (t->wolfError != WOLFSSL_ERROR_WANT_READ && t->wolfError != WOLFSSL_ERROR_WANT_WRITE)
            fail(t, S2_TLS_ERROR_HANDSHAKE);
    }
}

static bool verified(void *user)
{
    S2_TLS *t = user;
    return t->state == S2_TLS_VERIFIED && NETWORK_MANAGER_IsReady() && S2_TLS_ClockReady();
}
static int writePlaintext(void *user, const uint8_t *data, size_t length)
{
    S2_TLS *t = user;
    int result, error;
    if (!verified(t) || data == NULL || length == 0) return -1;
    if (t->readNeedsWrite) return 0; /* Retry pending SSL_read protocol output first. */
    if (t->writeLength == 0) {
        t->writeLength = length < sizeof(t->writeBuffer) ? length : sizeof(t->writeBuffer);
        memcpy(t->writeBuffer, data, t->writeLength);
    } else if (length < t->writeLength || memcmp(data, t->writeBuffer, t->writeLength) != 0) {
        fail(t, S2_TLS_ERROR_IO); return -1;
    }
    /* Stable pointer/length across WANT_READ/WANT_WRITE, even if the transport
     * compacts or appends its queue between retries. */
    result = wolfSSL_write(t->session, t->writeBuffer, (int)t->writeLength);
    if (result > 0 && (size_t)result <= t->writeLength) {
        memset(t->writeBuffer, 0, sizeof(t->writeBuffer)); t->writeLength = 0;
        return result;
    }
    error = wolfSSL_get_error(t->session, result);
    if (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE) return 0;
    t->wolfError = error; fail(t, S2_TLS_ERROR_IO); return -1;
}
static int readPlaintext(void *user, uint8_t *out, size_t capacity)
{
    S2_TLS *t = user;
    int result, error;
    size_t count;
    if (!verified(t) || out == NULL || capacity == 0) return -1;
    if (t->writeLength != 0) return 0; /* Finish pending SSL_write before SSL_read. */
    if (t->readOffset == t->readLength) {
        result = wolfSSL_read(t->session, t->readBuffer, sizeof(t->readBuffer));
        if (result <= 0) {
            error = wolfSSL_get_error(t->session, result);
            if (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE) {
                t->readNeedsWrite = error == WOLFSSL_ERROR_WANT_WRITE;
                return 0;
            }
            t->wolfError = error; fail(t, S2_TLS_ERROR_IO); return -1;
        }
        if ((size_t)result > sizeof(t->readBuffer)) { fail(t, S2_TLS_ERROR_IO); return -1; }
        t->readNeedsWrite = false;
        t->readOffset = 0; t->readLength = (size_t)result;
    }
    count = t->readLength - t->readOffset;
    if (count > capacity) count = capacity;
    memcpy(out, t->readBuffer + t->readOffset, count); t->readOffset += count;
    return (int)count;
}
static void closeBackend(void *user)
{
    S2_TLS *t = user;
    if (t->state == S2_TLS_CLOSING) return;
    if (verified(t) && t->writeLength == 0 && !t->readNeedsWrite) {
        /* SSL_write success means bytes queued to TCP, not necessarily sent.
         * Drain close reply/close_notify before reclaiming the socket. */
        t->phaseStarted = t->lastNow;
        t->shutdownComplete = false;
        t->state = S2_TLS_CLOSING;
    } else S2_TLS_Close(t);
}
static bool text(void *user, const char *message, size_t length)
{
    S2_TLS *t = user;
    return t->text(t->consumer, message, length);
}
bool S2_TLS_GetPlatform(S2_TLS *t, S2_TRANSPORT_PLATFORM *platform)
{
    if (t == NULL || !t->initialized || platform == NULL) return false;
    *platform = (S2_TRANSPORT_PLATFORM){t, verified, readPlaintext, writePlaintext,
        closeBackend, TULIP_CryptoRandom, S2_SHA1_WebSocketAccept, text};
    return true;
}
