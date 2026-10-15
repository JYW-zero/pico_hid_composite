/**
 * @file hmac_sha256.c
 * @brief HMAC-SHA256 消息认证码实现（RFC 2104）
 *
 * 标准实现，经过 RFC 4231 测试向量验证。
 * 代码大小约 500B，RAM 占用约 200 字节。
 */
#include "middleware/hmac_sha256.h"
#include <string.h>
#include <stdbool.h>

/* HMAC 内部和外部填充 */
#define HMAC_IPAD    0x36
#define HMAC_OPAD    0x5C

void hmac_sha256(const uint8_t* key, size_t key_len,
                  const uint8_t* data, size_t data_len,
                  uint8_t digest[HMAC_SHA256_DIGEST_SIZE])
{
    sha256_ctx_t ctx;
    uint8_t k_ipad[SHA256_BLOCK_SIZE];  /* 内部填充密钥 */
    uint8_t k_opad[SHA256_BLOCK_SIZE];  /* 外部填充密钥 */
    uint8_t tk[SHA256_DIGEST_SIZE];      /* 缩短后的密钥 */

    if (key == NULL || data == NULL || digest == NULL) return;

    /* 1. 如果密钥长度 > 64字节，先用 SHA-256 缩短 */
    if (key_len > SHA256_BLOCK_SIZE) {
        sha256(key, key_len, tk);
        key = tk;
        key_len = SHA256_DIGEST_SIZE;
    }

    /* 2. 初始化填充密钥 */
    memset(k_ipad, 0, sizeof(k_ipad));
    memset(k_opad, 0, sizeof(k_opad));
    memcpy(k_ipad, key, key_len);
    memcpy(k_opad, key, key_len);

    /* 3. XOR 填充 */
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        k_ipad[i] ^= HMAC_IPAD;
        k_opad[i] ^= HMAC_OPAD;
    }

    /* 4. 内部哈希：H(K XOR ipad, text) */
    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, digest);

    /* 5. 外部哈希：H(K XOR opad, H(K XOR ipad, text)) */
    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, SHA256_BLOCK_SIZE);
    sha256_update(&ctx, digest, SHA256_DIGEST_SIZE);
    sha256_final(&ctx, digest);
}

bool hmac_sha256_verify(const uint8_t* key, size_t key_len,
                         const uint8_t* data, size_t data_len,
                         const uint8_t expected_digest[HMAC_SHA256_DIGEST_SIZE])
{
    uint8_t computed[HMAC_SHA256_DIGEST_SIZE];
    uint8_t diff = 0;

    if (key == NULL || data == NULL || expected_digest == NULL) {
        return false;
    }

    /* 计算 HMAC */
    hmac_sha256(key, key_len, data, data_len, computed);

    /* 恒定时间比较（防止时序攻击） */
    for (int i = 0; i < HMAC_SHA256_DIGEST_SIZE; i++) {
        diff |= computed[i] ^ expected_digest[i];
    }

    return (diff == 0);
}
