/**
 * @file sha256.c
 * @brief SHA-256 哈希算法实现（FIPS 180-4 标准）
 *
 * 标准实现，经过 NIST 测试向量验证。
 * 代码大小约 1KB，RAM 占用约 112 字节。
 */
#include "middleware/sha256.h"
#include <string.h>

/* SHA-256 轮常量（FIPS 180-4 第4.2.2节） */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* 循环右移 */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/* SHA-256 压缩函数（处理一个512位块） */
static void sha256_transform(sha256_ctx_t* ctx, const uint8_t* block)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;

    /* 1. 准备消息调度表 */
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    /* 2. 初始化工作变量 */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    /* 3. 主循环（64轮） */
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        t1 = h + S1 + ch + K[i] + w[i];

        uint32_t S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* 4. 更新中间哈希值 */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_init(sha256_ctx_t* ctx)
{
    if (ctx == NULL) return;

    /* SHA-256 初始哈希值（FIPS 180-4 第5.3.3节） */
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;

    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len)
{
    if (ctx == NULL || data == NULL || len == 0) return;

    ctx->bit_count += (uint64_t)len * 8;

    /* 处理缓冲区中的剩余数据 */
    if (ctx->buffer_len > 0) {
        size_t need = SHA256_BLOCK_SIZE - ctx->buffer_len;
        if (len < need) {
            memcpy(ctx->buffer + ctx->buffer_len, data, len);
            ctx->buffer_len += len;
            return;
        }
        memcpy(ctx->buffer + ctx->buffer_len, data, need);
        sha256_transform(ctx, ctx->buffer);
        data += need;
        len -= need;
        ctx->buffer_len = 0;
    }

    /* 处理完整的块 */
    while (len >= SHA256_BLOCK_SIZE) {
        sha256_transform(ctx, data);
        data += SHA256_BLOCK_SIZE;
        len -= SHA256_BLOCK_SIZE;
    }

    /* 保存剩余数据到缓冲区 */
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
}

void sha256_final(sha256_ctx_t* ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
    if (ctx == NULL || digest == NULL) return;

    /* 1. 追加 0x80 */
    ctx->buffer[ctx->buffer_len++] = 0x80;

    /* 2. 如果缓冲区剩余空间不足8字节，先处理一个块 */
    if (ctx->buffer_len > 56) {
        while (ctx->buffer_len < SHA256_BLOCK_SIZE) {
            ctx->buffer[ctx->buffer_len++] = 0;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }

    /* 3. 填充0直到56字节 */
    while (ctx->buffer_len < 56) {
        ctx->buffer[ctx->buffer_len++] = 0;
    }

    /* 4. 追加64位大端长度 */
    for (int i = 7; i >= 0; i--) {
        ctx->buffer[ctx->buffer_len++] = (uint8_t)((ctx->bit_count >> (i * 8)) & 0xFF);
    }

    /* 5. 处理最后一个块 */
    sha256_transform(ctx, ctx->buffer);

    /* 6. 输出大端序摘要 */
    for (int i = 0; i < 8; i++) {
        digest[i * 4]     = (uint8_t)((ctx->state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xFF);
    }
}

void sha256(const uint8_t* data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE])
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}
