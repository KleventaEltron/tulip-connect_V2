#include "s2_transport.h"
#include <string.h>

static void releaseBackend(S2_TRANSPORT *t, S2_TRANSPORT_STATE state)
{
    if (t->backendOwned) {
        t->backendOwned = false;
        t->platform.close(t->platform.user);
    }
    memset(t->queue, 0, sizeof(t->queue)); /* Includes token-bearing upgrade. */
    t->queueOffset = t->queueLength = t->headerLength = 0;
    t->state = state;
}

static bool randomBytes(void *user, uint8_t *out, size_t length)
{
    S2_TRANSPORT *t = user;
    return t->platform.random(t->platform.user, out, length);
}
static bool sha1(void *user, const uint8_t *data, size_t length, uint8_t out[20])
{
    S2_TRANSPORT *t = user;
    return t->platform.sha1(t->platform.user, data, length, out);
}
static bool enqueue(void *user, const uint8_t *frame, size_t length)
{
    S2_TRANSPORT *t = user;
    size_t remaining = t->queueLength - t->queueOffset;
    /* Do not send queued application data after receiving a peer close, or
     * splice a close frame into a partially written frame. Abort instead. */
    if (length > 0 && (frame[0] & 15u) == 8u && remaining != 0) return false;
    if (length > sizeof(t->queue) - remaining) return false;
    memmove(t->queue, t->queue + t->queueOffset, remaining);
    memcpy(t->queue + remaining, frame, length);
    t->queueOffset = 0;
    t->queueLength = remaining + length;
    return true;
}
static bool text(void *user, const char *message, size_t length)
{
    S2_TRANSPORT *t = user;
    return t->platform.text(t->platform.user, message, length);
}
static void ended(void *user)
{
    S2_TRANSPORT *t = user;
    /* Defer close until an already queued close reply drains. No codec reentry. */
    t->drainTimerActive = false;
    t->state = S2_TRANSPORT_DRAINING;
}

bool S2_TRANSPORT_Initialize(S2_TRANSPORT *t, const S2_TRANSPORT_PLATFORM *platform)
{
    S2_TRANSPORT_PLATFORM copy;
    if (t == NULL || platform == NULL || platform->verified == NULL ||
        platform->read == NULL || platform->write == NULL || platform->close == NULL ||
        platform->random == NULL || platform->sha1 == NULL || platform->text == NULL) return false;
    copy = *platform;
    memset(t, 0, sizeof(*t));
    t->platform = copy;
    t->initialized = true;
    return true;
}

bool S2_TRANSPORT_Start(S2_TRANSPORT *t, const char *authority,
    const char *path, const char *commToken, uint32_t nowMs)
{
    S2_WS_PLATFORM callbacks;
    if (t == NULL || !t->initialized || t->backendOwned) return false;
    t->backendOwned = true;
    t->drainTimerActive = t->writeTimerActive = false;
    t->headerLength = t->queueOffset = t->queueLength = 0;
    t->phaseStarted = nowMs;
    if (!t->platform.verified(t->platform.user)) {
        releaseBackend(t, S2_TRANSPORT_FAILED);
        return false;
    }
    callbacks = (S2_WS_PLATFORM){t, randomBytes, sha1, enqueue, text, ended};
    if (S2_WS_Initialize(&t->websocket, &callbacks) != S2_WS_OK ||
        S2_WS_BuildUpgrade(&t->websocket, authority, path, commToken,
            (char *)t->queue, sizeof(t->queue)) != S2_WS_OK) {
        releaseBackend(t, S2_TRANSPORT_FAILED);
        return false;
    }
    t->queueLength = strlen((const char *)t->queue);
    t->state = S2_TRANSPORT_UPGRADING;
    return true;
}

void S2_TRANSPORT_Stop(S2_TRANSPORT *t)
{
    if (t == NULL || !t->initialized) return;
    S2_WS_Reset(&t->websocket);
    releaseBackend(t, S2_TRANSPORT_IDLE);
}

