#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB HID 配置通道测试脚本（分块协议版）
协议说明：
  - Report ID 5 (0x05)：配置块 0（偏移 0-62，63 字节）
  - Report ID 6 (0x06)：设备信息 Feature 报告（32字节）
  - Report ID 7 (0x07)：控制命令 Feature 报告（1字节）
  - Report ID 8 (0x08)：配置块 1（偏移 63-125，63 字节）
  - Report ID 9 (0x09)：配置块 2（偏移 126-188，63 字节）

依赖: pip install hidapi
"""
import hid
import struct
import sys
import time

# ==================== 配置 ====================
VID = 0xCAFE      # 厂商 ID
PID = 0x4004      # 产品 ID (HID 复合设备)

REPORT_ID_CONFIG_BLOCK0 = 5   # 配置块 0
REPORT_ID_DEVICE_INFO    = 6   # 设备信息
REPORT_ID_CONTROL        = 7   # 控制命令
REPORT_ID_CONFIG_BLOCK1 = 8   # 配置块 1
REPORT_ID_CONFIG_BLOCK2 = 9   # 配置块 2

# 控制命令码
CMD_SAVE_CONFIG   = 0x01
CMD_RESET_CONFIG  = 0x02
CMD_REBOOT        = 0x03
CMD_ENTER_DFU     = 0x04
CMD_APPLY_CONFIG  = 0x05

# 配置大小
CONFIG_SIZE = 146
# 块大小
BLOCK_SIZE = 62


class HidConfigClient:
    """HID 配置通道客户端（分块协议）"""

    def __init__(self, vid=VID, pid=PID):
        self.vid = vid
        self.pid = pid
        self.dev = None

    def open(self):
        """打开设备（找到 Vendor Usage Page 的那个 HID 集合）"""
        try:
            # 先枚举所有设备，找到 Vendor Usage Page (0xFF00) 的那个
            devices = hid.enumerate(self.vid, self.pid)
            print(f"找到 {len(devices)} 个 HID 设备:")
            for i, d in enumerate(devices):
                print(f"  [{i}] path: {d['path'][:50]}...")
                print(f"      usage_page: 0x{d['usage_page']:04X}, usage: 0x{d['usage']:04X}")
                print(f"      product_string: {d.get('product_string', 'N/A')}")

            # 找到 Vendor Usage Page 的设备
            vendor_dev = None
            for d in devices:
                if d['usage_page'] == 0xFF00:
                    vendor_dev = d
                    break

            if not vendor_dev:
                print("\n警告: 找不到 Vendor Usage Page (0xFF00) 的设备")
                print("尝试使用最后一个设备...")
                if devices:
                    vendor_dev = devices[-1]
                else:
                    print("错误: 没有找到任何 HID 设备")
                    return False

            print(f"\n使用设备: {vendor_dev['path'][:50]}...")
            print(f"  usage_page: 0x{vendor_dev['usage_page']:04X}")

            self.dev = hid.device()
            self.dev.open_path(vendor_dev['path'])
            self.dev.set_nonblocking(0)
            return True
        except Exception as e:
            print(f"打开设备失败: {e}")
            print("请确认:")
            print("  1. 设备已连接")
            print("  2. VID/PID 正确")
            print("  3. 没有其他程序占用设备")
            return False

    def close(self):
        """关闭设备"""
        if self.dev:
            self.dev.close()
            self.dev = None

    def read_device_info(self):
        """读取设备信息（Report ID 6）"""
        if not self.dev:
            return None
        try:
            data = self.dev.get_feature_report(REPORT_ID_DEVICE_INFO, 64)
            if data and len(data) >= 7:
                # data[0] 是 report_id
                fw_major = data[1]
                fw_minor = data[2]
                fw_patch = data[3]
                hw_major = data[4]
                hw_minor = data[5]
                cfg_size = data[6] | (data[7] << 8)
                return {
                    'firmware_version': f"{fw_major}.{fw_minor}.{fw_patch}",
                    'hardware_version': f"{hw_major}.{hw_minor}",
                    'config_size': cfg_size,
                    'raw': data
                }
        except Exception as e:
            print(f"读取设备信息失败: {e}")
        return None

    def read_config_block(self, block_id):
        """读取单个配置块"""
        if not self.dev:
            return None
        try:
            data = self.dev.get_feature_report(block_id, 64)
            if data and len(data) >= 1:
                # data[0] 是 report_id，后面是数据
                return bytes(data[1:1+BLOCK_SIZE])
        except Exception as e:
            print(f"读取配置块 {block_id} 失败: {e}")
        return None

    def read_config(self):
        """读取完整配置（分 3 块读取，然后拼接）"""
        if not self.dev:
            return None
        try:
            # 读取 3 个块
            block0 = self.read_config_block(REPORT_ID_CONFIG_BLOCK0)
            block1 = self.read_config_block(REPORT_ID_CONFIG_BLOCK1)
            block2 = self.read_config_block(REPORT_ID_CONFIG_BLOCK2)

            if not block0 or not block1 or not block2:
                return None

            # 拼接
            full_config = block0 + block1 + block2
            # 返回前 CONFIG_SIZE 字节
            return full_config[:CONFIG_SIZE]
        except Exception as e:
            print(f"读取配置失败: {e}")
        return None

    def write_config_block(self, block_id, data):
        """写入单个配置块"""
        if not self.dev:
            return False
        try:
            # 构造报告：[report_id, ...data]
            report = bytearray(BLOCK_SIZE + 1)
            report[0] = block_id
            copy_len = min(len(data), BLOCK_SIZE)
            report[1:1+copy_len] = data[:copy_len]
            self.dev.send_feature_report(report)
            return True
        except Exception as e:
            print(f"写入配置块 {block_id} 失败: {e}")
            return False

    def write_config(self, config_data):
        """写入完整配置（分 3 块写入，然后发送应用命令）"""
        if not self.dev:
            return False
        try:
            # 确保数据长度足够
            data = bytearray(config_data)
            if len(data) < CONFIG_SIZE:
                data.extend(b'\x00' * (CONFIG_SIZE - len(data)))

            # 分 3 块写入
            block0 = data[0:BLOCK_SIZE]
            block1 = data[BLOCK_SIZE:BLOCK_SIZE*2]
            block2 = data[BLOCK_SIZE*2:BLOCK_SIZE*3]

            if not self.write_config_block(REPORT_ID_CONFIG_BLOCK0, block0):
                return False
            if not self.write_config_block(REPORT_ID_CONFIG_BLOCK1, block1):
                return False
            if not self.write_config_block(REPORT_ID_CONFIG_BLOCK2, block2):
                return False

            # 发送应用配置命令
            time.sleep(0.05)
            return self.send_control_cmd(CMD_APPLY_CONFIG)
        except Exception as e:
            print(f"写入配置失败: {e}")
            return False

    def send_control_cmd(self, cmd):
        """发送控制命令（Report ID 7）"""
        if not self.dev:
            return False
        try:
            report = bytearray(2)
            report[0] = REPORT_ID_CONTROL
            report[1] = cmd
            self.dev.send_feature_report(report)
            return True
        except Exception as e:
            print(f"发送控制命令失败: {e}")
            return False

    def parse_config(self, config_data):
        """解析配置数据"""
        if not config_data or len(config_data) < 14:
            return None
        
        magic = struct.unpack('<I', config_data[0:4])[0]
        version = struct.unpack('<H', config_data[4:6])[0]
        dpi = struct.unpack('<H', config_data[6:8])[0]
        deadzone = struct.unpack('<H', config_data[8:10])[0]
        encoder_rev = config_data[10]
        seq = struct.unpack('<H', config_data[11:13])[0]
        reserved = config_data[13]
        crc = struct.unpack('<I', config_data[142:146])[0]
        
        return {
            'magic': magic,
            'version': version,
            'dpi': dpi,
            'joystick_deadzone': deadzone,
            'encoder_reverse': encoder_rev,
            'seq': seq,
            'reserved': reserved,
            'crc32': crc,
            'keymap': config_data[14:78],
            'fn_keymap': config_data[78:142],
        }


def main():
    print("=" * 60)
    print("  USB HID 配置通道测试工具（分块协议版）")
    print("=" * 60)
    print()

    client = HidConfigClient()

    print(f"尝试打开设备 VID=0x{VID:04X}, PID=0x{PID:04X}...")
    if not client.open():
        return 1

    print("设备已连接!")
    print()

    # 测试 0: 最简单的控制报告读取
    print("-" * 60)
    print("[测试 0] 读取控制状态 (Report ID 7, 最简单)")
    try:
        data = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        data_bytes = bytes(data)
        print(f"  成功! 读到 {len(data)} 字节: {data_bytes.hex()}")
        print("  ✓ 通过")
    except Exception as e:
        print(f"  失败: {e}")
    print()

    # 测试 0.5: 最小化测试 - 只写控制命令，然后立即读取控制状态
    print("-" * 60)
    print("[测试 0.5] 最小化测试：写控制命令后立即读控制状态")
    try:
        # 先读一次，确认正常
        data1 = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        print(f"  写入前读取: 成功，{len(data1)} 字节: {bytes(data1).hex()}")

        # 写一个控制命令（CMD_APPLY_CONFIG = 5，应该是安全的）
        report = bytearray(2)
        report[0] = REPORT_ID_CONTROL
        report[1] = 0x05  # CMD_APPLY_CONFIG
        client.dev.send_feature_report(report)
        print(f"  写入控制命令: 成功")

        # 立即读回来
        time.sleep(0.01)
        data2 = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        print(f"  写入后读取: 成功，{len(data2)} 字节: {bytes(data2).hex()}")
        print("  ✓ 通过 (写入后仍能读取)")
    except Exception as e:
        print(f"  ✗ 失败: {e}")
        print("  结论：只要有 set_report，后续 get_report 就会失败")
    print()

    # 测试 0.7: 写设备信息报告(32字节)后，读控制状态（测试中等长度数据）
    print("-" * 60)
    print("[测试 0.7] 写设备信息报告(32字节)后，读控制状态")
    try:
        # 先读控制状态，确认正常
        data1 = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        print(f"  写入前读控制状态: 成功，{len(data1)} 字节")

        # 写设备信息报告（32字节数据）
        report = bytearray(33)  # 1字节Report ID + 32字节数据
        report[0] = REPORT_ID_DEVICE_INFO
        # 数据部分全0
        client.dev.send_feature_report(report)
        print(f"  写入设备信息报告(32字节): 成功")

        # 立即读控制状态
        time.sleep(0.01)
        data2 = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        print(f"  写入后读控制状态: 成功，{len(data2)} 字节")
        print("  ✓ 32字节写入后仍能读取")
    except Exception as e:
        print(f"  ✗ 失败: {e}")
        print("  结论：写32字节也会导致后续读取失败，问题和数据长度有关")
    print()

    # 测试 0.6: 写配置块0后，读控制状态
    print("-" * 60)
    print("[测试 0.6] 写配置块0后，读控制状态")
    try:
        # 先读控制状态，确认正常
        data1 = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        print(f"  写入前读控制状态: 成功，{len(data1)} 字节")

        # 写配置块0（全0数据，63字节）
        report = bytearray(64)  # 1字节Report ID + 63字节数据
        report[0] = REPORT_ID_CONFIG_BLOCK0
        # 数据部分全0
        client.dev.send_feature_report(report)
        print(f"  写入配置块0: 成功")

        # 立即读控制状态
        time.sleep(0.01)
        data2 = client.dev.get_feature_report(REPORT_ID_CONTROL, 4)
        print(f"  写入后读控制状态: 成功，{len(data2)} 字节")
        print("  ✓ 其他 Report ID 仍能读取")

        # 再试试读配置块0
        time.sleep(0.01)
        try:
            data3 = client.dev.get_feature_report(REPORT_ID_CONFIG_BLOCK0, 64)
            print(f"  写入后读配置块0: 成功，{len(data3)} 字节")
            print("  ✓ 配置块0 也能读取")
        except Exception as e2:
            print(f"  写入后读配置块0: 失败 - {e2}")
            print("  结论：写配置块后，配置块本身的读取失败，但其他 Report ID 正常")
    except Exception as e:
        print(f"  ✗ 失败: {e}")
    print()

    # 测试 0.8: 只写3个配置块，不发送应用命令，看看会不会导致读取失败
    print("-" * 60)
    print("[测试 0.8] 只写3个配置块，不发应用命令")
    try:
        # 先读一次，确认正常
        data1 = client.read_config()
        print(f"  写入前读取配置: 成功，{len(data1)} 字节")

        # 构造测试数据（修改DPI为 1600）
        test_data = bytearray(data1)
        test_data[6] = 0x40  # 1600 = 0x0640
        test_data[7] = 0x06

        # 只写3个块，不发送应用命令
        block0 = test_data[0:62]
        block1 = test_data[62:124]
        block2 = test_data[124:186]

        client.write_config_block(REPORT_ID_CONFIG_BLOCK0, block0)
        print(f"  写块0: 成功")
        client.write_config_block(REPORT_ID_CONFIG_BLOCK1, block1)
        print(f"  写块1: 成功")
        client.write_config_block(REPORT_ID_CONFIG_BLOCK2, block2)
        print(f"  写块2: 成功")

        # 立即读取
        time.sleep(0.01)
        data2 = client.read_config()
        if data2:
            print(f"  写入后读取配置: 成功，{len(data2)} 字节")
            print("  ✓ 只写3个块不会导致读取失败")
        else:
            print(f"  写入后读取配置: 失败")
            print("  结论：写3个配置块本身就会导致读取失败")
    except Exception as e:
        print(f"  ✗ 失败: {e}")
    print()

    # 测试 1: 读取设备信息
    print("-" * 60)
    print("[测试 1] 读取设备信息 (Report ID 6)")
    info = client.read_device_info()
    if info:
        print(f"  固件版本: {info['firmware_version']}")
        print(f"  硬件版本: {info['hardware_version']}")
        print(f"  配置大小: {info['config_size']} 字节")
        print("  ✓ 通过")
    else:
        print("  ✗ 失败")
    print()

    # 测试 2: 读取配置（分块）
    print("-" * 60)
    print("[测试 2] 读取配置（分 3 块读取）")
    config_data = client.read_config()
    if config_data:
        print(f"  读取到 {len(config_data)} 字节配置数据")
        cfg = client.parse_config(config_data)
        if cfg:
            print(f"  魔数:     0x{cfg['magic']:08X} {'(正确)' if cfg['magic'] == 0x5A5A5A5A else '(错误!)'}")
            print(f"  版本:     0x{cfg['version']:04X}")
            print(f"  序列号:   {cfg['seq']}")
            print(f"  DPI:      {cfg['dpi']}")
            print(f"  摇杆死区: {cfg['joystick_deadzone']}")
            print(f"  编码器方向: {'反转' if cfg['encoder_reverse'] else '正常'}")
            print(f"  CRC32:    0x{cfg['crc32']:08X}")
            print("  ✓ 通过")
        else:
            print("  ✗ 解析失败")
    else:
        print("  ✗ 读取失败")
    print()

    # 测试 3: 写入测试（修改 DPI，然后读回来验证）
    print("-" * 60)
    print("[测试 3] 写入配置测试 (修改 DPI 为 2400)")
    if config_data and len(config_data) >= 8:
        # 修改 DPI 字段（偏移 6-7，小端）
        test_data = bytearray(config_data)
        test_data[6] = 0xE0  # 2400 = 0x0960
        test_data[7] = 0x09
        
        if client.write_config(test_data):
            print("  写入成功")
            # 等一下再读回来（写Flash需要时间）
            time.sleep(1.0)
            verify_data = client.read_config()
            if verify_data:
                verify_dpi = struct.unpack('<H', verify_data[6:8])[0]
                print(f"  读回验证: DPI = {verify_dpi}")
                if verify_dpi == 2400:
                    print("  ✓ 通过")
                else:
                    print("  ✗ 验证失败 (DPI 不匹配)")
            else:
                print("  ✗ 读回失败")
                print("  尝试重新打开设备...")
                time.sleep(0.5)
                client2 = HidConfigClient()
                if client2.open():
                    print("  重新打开成功，再次读取...")
                    verify_data2 = client2.read_config()
                    if verify_data2:
                        verify_dpi2 = struct.unpack('<H', verify_data2[6:8])[0]
                        print(f"  重新打开后读回: DPI = {verify_dpi2}")
                        print("  结论：设备正常，只是写Flash时超时导致句柄失效")
                    else:
                        print("  重新打开后还是读不到")
                        print("  结论：设备可能已经挂了")
                else:
                    print("  重新打开失败")
                    print("  结论：设备已经断开，需要重新插拔")
        else:
            print("  ✗ 写入失败")
    else:
        print("  跳过 (没有原始配置数据)")
    print()

    # 测试 4: 控制命令测试（恢复默认配置）
    print("-" * 60)
    print("[测试 4] 控制命令测试 (恢复默认配置)")
    if client.send_control_cmd(CMD_RESET_CONFIG):
        print("  命令发送成功")
        time.sleep(0.2)
        # 读取验证
        reset_data = client.read_config()
        if reset_data:
            reset_dpi = struct.unpack('<H', reset_data[6:8])[0]
            print(f"  恢复后 DPI: {reset_dpi}")
            print("  ✓ 通过")
        else:
            print("  读取验证失败")
    else:
        print("  ✗ 命令发送失败")
    print()

    print("=" * 60)
    print("  测试完成!")
    print("=" * 60)

    client.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
