"""设备抽象（第二个"缝"）。

GUI / CLI / 未来夹具脚本都只依赖 :class:`Device` 接口，不关心背后是 Mock、
真实 USB-HID 还是测试夹具。原型只实现 :class:`~adapters.mock_device.MockDevice`；
后续新增 ``hid.py``（移植 pc_tool 的 HID/协议层）、``serial.py``、``fixture.py``
实现同一接口即可，核心流程与外壳无需改动。
"""

from __future__ import annotations

import abc
from dataclasses import dataclass, field

from core.models import DeviceInfo
from core import errors

# 与固件 capability_bitmap / device_flags 对齐（详见固件 config_hid.h、方案 §6/§9）
CAP_MFG_FACTORY_TEST = 24
CAP_MFG_PROVISION = 25
DEVICE_FLAG_MFG_SHIPPED_LOCKED = 1 << 0
DEVICE_FLAG_MFG_SESSION_UNLOCKED = 1 << 1


class DeviceError(Exception):
    """通信 / 连接类错误（设备根本没应答），携带设备错误码。"""

    def __init__(self, code: int, message: str = ""):
        self.code = code
        super().__init__(message or errors.code_name(code))


@dataclass
class DeviceResponse:
    """设备对一次业务操作（写 SN / 自检）的应答。

    业务失败（如写 Flash 失败、自检不过、制造锁定）用 code 表达，不抛异常，
    交由编排层判定；只有通信层失败才抛 :class:`DeviceError`。
    """

    code: int = errors.OK
    message: str = ""
    details: dict = field(default_factory=dict)

    @property
    def ok(self) -> bool:
        return self.code == errors.OK


class Device(abc.ABC):
    """产线设备适配器接口。"""

    name: str = "device"

    @abc.abstractmethod
    def connect(self) -> None:
        """建立连接；失败抛 :class:`DeviceError`。"""

    @abc.abstractmethod
    def disconnect(self) -> None:
        """断开连接，幂等。"""

    @abc.abstractmethod
    def identify(self) -> DeviceInfo:
        """读取设备信息（型号 / 固件 / 协议 / 能力位 / 标志）。"""

    @abc.abstractmethod
    def is_mfg_unlocked(self) -> bool:
        """制造能力当前是否可用（未出厂锁定或本会话已售后解锁）。"""

    @abc.abstractmethod
    def write_serial(self, sn: str) -> DeviceResponse:
        """烧录 / 写入序列号（制造命令）。"""

    @abc.abstractmethod
    def run_self_test(self, test_id: str) -> DeviceResponse:
        """运行一项出厂自检，返回应答（含测量值 details）。"""

    def ship_lock(self) -> DeviceResponse:  # pragma: no cover - 阶段后期实现
        """出厂锁定（阶段 3 后期随真实制造命令实现）。"""
        raise NotImplementedError("ship_lock 将在接入真实制造命令后实现")