static bool writePending(S2_TRANSPORT *t, uint32_t nowMs)
{
    size_t remaining = t->queueLength - t->queueOffset;
    int written;
    if (remaining == 0) { t->writeTimerActive = false; return true; }
    if (!t->writeTimerActive) { t->writeProgress = nowMs; t->writeTimerActive = true; }
    if ((uint32_t)(nowMs - t->writeProgress) >= S2_TRANSPORT_WRITE_TIMEOUT_MS) return false;
    if (remaining > S2_TRANSPORT_IO_BUDGET) remaining = S2_TRANSPORT_IO_BUDGET;
    written = t->platform.write(t->platform.user, t->queue + t->queueOffset, remaining);
    if (written < 0 || (size_t)written > remaining) return false;
    if (written > 0) t->writeProgress = nowMs;
    /* Erase consumed request bytes as soon as the backend has copied them. */
    memset(t->queue + t->queueOffset, 0, (size_t)written);
    t->queueOffset += (size_t)written;
    if (t->queueOffset == t->queueLength) t->queueOffset = t->queueLength = 0;
    return true;
}

void S2_TRANSPORT_Tasks(S2_TRANSPORT *t, bool networkReady, uint32_t nowMs)
{
    int received;
    size_t i = 0;
    if (t == NULL || !t->initialized || !t->backendOwned) return;
    if (!networkReady) { S2_TRANSPORT_Stop(t); return; }
    if (!t->platform.verified(t->platform.user)) goto failed;
    if (t->state == S2_TRANSPORT_DRAINING && !t->drainTimerActive) {
        t->phaseStarted = nowMs;
        t->drainTimerActive = true;
    }
    if (t->state == S2_TRANSPORT_DRAINING &&
        (uint32_t)(nowMs - t->phaseStarted) >= S2_TRANSPORT_DRAIN_TIMEOUT_MS) {
        releaseBackend(t, S2_TRANSPORT_IDLE);
        return;
    }
    if (!writePending(t, nowMs)) goto failed;
    if (t->state == S2_TRANSPORT_DRAINING) {
        if (t->queueLength == 0 || (uint32_t)(nowMs - t->phaseStarted) >= S2_TRANSPORT_DRAIN_TIMEOUT_MS)
            releaseBackend(t, S2_TRANSPORT_IDLE);
        return;
    }
    if (t->state == S2_TRANSPORT_UPGRADING &&
        (uint32_t)(nowMs - t->phaseStarted) >= S2_TRANSPORT_UPGRADE_TIMEOUT_MS) goto failed;
    received = t->platform.read(t->platform.user, t->readBuffer, sizeof(t->readBuffer));
    if (received < 0 || (size_t)received > sizeof(t->readBuffer)) goto failed;
    if (t->state == S2_TRANSPORT_UPGRADING) {
        while (i < (size_t)received) {
            size_t n;
            if (t->headerLength == sizeof(t->headers)) goto failed;
            t->headers[t->headerLength++] = (char)t->readBuffer[i++];
            n = t->headerLength;
            if (n >= 4 && memcmp(t->headers + n - 4, "\r\n\r\n", 4) == 0) {
                if (t->queueLength != 0 || S2_WS_AcceptUpgrade(&t->websocket, t->headers, n) != S2_WS_OK)
                    goto failed;
                t->state = S2_TRANSPORT_OPEN;
                break;
            }
        }
    }
    if (t->state == S2_TRANSPORT_OPEN && i < (size_t)received) {
        S2_WS_RESULT result = S2_WS_Feed(&t->websocket, t->readBuffer + i, (size_t)received - i);
        if (result == S2_WS_IO_ERROR) goto failed;
        if (t->state == S2_TRANSPORT_DRAINING) {
            t->phaseStarted = nowMs;
            t->drainTimerActive = true;
        }
    }
    return;
failed:
    S2_WS_Reset(&t->websocket);
    releaseBackend(t, S2_TRANSPORT_FAILED);
}

S2_WS_RESULT S2_TRANSPORT_SendText(S2_TRANSPORT *t, const char *message, size_t length)
{
    if (t == NULL || !t->initialized || t->state != S2_TRANSPORT_OPEN) return S2_WS_CLOSED;
    S2_WS_RESULT result = S2_WS_SendText(&t->websocket, message, length);
    if (result == S2_WS_IO_ERROR) releaseBackend(t, S2_TRANSPORT_FAILED);
    return result;
}

S2_TRANSPORT_STATE S2_TRANSPORT_GetState(const S2_TRANSPORT *t)
{
    return t != NULL && t->initialized ? t->state : S2_TRANSPORT_IDLE;
}
