#ifndef S2_TOKEN_STORE_H
#define S2_TOKEN_STORE_H
#include "s2_session.h"
#define S2_TOKEN_STORE_SLOT_SIZE 568u
/* Dedicated slots, isolated failure domains. Writes may tear but MUST NOT alter
 * the other slot. sync must make all preceding writes durable and ordered.
 * Reads must report I/O errors, not substitute erased data. Main-loop only.
 * No implementation here assumes that FAT files or EEPROM satisfy this contract. */
typedef struct {
    void *user;
    bool (*read)(void *, unsigned slot, size_t offset, void *, size_t);
    bool (*write)(void *, unsigned slot, size_t offset, const void *, size_t);
    bool (*sync)(void *);
} S2_TOKEN_STORE_IO;
typedef enum { S2_TOKEN_STORE_OK=0, S2_TOKEN_STORE_EMPTY,
    S2_TOKEN_STORE_INVALID, S2_TOKEN_STORE_IO_ERROR } S2_TOKEN_STORE_RESULT;
typedef struct {
    S2_TOKEN_STORE_IO io;
    uint8_t binding[32]; /* Stable identity of this pairing, supplied by owner. */
} S2_TOKEN_STORE;
/* Zero-initialize; binding must identify the paired CEM/trust/resource together.
 * Changing pairing requires separate storage or explicit lifecycle management.
 * Tokens are plaintext at rest; backend access protection is required. */
bool S2_TOKEN_STORE_Init(S2_TOKEN_STORE *, const S2_TOKEN_STORE_IO *, const uint8_t binding[32]);
S2_TOKEN_STORE_RESULT S2_TOKEN_STORE_Load(S2_TOKEN_STORE *, S2_SESSION_TOKENS *);
/* Directly usable as S2_SESSION_STORAGE.persist with user pointing to store.
 * No RAM cache: after uncertain commit each operation rereads both slots. */
bool S2_TOKEN_STORE_Persist(void *, const S2_SESSION_TOKENS *);
void S2_TOKEN_STORE_Clear(S2_TOKEN_STORE *);
#endif
