#include "s2_sha1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static uint8_t data[1000001];
int main(void)
{
    uint8_t digest[20], unchanged[20];
    size_t length;
#ifdef _WIN32
    assert(_setmode(_fileno(stdin), _O_BINARY) != -1);
#endif
    length = fread(data, 1, sizeof(data), stdin);
    assert(!ferror(stdin) && length < sizeof(data));
    memset(digest,0x55,sizeof(digest)); memcpy(unchanged,digest,sizeof(digest));
    assert(!S2_SHA1_WebSocketAccept(NULL,NULL,1,digest));
    assert(memcmp(digest,unchanged,sizeof(digest))==0);
    assert(!S2_SHA1_WebSocketAccept(NULL,data,length,NULL));
    assert(S2_SHA1_WebSocketAccept(NULL,NULL,0,digest));
    assert(S2_SHA1_WebSocketAccept(NULL,data,length,digest));
    for (unsigned i = 0; i < sizeof(digest); i++) printf("%02x",digest[i]);
    putchar('\n');
    return 0;
}
