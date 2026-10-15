"""设备工厂：按工装配置与命令行 / GUI 选项构造适配器。

原型只支持 Mock；真实 HID / 串口 / 夹具适配器后续在此按同一 :class:`Device`
接口接入（第三个"缝"），核心流程与外壳无需改动。
"""

from __future__ import annotations

from adapters.mock_device import MockDevice

# 故障注入选项（GUI 下拉 / CLI --fault 共用）
FAULT_CHOICES = [
    "none", "connect", "identify", "wrong_model",
    "write_sn", "mfg_locked", "selftest", "selftest:keypad", "selftest:sensor",
]


def build_device(config, use_mock: bool = True, fault: str = "none",
                 mfg_locked: bool = False):
    """根据配置构造设备。

    :param config: :class:`core.config.WorkflowConfig`
    :param use_mock: 原型仅支持 True；False 留待真实 HID 适配器
    :param fault: 故障注入键（见 FAULT_CHOICES / MockDevice.faults）
    :param mfg_locked: 是否模拟已出厂锁定
    """
    if not use_mock:
        # 留缝：接入真实设备时在此返回 HidDevice（移植 pc_tool 的 HID/协议层）
        raise NotImplementedError(
            "真实 HID 适配器尚未实现：原型阶段请使用 Mock（--mock / 勾选 Mock）")

    faults = [fault] if fault and fault != "none" else []
    m = config.mock or {}
    return MockDevice(
        model=config.model,
        firmware=m.get("firmware", "0.1.0"),
        protocol=m.get("protocol", "0.2"),
        delay_ms=int(m.get("delay_ms", 0)),
        faults=faults,
        mfg_locked=bool(mfg_locked) or (fault == "mfg_locked"),
    )
