/**
 * @file hmac_sha256.h
 * @brief HMAC-SHA256 消息认证码接口
 *
 * 基于 SHA-256 的 HMAC 实现（RFC 2104），用于固件签名验证。
 * 使用对称密钥，32字节签名，安全性高，实现简单。
 */
#ifndef MIDDLEWARE_HMAC_SHA256_H
#define MIDDLEWARE_HMAC_SHA256_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "middleware/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HMAC-SHA256 输出大小（字节） */
#define HMAC_SHA256_DIGEST_SIZE    SHA256_DIGEST_SIZE  /* 32 */

/* HMAC 密钥最大长度（字节） */
#define HMAC_SHA256_MAX_KEY_SIZE    64

/**
 * @brief 计算 HMAC-SHA256
 * @param key 密钥
 * @param key_len 密钥长度（字节）
 * @param data 输入数据
 * @param data_len 数据长度（字节）
 * @param digest 输出 HMAC 摘要（32字节）
 */
void hmac_sha256(const uint8_t* key, size_t key_len,
                  const uint8_t* data, size_t data_len,
                  uint8_t digest[HMAC_SHA256_DIGEST_SIZE]);

/**
 * @brief 验证 HMAC-SHA256 签名（恒定时间比较，防止时序攻击）
 * @param key 密钥
 * @param key_len 密钥长度（字节）
 * @param data 输入数据
 * @param data_len 数据长度（字节）
 * @param expected_digest 期望的 HMAC 摘要（32字节）
 * @return true=验证通过，false=验证失败
 */
bool hmac_sha256_verify(const uint8_t* key, size_t key_len,
                         const uint8_t* data, size_t data_len,
                         const uint8_t expected_digest[HMAC_SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_HMAC_SHA256_H */
