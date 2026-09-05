/*
*   Minimal SHA-256 (FIPS 180-4). Public domain.
*   Used by the timelock subsystem to hash PIN+salt for storage.
*/

#include "sha256.h"
#include <string.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR((x),  2) ^ ROTR((x), 13) ^ ROTR((x), 22))
#define BSIG1(x) (ROTR((x),  6) ^ ROTR((x), 11) ^ ROTR((x), 25))
#define SSIG0(x) (ROTR((x),  7) ^ ROTR((x), 18) ^ ((x) >>  3))
#define SSIG1(x) (ROTR((x), 17) ^ ROTR((x), 19) ^ ((x) >> 10))

static const u32 K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static void sha256_transform(sha256_ctx *ctx, const u8 block[SHA256_BLOCK_SIZE])
{
    u32 w[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4)
        w[i] = ((u32)block[j] << 24) | ((u32)block[j+1] << 16) | ((u32)block[j+2] << 8) | (u32)block[j+3];
    for (int i = 16; i < 64; ++i)
        w[i] = SSIG1(w[i-2]) + w[i-7] + SSIG0(w[i-15]) + w[i-16];

    u32 a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    u32 e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 64; ++i) {
        u32 t1 = h + BSIG1(e) + CH(e, f, g) + K[i] + w[i];
        u32 t2 = BSIG0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void sha256_init(sha256_ctx *ctx)
{
    ctx->bitlen  = 0;
    ctx->datalen = 0;
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
}

void sha256_update(sha256_ctx *ctx, const u8 *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void sha256_final(sha256_ctx *ctx, u8 hash[SHA256_DIGEST_SIZE])
{
    u32 i = ctx->datalen;
    ctx->data[i++] = 0x80;

    if (i > 56) {
        while (i < SHA256_BLOCK_SIZE) ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56) ctx->data[i++] = 0;

    ctx->bitlen += (u64)ctx->datalen * 8u;
    for (int j = 7; j >= 0; --j)
        ctx->data[56 + (7 - j)] = (u8)((ctx->bitlen >> (j * 8)) & 0xff);
    sha256_transform(ctx, ctx->data);

    for (int j = 0; j < 8; ++j) {
        hash[4*j + 0] = (u8)((ctx->state[j] >> 24) & 0xff);
        hash[4*j + 1] = (u8)((ctx->state[j] >> 16) & 0xff);
        hash[4*j + 2] = (u8)((ctx->state[j] >>  8) & 0xff);
        hash[4*j + 3] = (u8)( ctx->state[j]        & 0xff);
    }
}

void sha256(const u8 *data, size_t len, u8 hash[SHA256_DIGEST_SIZE])
{
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}
