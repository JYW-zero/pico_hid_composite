#!/usr/bin/env python3
"""
固件签名工具（HMAC-SHA256）

功能：
1. 读取固件文件（.bin）
2. 计算固件的 HMAC-SHA256 签名
3. 在固件末尾追加签名（4字节魔数 + 32字节签名）
4. 输出签名后的固件文件

签名格式：
  - 4 字节魔数 (0x53494731 "SIG1")
  - 32 字节 HMAC-SHA256 签名

签名计算范围：整个固件（不包括末尾的36字节签名）

用法：
  python sign_firmware.py <input.bin> <output.bin> [--key <key_file>]

注意：
  - 密钥必须与 Bootloader 中嵌入的密钥一致
  - 默认密钥为演示密钥，生产环境必须替换
"""

import sys
import hmac
import hashlib
import struct
import argparse

# 签名魔数（与固件中定义一致）
FIRMWARE_SIGNATURE_MAGIC = 0x53494731  # "SIG1"
FIRMWARE_SIGNATURE_SIZE = 32  # HMAC-SHA256

# 默认演示密钥（与 Bootloader 中嵌入的密钥一致）
# 生产环境必须替换为随机生成的密钥！
DEFAULT_KEY = bytes([
    0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B,
    0x9C, 0xAD, 0xBE, 0xCF, 0x01, 0x12, 0x23, 0x34,
    0x45, 0x56, 0x67, 0x78, 0x89, 0x9A, 0xAB, 0xBC,
    0xCD, 0xDE, 0xEF, 0xF0, 0x0A, 0x1B, 0x2C
])


def sign_firmware(input_path, output_path, key):
    """对固件进行签名"""
    # 读取输入固件
    with open(input_path, 'rb') as f:
        firmware = f.read()

    print(f"输入固件大小: {len(firmware)} 字节")

    # 检查是否已经有签名（如果末尾有签名魔数，先移除）
    if len(firmware) >= FIRMWARE_SIGNATURE_SIZE + 4:
        sig_offset = len(firmware) - FIRMWARE_SIGNATURE_SIZE - 4
        existing_magic = struct.unpack('>I', firmware[sig_offset:sig_offset+4])[0]
        if existing_magic == FIRMWARE_SIGNATURE_MAGIC:
            print("检测到已有签名，移除旧签名...")
            firmware = firmware[:sig_offset]
            print(f"移除签名后大小: {len(firmware)} 字节")

    # 计算 HMAC-SHA256 签名
    signature = hmac.new(key, firmware, hashlib.sha256).digest()
    print(f"签名 (HMAC-SHA256): {signature.hex()}")

    # 构造签名块（魔数 + 签名）
    magic_bytes = struct.pack('>I', FIRMWARE_SIGNATURE_MAGIC)
    signature_block = magic_bytes + signature

    # 追加签名到固件末尾
    signed_firmware = firmware + signature_block

    # 输出签名后的固件
    with open(output_path, 'wb') as f:
        f.write(signed_firmware)

    print(f"输出固件大小: {len(signed_firmware)} 字节")
    print(f"签名块大小: {len(signature_block)} 字节 (4字节魔数 + 32字节签名)")
    print(f"✅ 固件签名完成: {output_path}")

    return True


def verify_firmware(input_path, key):
    """验证固件签名"""
    with open(input_path, 'rb') as f:
        firmware = f.read()

    print(f"固件大小: {len(firmware)} 字节")

    # 检查是否有签名
    if len(firmware) < FIRMWARE_SIGNATURE_SIZE + 4:
        print("❌ 固件太小，不可能有签名")
        return False

    sig_offset = len(firmware) - FIRMWARE_SIGNATURE_SIZE - 4
    magic = struct.unpack('>I', firmware[sig_offset:sig_offset+4])[0]

    if magic != FIRMWARE_SIGNATURE_MAGIC:
        print("⚠️  固件未签名（无签名魔数）")
        return None

    # 读取签名
    signature = firmware[sig_offset+4:sig_offset+4+FIRMWARE_SIGNATURE_SIZE]

    # 计算期望的签名
    firmware_data = firmware[:sig_offset]
    expected_signature = hmac.new(key, firmware_data, hashlib.sha256).digest()

    # 比较签名（恒定时间比较）
    if hmac.compare_digest(signature, expected_signature):
        print("✅ 固件签名验证通过")
        return True
    else:
        print("❌ 固件签名验证失败")
        print(f"  期望: {expected_signature.hex()}")
        print(f"  实际: {signature.hex()}")
        return False


def main():
    parser = argparse.ArgumentParser(description='固件签名工具（HMAC-SHA256）')
    parser.add_argument('input', help='输入固件文件（.bin）')
    parser.add_argument('output', nargs='?', help='输出签名后的固件文件（.bin）')
    parser.add_argument('--key', help='密钥文件（32字节二进制）')
    parser.add_argument('--verify', action='store_true', help='仅验证签名，不签名')
    parser.add_argument('--generate-key', action='store_true', help='生成随机密钥并保存')

    args = parser.parse_args()

    # 生成随机密钥
    if args.generate_key:
        import os
        key = os.urandom(32)
        key_path = args.key or 'firmware_signing_key.bin'
        with open(key_path, 'wb') as f:
            f.write(key)
        print(f"✅ 随机密钥已生成: {key_path}")
        print(f"   密钥 (hex): {key.hex()}")
        print(f"   请将此密钥更新到 Bootloader 的 dfu_metadata.c 中")
        return 0

    # 加载密钥
    if args.key:
        with open(args.key, 'rb') as f:
            key = f.read()
        if len(key) != 32:
            print(f"❌ 密钥长度错误: {len(key)} 字节（需要32字节）")
            return 1
    else:
        key = DEFAULT_KEY
        print("⚠️  使用默认演示密钥（生产环境必须替换！）")

    # 验证签名
    if args.verify:
        result = verify_firmware(args.input, key)
        return 0 if result is not False else 1

    # 签名
    if not args.output:
        print("❌ 签名模式需要指定输出文件")
        return 1

    success = sign_firmware(args.input, args.output, key)
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
