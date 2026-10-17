/*
*   Minimal SHA-256 for the timelock subsystem.
*   Public-domain reference implementation (FIPS 180-4).
*/

#pragma once

#include <3ds/types.h>
#include <stddef.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

typedef struct {
    u32  state[8];
    u64  bitlen;
    u32  datalen;
    u8   data[SHA256_BLOCK_SIZE];
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const u8 *data, size_t len);
void sha256_final(sha256_ctx *ctx, u8 hash[SHA256_DIGEST_SIZE]);

// One-shot convenience.
void sha256(const u8 *data, size_t len, u8 hash[SHA256_DIGEST_SIZE]);
