#include "s2_sha1.h"
#include <string.h>

/* SHA-1 compression and padding per RFC3174, implemented independently of
 * wolfSSL so the project's NO_SHA configuration can remain unchanged.
 * Sixteen-word circular schedule keeps the local workspace bounded. */
static uint32_t rotate(uint32_t value, unsigned count)
{
    return (value << count) | (value >> (32u - count));
}

static void compress(uint32_t hash[5], const uint8_t block[64])
{
    uint32_t words[16], a = hash[0], b = hash[1], c = hash[2], d = hash[3], e = hash[4];
    unsigned i;
    for (i = 0; i < 16; i++) {
        const uint8_t *p = block + i * 4;
        words[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                   ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    for (i = 0; i < 80; i++) {
        uint32_t function, constant, next;
        if (i >= 16) words[i & 15u] = rotate(words[(i - 3u) & 15u] ^
            words[(i - 8u) & 15u] ^ words[(i - 14u) & 15u] ^ words[i & 15u], 1);
        if (i < 20) { function = (b & c) | (~b & d); constant = 0x5a827999u; }
        else if (i < 40) { function = b ^ c ^ d; constant = 0x6ed9eba1u; }
        else if (i < 60) { function = (b & c) | (b & d) | (c & d); constant = 0x8f1bbcdcu; }
        else { function = b ^ c ^ d; constant = 0xca62c1d6u; }
        next = rotate(a, 5) + function + e + constant + words[i & 15u];
        e = d; d = c; c = rotate(b, 30); b = a; a = next;
    }
    hash[0] += a; hash[1] += b; hash[2] += c; hash[3] += d; hash[4] += e;
}

bool S2_SHA1_WebSocketAccept(void *user, const uint8_t *data, size_t length, uint8_t out[20])
{
    uint32_t hash[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    uint8_t block[64];
    uint64_t bits;
    size_t remaining = length;
    unsigned i;
    (void)user;
    if (out == NULL || (data == NULL && length != 0)) return false;
#if SIZE_MAX > (UINT64_MAX / 8u)
    if (length > UINT64_MAX / 8u) return false;
#endif
    bits = (uint64_t)length * 8u;
    while (remaining >= sizeof(block)) {
        compress(hash, data);
        data += sizeof(block);
        remaining -= sizeof(block);
    }
    memset(block, 0, sizeof(block));
    if (remaining != 0) memcpy(block, data, remaining);
    block[remaining] = 0x80;
    if (remaining >= 56) {
        compress(hash, block);
        memset(block, 0, sizeof(block));
    }
    for (i = 0; i < 8; i++) block[63u - i] = (uint8_t)(bits >> (8u * i));
    compress(hash, block);
    for (i = 0; i < 20; i++) out[i] = (uint8_t)(hash[i / 4u] >> (24u - 8u * (i & 3u)));
    return true;
}
