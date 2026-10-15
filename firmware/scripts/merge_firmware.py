#!/usr/bin/env python3
"""
合并 Bootloader 和应用程序的 UF2 文件。

Bootloader 位于 Flash 起始地址 (0x10000000)，
应用程序位于 0x10009000。
合并后生成完整固件，可直接通过 BOOTSEL 模式烧录。

用法:
    python merge_firmware.py bootloader.uf2 app.uf2 output.uf2
"""
import sys
import struct
import os

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30

def read_uf2(filepath):
    """读取 UF2 文件，返回块列表"""
    blocks = []
    with open(filepath, 'rb') as f:
        while True:
            data = f.read(512)
            if len(data) < 512:
                break
            # 解析 UF2 块头
            magic0, magic1 = struct.unpack_from('<II', data, 0)
            if magic0 != UF2_MAGIC_START0 or magic1 != UF2_MAGIC_START1:
                continue
            target_addr = struct.unpack_from('<I', data, 12)[0]
            payload_size = struct.unpack_from('<I', data, 16)[0]
            blocks.append({
                'addr': target_addr,
                'size': payload_size,
                'data': data
            })
    return blocks

def write_uf2(filepath, blocks):
    """写入 UF2 文件"""
    with open(filepath, 'wb') as f:
        for block in blocks:
            f.write(block['data'])

def merge_uf2(bootloader_path, app_path, output_path):
    """合并 Bootloader 和应用程序 UF2"""
    print(f"读取 Bootloader: {bootloader_path}")
    boot_blocks = read_uf2(bootloader_path)
    print(f"  Bootloader 块数: {len(boot_blocks)}")
    if boot_blocks:
        print(f"  地址范围: 0x{boot_blocks[0]['addr']:08X} - 0x{boot_blocks[-1]['addr'] + boot_blocks[-1]['size']:08X}")

    print(f"读取应用程序: {app_path}")
    app_blocks = read_uf2(app_path)
    print(f"  应用程序块数: {len(app_blocks)}")
    if app_blocks:
        print(f"  地址范围: 0x{app_blocks[0]['addr']:08X} - 0x{app_blocks[-1]['addr'] + app_blocks[-1]['size']:08X}")

    # 检查地址重叠
    if boot_blocks and app_blocks:
        boot_end = boot_blocks[-1]['addr'] + boot_blocks[-1]['size']
        app_start = app_blocks[0]['addr']
        if boot_end > app_start:
            print(f"警告: Bootloader (0x{boot_end:08X}) 与应用程序 (0x{app_start:08X}) 地址重叠!")
            return False

    # 合并
    merged = boot_blocks + app_blocks
    merged.sort(key=lambda x: x['addr'])

    print(f"写入合并文件: {output_path}")
    write_uf2(output_path, merged)
    print(f"  总块数: {len(merged)}")
    print(f"  文件大小: {os.path.getsize(output_path)} 字节")
    print("合并完成!")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("用法: python merge_firmware.py <bootloader.uf2> <app.uf2> <output.uf2>")
        sys.exit(1)

    success = merge_uf2(sys.argv[1], sys.argv[2], sys.argv[3])
    sys.exit(0 if success else 1)
