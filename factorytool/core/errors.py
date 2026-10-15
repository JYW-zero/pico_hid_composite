"""错误码定义。

设备/HID 返回码（0~12、0xFF）与固件 ``config_hid_error_t``、上位机
``ProtocolConstants`` / ``HidErrorMapper`` 严格对齐，未来接真实设备时直接透传，
MockDevice 也用同一套数值模拟。

工具/流程级返回码（100+）只存在于 FactoryTool 编排层（扫码、型号比对、SN 规则、
自检判定等），不与固件码冲突，便于日后夹具脚本 / MES 区分"设备拒绝"与"工装判定"。
"""

# ---- 设备 / HID 返回码（对齐固件 config_hid_error_t，勿随意改值）----
OK = 0
INVALID_CMD = 1
INVALID_PARAM = 2
CONFIG_LOCKED = 3
CRC_MISMATCH = 4
INCOMPLETE = 5
FLASH_WRITE = 6          # 写 Flash / 烧录失败（写 SN 失败用此码）
TIMEOUT = 7
BUSY = 8
NOT_SUPPORTED = 9
VERSION_MISMATCH = 10
DIAG_LOCKED = 11
MFG_LOCKED = 12          # 已出厂锁定，制造命令被拒
UNKNOWN = 0xFF

# ---- FactoryTool 工具 / 流程级返回码（100+）----
SCAN_FAIL = 100          # 未扫到码 / 条码不可读
MODEL_MISMATCH = 101     # 识别到的型号与工单配置不符
SN_RULE_FAIL = 102       # 序列号不符合规则（前缀 / 长度 / 字符集）
SELF_TEST_FAIL = 103     # 某项自检未通过
DEVICE_NOT_CONNECTED = 104
STEP_EXCEPTION = 105     # 步骤抛出未预期异常
CONFIG_ERROR = 106       # 工装配置文件错误

_DEVICE_CODE_NAMES = {
    OK: "OK", INVALID_CMD: "INVALID_CMD", INVALID_PARAM: "INVALID_PARAM",
    CONFIG_LOCKED: "CONFIG_LOCKED", CRC_MISMATCH: "CRC_MISMATCH",
    INCOMPLETE: "INCOMPLETE", FLASH_WRITE: "FLASH_WRITE", TIMEOUT: "TIMEOUT",
    BUSY: "BUSY", NOT_SUPPORTED: "NOT_SUPPORTED", VERSION_MISMATCH: "VERSION_MISMATCH",
    DIAG_LOCKED: "DIAG_LOCKED", MFG_LOCKED: "MFG_LOCKED", UNKNOWN: "UNKNOWN",
}

_TOOL_CODE_NAMES = {
    SCAN_FAIL: "SCAN_FAIL", MODEL_MISMATCH: "MODEL_MISMATCH", SN_RULE_FAIL: "SN_RULE_FAIL",
    SELF_TEST_FAIL: "SELF_TEST_FAIL", DEVICE_NOT_CONNECTED: "DEVICE_NOT_CONNECTED",
    STEP_EXCEPTION: "STEP_EXCEPTION", CONFIG_ERROR: "CONFIG_ERROR",
}


def code_name(code):
    """返回错误码的可读名称（调试 / JSON 报告用）。"""
    if code in _DEVICE_CODE_NAMES:
        return _DEVICE_CODE_NAMES[code]
    if code in _TOOL_CODE_NAMES:
        return _TOOL_CODE_NAMES[code]
    return f"CODE_{code}"


def is_device_code(code):
    """该码是否来自设备/HID（未来需要与真实固件透传区分时使用）。"""
    return code in _DEVICE_CODE_NAMES
