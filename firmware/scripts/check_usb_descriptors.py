#!/usr/bin/env python3
"""
USB 描述符合规性检查工具

检查 firmware/src/protocol/usb_descriptors.c 中的 USB 描述符：
- 设备描述符（VID/PID、版本、字符串索引）
- HID 报表描述符（长度、Report ID、集合结构）
- 配置描述符（接口数、端点配置）
- 字符串描述符（厂商、产品、序列号）

用法:
    python check_usb_descriptors.py [--file <path>] [--json] [--strict]

退出码:
    0 - 所有检查通过
    1 - 存在错误
    2 - 存在警告（仅在 --strict 模式下返回 1）
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


# ============================================================
# 数据结构
# ============================================================

@dataclass
class CheckResult:
    """检查结果"""
    level: str  # "error", "warning", "info"
    category: str
    message: str
    suggestion: Optional[str] = None

    def __str__(self):
        icon = {"error": "❌", "warning": "⚠️ ", "info": "ℹ️ "}.get(self.level, "?")
        return f"{icon} [{self.category}] {self.message}"


@dataclass
class USBChecker:
    """USB 描述符合规性检查器"""
    file_path: str
    strict: bool = False
    results: List[CheckResult] = field(default_factory=list)

    # 解析后的数据
    device_desc: dict = field(default_factory=dict)
    hid_reports: List[dict] = field(default_factory=list)
    config_desc: dict = field(default_factory=dict)
    string_descs: dict = field(default_factory=dict)

    def add_error(self, category: str, message: str, suggestion: str = None):
        self.results.append(CheckResult("error", category, message, suggestion))

    def add_warning(self, category: str, message: str, suggestion: str = None):
        self.results.append(CheckResult("warning", category, message, suggestion))

    def add_info(self, category: str, message: str):
        self.results.append(CheckResult("info", category, message))

    @property
    def errors(self) -> List[CheckResult]:
        return [r for r in self.results if r.level == "error"]

    @property
    def warnings(self) -> List[CheckResult]:
        return [r for r in self.results if r.level == "warning"]

    # ============================================================
    # 解析 C 文件
    # ============================================================

    def parse(self) -> bool:
        """解析 usb_descriptors.c 文件"""
        try:
            with open(self.file_path, "r", encoding="utf-8") as f:
                content = f.read()
        except FileNotFoundError:
            self.add_error("文件", f"文件不存在: {self.file_path}")
            return False
        except Exception as e:
            self.add_error("文件", f"读取文件失败: {e}")
            return False

        # 解析各部分
        self._parse_device_descriptor(content)
        self._parse_hid_report_descriptors(content)
        self._parse_configuration_descriptor(content)
        self._parse_string_descriptors(content)

        return True

    def _parse_device_descriptor(self, content: str):
        """解析设备描述符"""
        # 提取 #define USB_VID, USB_PID, USB_BCD
        vid_match = re.search(r"#define\s+USB_VID\s+0x([0-9A-Fa-f]+)", content)
        pid_match = re.search(r"#define\s+USB_PID\s+0x([0-9A-Fa-f]+)", content)
        bcd_match = re.search(r"#define\s+USB_BCD\s+0x([0-9A-Fa-f]+)", content)

        if vid_match:
            self.device_desc["vid"] = int(vid_match.group(1), 16)
        if pid_match:
            self.device_desc["pid"] = int(pid_match.group(1), 16)
        if bcd_match:
            self.device_desc["bcd_usb"] = int(bcd_match.group(1), 16)

        # 提取 bcdDevice
        bcd_device_match = re.search(r"\.bcdDevice\s*=\s*0x([0-9A-Fa-f]+)", content)
        if bcd_device_match:
            self.device_desc["bcd_device"] = int(bcd_device_match.group(1), 16)

        # 提取字符串索引
        for field_name in ["iManufacturer", "iProduct", "iSerialNumber"]:
            match = re.search(rf"\.{field_name}\s*=\s*0x([0-9A-Fa-f]+)", content)
            if match:
                self.device_desc[field_name] = int(match.group(1), 16)

        # 提取 bNumConfigurations
        config_match = re.search(r"\.bNumConfigurations\s*=\s*0x([0-9A-Fa-f]+)", content)
        if config_match:
            self.device_desc["bNumConfigurations"] = int(config_match.group(1), 16)

    def _parse_hid_report_descriptors(self, content: str):
        """解析 HID 报表描述符"""
        # 已知的非 HID 报表描述符名称
        non_hid_names = {
            "desc_configuration",
            "desc_device",
            "desc_other_speed_config",
            "desc_device_qualifier",
        }

        # 提取所有 uint8_t const desc_*_report[] = {...};
        # 注意：只匹配以 _report 结尾的变量名
        pattern = r"uint8_t\s+const\s+(desc_\w+_report)\[\]\s*=\s*\{([^}]+)\};"
        matches = re.findall(pattern, content, re.DOTALL)

        for name, body in matches:
            if name in non_hid_names:
                continue

            # 解析字节数组
            bytes_list = self._parse_byte_array(body)

            # 尝试展开 TinyUSB 宏
            macro_bytes = self._expand_tinyusb_macros(body)
            if macro_bytes:
                bytes_list = macro_bytes + bytes_list

            if bytes_list:
                report_ids = self._extract_report_ids(bytes_list)
                self.hid_reports.append({
                    "name": name,
                    "length": len(bytes_list),
                    "bytes": bytes_list,
                    "report_ids": report_ids,
                    "has_macros": bool(macro_bytes),
                })

    def _parse_byte_array(self, body: str) -> List[int]:
        """解析 C 字节数组"""
        bytes_list = []

        # 已知的 Report ID 宏常量映射
        report_id_macros = {
            # 应用程序 Report ID
            "REPORT_ID_KEYBOARD": 1,
            "REPORT_ID_MOUSE": 2,
            "REPORT_ID_CONSUMER_CONTROL": 3,
            "REPORT_ID_GAMEPAD": 4,
            "REPORT_ID_NKRO": 22,
            # Bootloader DFU Report ID
            "DFU_REPORT_ID_STATUS": 0x30,
            "DFU_REPORT_ID_ERASE": 0x31,
            "DFU_REPORT_ID_DATA": 0x32,
            "DFU_REPORT_ID_VERIFY": 0x33,
            "DFU_REPORT_ID_FINISH": 0x34,
            "DFU_REPORT_ID_ABORT": 0x35,
        }

        # 先移除注释
        body = re.sub(r"//[^\n]*", "", body)
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)

        # 按逗号分割
        tokens = [t.strip() for t in body.split(",") if t.strip()]

        for token in tokens:
            # 十六进制
            hex_match = re.match(r"0x([0-9A-Fa-f]+)", token)
            if hex_match:
                bytes_list.append(int(hex_match.group(1), 16))
                continue

            # 十进制
            dec_match = re.match(r"^(\d+)$", token)
            if dec_match:
                bytes_list.append(int(dec_match.group(1)))
                continue

            # 宏常量（如 REPORT_ID_NKRO）
            if token in report_id_macros:
                bytes_list.append(report_id_macros[token])
                continue

            # 宏调用（如 TUD_HID_REPORT_DESC_KEYBOARD(...)）
            if "(" in token or token.startswith("TUD_") or token.startswith("HID_"):
                # 这是一个宏，跳过
                continue

            # 其他标识符
            if re.match(r"^[A-Z_][A-Z0-9_]*$", token):
                # 未知的宏常量，跳过
                continue

        return bytes_list

    def _expand_tinyusb_macros(self, body: str) -> List[int]:
        """尝试展开 TinyUSB HID 报表描述符宏

        TinyUSB 的宏定义（参考 tusb_hid.h）：
        - TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(id))
          = Usage Page (Generic Desktop), Usage (Keyboard), Collection (Application),
            Report ID (id), ... (标准键盘描述符)

        - TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(id))
          = Usage Page (Generic Desktop), Usage (Mouse), Collection (Application),
            Report ID (id), ... (标准鼠标描述符)

        - TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(id))
          = Usage Page (Consumer), Usage (Consumer Control), Collection (Application),
            Report ID (id), ... (消费者控制描述符)

        - TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(id))
          = Usage Page (Generic Desktop), Usage (Game Pad), Collection (Application),
            Report ID (id), ... (游戏手柄描述符)

        由于完整展开很复杂，我们只提取 Report ID 并记录宏的存在。
        """
        bytes_list = []

        # 移除注释
        body = re.sub(r"//[^\n]*", "", body)
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)

        # 查找 HID_REPORT_ID(id) 宏调用
        # 格式: HID_REPORT_ID(REPORT_ID_KEYBOARD) 或 HID_REPORT_ID(1)
        report_id_pattern = r"HID_REPORT_ID\s*\(\s*([^)]+)\s*\)"
        matches = re.findall(report_id_pattern, body)

        for match in matches:
            match = match.strip()
            # 尝试解析为数字
            if match.isdigit():
                rid = int(match)
            else:
                # 尝试解析为十六进制
                hex_match = re.match(r"0x([0-9A-Fa-f]+)", match)
                if hex_match:
                    rid = int(hex_match.group(1), 16)
                else:
                    # 这是一个宏常量（如 REPORT_ID_KEYBOARD）
                    # 我们无法直接解析，但可以记录
                    # 尝试从常见的 Report ID 宏映射
                    rid_map = {
                        "REPORT_ID_KEYBOARD": 1,
                        "REPORT_ID_MOUSE": 2,
                        "REPORT_ID_CONSUMER_CONTROL": 3,
                        "REPORT_ID_GAMEPAD": 4,
                        "REPORT_ID_NKRO": 22,
                    }
                    rid = rid_map.get(match, None)

            if rid is not None:
                # 构造一个简化的 Report ID 项: 0x85, rid
                bytes_list.append(0x85)
                bytes_list.append(rid)

        return bytes_list

    def _extract_report_ids(self, bytes_list: List[int]) -> List[int]:
        """从 HID 报表描述符中提取 Report ID"""
        report_ids = []
        i = 0
        while i < len(bytes_list):
            tag = bytes_list[i]
            # Report ID tag: 0x85
            if tag == 0x85 and i + 1 < len(bytes_list):
                report_ids.append(bytes_list[i + 1])
                i += 2
            else:
                # 简单的项类型解析
                # bType = (tag >> 2) & 0x03
                # bSize = tag & 0x03
                b_size = tag & 0x03
                if b_size == 3:
                    b_size = 4
                i += 1 + b_size

        return report_ids

    def _parse_configuration_descriptor(self, content: str):
        """解析配置描述符"""
        # 提取 CONFIG_TOTAL_LEN
        total_len_match = re.search(r"#define\s+CONFIG_TOTAL_LEN\s+\(([^)]+)\)", content)
        if total_len_match:
            self.config_desc["total_len_expr"] = total_len_match.group(1)

        # 提取端点号
        ep_matches = re.findall(r"#define\s+(EPNUM_\w+)\s+0x([0-9A-Fa-f]+)", content)
        for name, value in ep_matches:
            self.config_desc.setdefault("endpoints", {})[name] = int(value, 16)

        # 提取 TUD_HID_DESCRIPTOR 调用次数（接口数）
        hid_desc_count = len(re.findall(r"TUD_HID_DESCRIPTOR\(", content))
        self.config_desc["hid_interface_count"] = hid_desc_count

        # 提取 ITF_NUM_TOTAL
        itf_match = re.search(r"ITF_NUM_TOTAL", content)
        if itf_match:
            self.config_desc["has_itf_total"] = True

    def _parse_string_descriptors(self, content: str):
        """解析字符串描述符"""
        # 提取 string_desc_arr 数组
        arr_match = re.search(
            r"char\s+const\s*\*\s*string_desc_arr\s*\[\s*\]\s*=\s*\{(.*?)\};",
            content,
            re.DOTALL,
        )
        if arr_match:
            body = arr_match.group(1)
            # 移除注释
            body = re.sub(r"//[^\n]*", "", body)
            body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)

            # 按逗号分割数组元素（注意：需要处理嵌套的花括号）
            elements = []
            depth = 0
            current = ""
            for char in body:
                if char == "{":
                    depth += 1
                    current += char
                elif char == "}":
                    depth -= 1
                    current += char
                elif char == "," and depth == 0:
                    elements.append(current.strip())
                    current = ""
                else:
                    current += char
            if current.strip():
                elements.append(current.strip())

            # 解析每个元素
            for i, elem in enumerate(elements):
                # 检查是否是语言 ID (const char[]) { 0x09, 0x04 }
                lang_match = re.search(r"\{\s*0x([0-9A-Fa-f]+)\s*,\s*0x([0-9A-Fa-f]+)\s*\}", elem)
                if lang_match:
                    lang_byte1 = int(lang_match.group(1), 16)
                    lang_byte2 = int(lang_match.group(2), 16)
                    # 语言 ID 是小端序
                    lang_id = lang_byte1 | (lang_byte2 << 8)
                    self.string_descs[i] = f"LANGID:0x{lang_id:04X}"
                    self.string_descs["lang_id"] = lang_id
                    continue

                # 检查是否是 NULL
                if re.match(r"^\s*NULL\s*$", elem, re.IGNORECASE):
                    self.string_descs[i] = None
                    self.string_descs["has_null_serial"] = True
                    continue

                # 普通字符串
                str_match = re.search(r'"([^"]*)"', elem)
                if str_match:
                    self.string_descs[i] = str_match.group(1)

    # ============================================================
    # 检查规则
    # ============================================================

    def check_all(self):
        """执行所有检查"""
        self.check_device_descriptor()
        self.check_hid_report_descriptors()
        self.check_configuration_descriptor()
        self.check_string_descriptors()
        self.check_vid_pid()

    def check_device_descriptor(self):
        """检查设备描述符"""
        category = "设备描述符"

        if not self.device_desc:
            self.add_error(category, "未找到设备描述符")
            return

        # VID/PID
        vid = self.device_desc.get("vid")
        pid = self.device_desc.get("pid")
        if vid is None:
            self.add_error(category, "未定义 USB_VID")
        if pid is None:
            self.add_error(category, "未定义 USB_PID")

        # bcdUSB
        bcd_usb = self.device_desc.get("bcd_usb")
        if bcd_usb is not None:
            if bcd_usb < 0x0110:
                self.add_warning(category, f"USB 版本过低: 0x{bcd_usb:04X} (建议至少 0x0200)")
            elif bcd_usb == 0x0200:
                self.add_info(category, "USB 2.0 全速设备")
            elif bcd_usb == 0x0300:
                self.add_info(category, "USB 3.0 设备")

        # bcdDevice
        bcd_device = self.device_desc.get("bcd_device")
        if bcd_device is not None:
            major = (bcd_device >> 8) & 0xFF
            minor = bcd_device & 0xFF
            self.add_info(category, f"固件版本: v{major}.{minor} (bcdDevice=0x{bcd_device:04X})")

        # 字符串索引
        for field_name, label in [
            ("iManufacturer", "厂商字符串"),
            ("iProduct", "产品字符串"),
            ("iSerialNumber", "序列号字符串"),
        ]:
            idx = self.device_desc.get(field_name)
            if idx is None or idx == 0:
                self.add_warning(category, f"{label}索引为 0（未设置）")
            else:
                self.add_info(category, f"{label}索引: {idx}")

        # 配置数
        num_configs = self.device_desc.get("bNumConfigurations")
        if num_configs is not None and num_configs == 0:
            self.add_error(category, "bNumConfigurations 为 0")

    def check_hid_report_descriptors(self):
        """检查 HID 报表描述符"""
        category = "HID 报表描述符"

        if not self.hid_reports:
            self.add_error(category, "未找到 HID 报表描述符")
            return

        for report in self.hid_reports:
            name = report["name"]
            length = report["length"]
            report_ids = report["report_ids"]

            self.add_info(category, f"{name}: {length} 字节, {len(report_ids)} 个 Report ID")

            # 长度检查
            if length == 0:
                self.add_error(category, f"{name}: 描述符长度为 0")
            elif length < 10:
                self.add_warning(category, f"{name}: 描述符过短 ({length} 字节)，可能不完整")

            # Report ID 检查
            if report_ids:
                # 检查重复
                seen = set()
                duplicates = set()
                for rid in report_ids:
                    if rid in seen:
                        duplicates.add(rid)
                    seen.add(rid)

                if duplicates:
                    self.add_error(
                        category,
                        f"{name}: 重复的 Report ID: {sorted(duplicates)}",
                        "每个 Report ID 在一个描述符中应该唯一",
                    )

                # 检查 Report ID 范围
                for rid in report_ids:
                    if rid == 0:
                        self.add_warning(category, f"{name}: Report ID 0 不推荐使用（无 Report ID 模式）")
                    elif rid > 255:
                        self.add_error(category, f"{name}: 无效的 Report ID: {rid} (必须 1-255)")

                self.add_info(category, f"{name}: Report IDs = {sorted(report_ids)}")
            else:
                self.add_warning(category, f"{name}: 未找到 Report ID（可能使用了宏定义）")

            # 集合结构检查（简化版）
            bytes_list = report["bytes"]
            if bytes_list:
                # 检查 Collection (0xA1) 和 End Collection (0xC0) 匹配
                collections = 0
                end_collections = 0
                for b in bytes_list:
                    if b == 0xA1:
                        collections += 1
                    elif b == 0xC0:
                        end_collections += 1

                if collections != end_collections:
                    self.add_error(
                        category,
                        f"{name}: Collection/End Collection 不匹配 ({collections} 个 Collection, {end_collections} 个 End Collection)",
                        "每个 Collection (0xA1) 必须有对应的 End Collection (0xC0)",
                    )

    def check_configuration_descriptor(self):
        """检查配置描述符"""
        category = "配置描述符"

        if not self.config_desc:
            self.add_error(category, "未找到配置描述符")
            return

        # 接口数
        hid_count = self.config_desc.get("hid_interface_count", 0)
        if hid_count == 0:
            self.add_error(category, "未找到 HID 接口描述符")
        else:
            self.add_info(category, f"HID 接口数: {hid_count}")

        # 端点检查
        endpoints = self.config_desc.get("endpoints", {})
        if endpoints:
            for name, addr in endpoints.items():
                direction = "IN" if addr & 0x80 else "OUT"
                ep_num = addr & 0x7F
                self.add_info(category, f"{name}: 0x{addr:02X} (EP{ep_num} {direction})")

                # 检查端点号范围
                if ep_num == 0:
                    self.add_error(category, f"{name}: 端点号不能为 0（端点 0 是控制端点）")
                elif ep_num > 15:
                    self.add_warning(category, f"{name}: 端点号 {ep_num} 较大（全速设备最多 16 个端点）")

            # 检查端点地址重复
            addrs = list(endpoints.values())
            if len(addrs) != len(set(addrs)):
                self.add_error(category, "存在重复的端点地址")
        else:
            self.add_warning(category, "未找到端点定义")

    def check_string_descriptors(self):
        """检查字符串描述符"""
        category = "字符串描述符"

        if not self.string_descs:
            self.add_error(category, "未找到字符串描述符数组")
            return

        # 检查语言 ID
        lang_id = self.string_descs.get("lang_id")
        if lang_id is not None:
            if lang_id == 0x0409:
                self.add_info(category, "语言 ID: 0x0409 (英语 - 美国)")
            else:
                self.add_info(category, f"语言 ID: 0x{lang_id:04X}")
        else:
            self.add_warning(category, "未设置语言 ID")

        # 检查厂商字符串（索引 1）
        manufacturer = self.string_descs.get(1)
        if manufacturer is not None and not isinstance(manufacturer, str) or (isinstance(manufacturer, str) and manufacturer.startswith("LANGID")):
            manufacturer = None

        if manufacturer:
            if manufacturer in ["Your Company", "TODO", "", "Manufacturer"]:
                self.add_warning(
                    category,
                    f"厂商字符串为占位符: \"{manufacturer}\"",
                    "建议替换为实际品牌名称",
                )
            else:
                self.add_info(category, f"厂商: \"{manufacturer}\"")
        else:
            self.add_warning(category, "未设置厂商字符串")

        # 检查产品字符串（索引 2）
        product = self.string_descs.get(2)
        if product:
            if product in ["Your Product", "TODO", "", "Product"]:
                self.add_warning(
                    category,
                    f"产品字符串为占位符: \"{product}\"",
                    "建议替换为实际产品名称",
                )
            else:
                self.add_info(category, f"产品: \"{product}\"")
        else:
            self.add_warning(category, "未设置产品字符串")

        # 检查序列号（索引 3）
        has_null_serial = self.string_descs.get("has_null_serial", False)
        if has_null_serial:
            self.add_info(category, "序列号: 使用芯片唯一 ID (动态生成)")
        else:
            serial = self.string_descs.get(3)
            if serial:
                self.add_info(category, f"序列号: \"{serial}\"")
            else:
                self.add_warning(category, "未设置序列号字符串")

    def check_vid_pid(self):
        """检查 VID/PID 合规性"""
        category = "VID/PID"

        vid = self.device_desc.get("vid")
        pid = self.device_desc.get("pid")

        if vid is None or pid is None:
            return

        # 已知的保留/测试 VID
        reserved_vids = {
            0x0000: "未定义",
            0xFFFF: "测试 VID",
            0x1234: "测试 VID (常见示例)",
            0xDEAD: "测试 VID",
            0xBEEF: "测试 VID",
        }

        # Raspberry Pi VID
        raspberry_pi_vid = 0x2E8A

        # pid.codes VID
        pid_codes_vid = 0x1209

        if vid in reserved_vids:
            self.add_error(
                category,
                f"VID 0x{vid:04X} 是保留/测试 VID ({reserved_vids[vid]})",
                "请使用合法的 VID，如 pid.codes (0x1209) 或购买 USB-IF VID",
            )
        elif vid == raspberry_pi_vid:
            self.add_warning(
                category,
                f"VID 0x{vid:04X} 是 Raspberry Pi 官方 VID",
                "商业产品不应使用 Raspberry Pi VID，建议使用 pid.codes 或购买独立 VID",
            )
        elif vid == pid_codes_vid:
            self.add_info(category, f"VID 0x{vid:04X} 是 pid.codes VID (开源项目可用)")
            # 检查 PID 是否在 pid.codes 已分配范围内
            if pid < 0x1000:
                self.add_warning(
                    category,
                    f"PID 0x{pid:04X} 可能未在 pid.codes 注册",
                    "请到 https://pid.codes/ 申请正式 PID",
                )
        else:
            self.add_info(category, f"VID 0x{vid:04X} (自定义 VID)")

        # PID 检查
        if pid == 0x0000:
            self.add_error(category, "PID 不能为 0x0000")
        elif pid == 0xFFFF:
            self.add_warning(category, "PID 0xFFFF 是测试值，不建议用于生产")

    # ============================================================
    # 输出
    # ============================================================

    def print_report(self):
        """打印检查报告"""
        print("=" * 70)
        print("  USB 描述符合规性检查报告")
        print("=" * 70)
        print(f"  文件: {self.file_path}")
        print(f"  模式: {'严格' if self.strict else '常规'}")
        print("-" * 70)

        if not self.results:
            print("  无检查结果")
            return

        # 按类别分组
        categories = {}
        for r in self.results:
            categories.setdefault(r.category, []).append(r)

        for category, results in categories.items():
            print(f"\n  【{category}】")
            for r in results:
                print(f"    {r}")
                if r.suggestion:
                    print(f"       💡 建议: {r.suggestion}")

        # 汇总
        print("\n" + "=" * 70)
        print("  汇总")
        print("=" * 70)
        print(f"  ❌ 错误: {len(self.errors)}")
        print(f"  ⚠️  警告: {len(self.warnings)}")
        print(f"  ℹ️  信息: {len([r for r in self.results if r.level == 'info'])}")

        if self.errors:
            print(f"\n  ❌ 检查失败，存在 {len(self.errors)} 个错误")
        elif self.warnings and self.strict:
            print(f"\n  ⚠️  严格模式下检查失败，存在 {len(self.warnings)} 个警告")
        else:
            print("\n  ✅ 检查通过")

        print("=" * 70)

    def to_json(self) -> dict:
        """转换为 JSON 格式"""
        return {
            "file": self.file_path,
            "strict": self.strict,
            "summary": {
                "errors": len(self.errors),
                "warnings": len(self.warnings),
                "info": len([r for r in self.results if r.level == "info"]),
                "total": len(self.results),
            },
            "device_descriptor": self.device_desc,
            "hid_reports": [
                {
                    "name": r["name"],
                    "length": r["length"],
                    "report_ids": r["report_ids"],
                }
                for r in self.hid_reports
            ],
            "configuration_descriptor": self.config_desc,
            "string_descriptors": {
                k: v for k, v in self.string_descs.items() if isinstance(k, int)
            },
            "results": [
                {
                    "level": r.level,
                    "category": r.category,
                    "message": r.message,
                    "suggestion": r.suggestion,
                }
                for r in self.results
            ],
        }

    def get_exit_code(self) -> int:
        """获取退出码"""
        if self.errors:
            return 1
        if self.warnings and self.strict:
            return 1
        return 0


# ============================================================
# 主函数
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="USB 描述符合规性检查工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python check_usb_descriptors.py
  python check_usb_descriptors.py --file ../src/protocol/usb_descriptors.c
  python check_usb_descriptors.py --json --strict
  python check_usb_descriptors.py --json > report.json
        """,
    )

    parser.add_argument(
        "--file",
        "-f",
        default="../src/protocol/usb_descriptors.c",
        help="usb_descriptors.c 文件路径 (默认: ../src/protocol/usb_descriptors.c)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="以 JSON 格式输出",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="严格模式（警告也会导致失败）",
    )

    args = parser.parse_args()

    # 解析文件路径（相对于脚本所在目录）
    import os

    script_dir = os.path.dirname(os.path.abspath(__file__))
    file_path = args.file
    if not os.path.isabs(file_path):
        file_path = os.path.join(script_dir, file_path)
    file_path = os.path.normpath(file_path)

    # 创建检查器
    checker = USBChecker(file_path=file_path, strict=args.strict)

    # 解析并检查
    if not checker.parse():
        if args.json:
            print(json.dumps(checker.to_json(), indent=2, ensure_ascii=False))
        else:
            checker.print_report()
        sys.exit(1)

    checker.check_all()

    # 输出
    if args.json:
        print(json.dumps(checker.to_json(), indent=2, ensure_ascii=False))
    else:
        checker.print_report()

    sys.exit(checker.get_exit_code())


if __name__ == "__main__":
    main()
