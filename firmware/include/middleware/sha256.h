/**
 * @file sha256.h
 * @brief SHA-256 哈希算法接口
 *
 * 标准 FIPS 180-4 SHA-256 实现，用于固件签名验证。
 * 代码大小约 1KB，RAM 占用约 112 字节，适合嵌入式系统。
 */
#ifndef MIDDLEWARE_SHA256_H
#define MIDDLEWARE_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SHA-256 输出大小（字节） */
#define SHA256_DIGEST_SIZE    32

/* SHA-256 块大小（字节） */
#define SHA256_BLOCK_SIZE     64

/* SHA-256 上下文 */
typedef struct {
    uint32_t state[8];           /* 中间哈希状态 */
    uint64_t bit_count;           /* 总位数 */
    uint8_t  buffer[SHA256_BLOCK_SIZE];  /* 输入缓冲区 */
    size_t   buffer_len;          /* 缓冲区当前长度 */
} sha256_ctx_t;

/**
 * @brief 初始化 SHA-256 上下文
 * @param ctx 上下文指针
 */
void sha256_init(sha256_ctx_t* ctx);

/**
 * @brief 更新 SHA-256 哈希（可多次调用）
 * @param ctx 上下文指针
 * @param data 输入数据
 * @param len 数据长度（字节）
 */
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief 完成 SHA-256 哈希计算，输出摘要
 * @param ctx 上下文指针
 * @param digest 输出摘要（32字节）
 */
void sha256_final(sha256_ctx_t* ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

/**
 * @brief 一次性计算 SHA-256 哈希
 * @param data 输入数据
 * @param len 数据长度（字节）
 * @param digest 输出摘要（32字节）
 */
void sha256(const uint8_t* data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_SHA256_H */
