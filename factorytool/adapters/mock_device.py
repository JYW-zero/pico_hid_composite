"""Mock 设备 / 模拟器（原型唯一实现的适配器）。

不接任何硬件，在内存里模拟：连接、识别、写 SN、自检、制造锁定，并可通过
``faults`` 注入故障用于演示与测试：

- ``"connect"``          连接失败 / 超时
- ``"identify"``         识别无响应
- ``"wrong_model"``      识别到的型号与工单配置不符
- ``"write_sn"``         写 SN 返回 Flash 写入失败
- ``"mfg_locked"``       设备已出厂锁定（撤制造位、制造命令返回 MFG_LOCKED）
- ``"selftest"``         所有自检失败
- ``"selftest:<id>"``    指定自检项失败（如 ``selftest:keypad``）
"""

from __future__ import annotations

import time

from adapters.base import (
    Device,
    DeviceError,
    DeviceResponse,
    CAP_MFG_FACTORY_TEST,
    CAP_MFG_PROVISION,
    DEVICE_FLAG_MFG_SHIPPED_LOCKED,
)
from core.models import DeviceInfo
from core import errors

# 消费者基础能力位 bit0..15（与固件 CAP_BASE_BITMAP 对应，Mock 只取示意掩码）
_BASE_CAPS = 0x0000FFFF
_MFG_CAPS = (1 << CAP_MFG_FACTORY_TEST) | (1 << CAP_MFG_PROVISION)

# 各自检项的模拟测量值（成功时）
_SELF_TEST_MEASUREMENTS = {
    "keypad": {"keys_total": 64, "keys_ok": 64},
    "sensor": {"dpi_check": "ok", "lod_check": "ok"},
    "optical": {"dpi_check": "ok", "lod_check": "ok"},
    "joystick": {"center_x": 2048, "center_y": 2048, "deadzone_ok": True},
    "encoder": {"steps_ok": True, "switch_ok": True},
}


class MockDevice(Device):
    name = "mock"

    def __init__(self, model="PICO2-HID-COMPOSITE", firmware="0.1.0",
                 protocol="0.2", delay_ms=0, faults=None, mfg_locked=False):
        self.expected_model = model
        self.firmware = firmware
        self.protocol = protocol
        self.delay_ms = delay_ms
        self.faults = set(faults or [])
        self._mfg_locked = bool(mfg_locked) or ("mfg_locked" in self.faults)
        self._connected = False
        self._serial_written = ""
        self.self_test_calls = []   # 记录调用顺序，供测试断言

    def _sleep(self):
        if self.delay_ms > 0:
            time.sleep(self.delay_ms / 1000.0)

    def _require_connected(self):
        if not self._connected:
            raise DeviceError(errors.DEVICE_NOT_CONNECTED, "设备未连接")

    # ---- Device 接口 ----
    def connect(self) -> None:
        self._sleep()
        if "connect" in self.faults:
            raise DeviceError(errors.TIMEOUT, "连接设备超时（模拟故障）")
        self._connected = True

    def disconnect(self) -> None:
        self._connected = False

    def is_mfg_unlocked(self) -> bool:
        return not self._mfg_locked

    def identify(self) -> DeviceInfo:
        self._require_connected()
        self._sleep()
        if "identify" in self.faults:
            raise DeviceError(errors.TIMEOUT, "读取设备信息超时（模拟故障）")
        caps = _BASE_CAPS
        flags = 0
        if not self._mfg_locked:
            caps |= _MFG_CAPS
        else:
            flags |= DEVICE_FLAG_MFG_SHIPPED_LOCKED
        model = "UNKNOWN-DEVICE" if "wrong_model" in self.faults else self.expected_model
        return DeviceInfo(
            model=model, firmware=self.firmware, protocol=self.protocol,
            serial=self._serial_written, capabilities=caps, device_flags=flags,
            connected=True,
        )

    def write_serial(self, sn: str) -> DeviceResponse:
        self._require_connected()
        self._sleep()
        if self._mfg_locked:
            return DeviceResponse(errors.MFG_LOCKED, "设备已出厂锁定，制造写号被拒")
        if "write_sn" in self.faults:
            return DeviceResponse(errors.FLASH_WRITE, "写入序列号失败：Flash 编程错误（模拟）")
        self._serial_written = sn
        return DeviceResponse(errors.OK, f"序列号写入成功：{sn}", {"serial": sn})

    def run_self_test(self, test_id: str) -> DeviceResponse:
        self._require_connected()
        self._sleep()
        self.self_test_calls.append(test_id)
        failed = ("selftest" in self.faults) or (f"selftest:{test_id}" in self.faults)
        measurements = dict(_SELF_TEST_MEASUREMENTS.get(test_id, {"checked": True}))
        if failed:
            if test_id == "keypad":
                measurements.update(keys_ok=63, failed_keys=[17])
            return DeviceResponse(
                errors.SELF_TEST_FAIL,
                f"自检未通过：{test_id}",
                {"test_id": test_id, "measurements": measurements},
            )
        return DeviceResponse(
            errors.OK, f"自检通过：{test_id}",
            {"test_id": test_id, "measurements": measurements},
        )

    def ship_lock(self) -> DeviceResponse:
        """原型：仅在内存置位，模拟出厂锁定（真实持久化随阶段后期制造命令）。"""
        self._require_connected()
        self._mfg_locked = True
        return DeviceResponse(errors.OK, "已出厂锁定（Mock，仅内存态）")
