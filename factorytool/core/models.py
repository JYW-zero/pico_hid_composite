"""结构化结果数据模型。

每个步骤返回 :class:`StepResult`，一次完整产线流程汇总为 :class:`TestResult`，
统一可序列化为 JSON——GUI、CLI、未来的夹具脚本 / MES 读同一份结构。
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field, asdict
from datetime import datetime

# 步骤 / 总判定状态
PASS = "pass"
FAIL = "fail"
SKIP = "skip"
ERROR = "error"


def now_iso():
    """本地时间 ISO-8601（秒级），报告时间戳用。"""
    return datetime.now().isoformat(timespec="seconds")


@dataclass
class DeviceInfo:
    """设备识别信息（对应固件 Device Info 报告的精简版）。"""

    model: str = ""
    firmware: str = ""
    protocol: str = ""
    serial: str = ""
    capabilities: int = 0
    device_flags: int = 0
    connected: bool = False

    def to_dict(self):
        return asdict(self)


@dataclass
class StepResult:
    """单个产线步骤的结构化结果。

    :param name: 步骤名（scan / identify / write_sn / self_test:<id> ...）
    :param status: pass / fail / skip / error
    :param code: 0 成功，否则为 errors.py 中的设备码或工具码
    :param code_source: 错误码来源："device"=设备/HID 透传码（0~12/0xFF），
        "tool"=工装流程码（100+），""=跳过等无设备应答情形
    :param message: 人类可读信息（中文，可直接展示给操作员）
    :param elapsed_ms: 该步骤耗时（毫秒）
    :param details: 结构化测量值 / 附加数据（MES / 调试用）
    """

    name: str
    status: str = PASS
    code: int = 0
    code_source: str = ""
    message: str = ""
    elapsed_ms: int = 0
    details: dict = field(default_factory=dict)

    @property
    def ok(self) -> bool:
        return self.status == PASS

    def to_dict(self):
        return asdict(self)


@dataclass
class TestResult:
    """一次完整"单板过站"流程的结果。"""

    serial: str = ""
    model: str = ""
    overall: str = FAIL
    started_at: str = ""
    finished_at: str = ""
    total_elapsed_ms: int = 0
    adapter: str = "mock"          # mock / hid / serial / fixture ...
    device: dict = field(default_factory=dict)
    steps: list = field(default_factory=list)   # list[StepResult]
    logs: list = field(default_factory=list)    # list[str]
    report_path: str = ""

    @property
    def passed(self) -> bool:
        return self.overall == PASS

    def add_step(self, step: StepResult) -> None:
        self.steps.append(step)

    def add_log(self, line: str) -> None:
        self.logs.append(line)

    def to_dict(self):
        return {
            "serial": self.serial,
            "model": self.model,
            "overall": self.overall,
            "passed": self.passed,
            "started_at": self.started_at,
            "finished_at": self.finished_at,
            "total_elapsed_ms": self.total_elapsed_ms,
            "adapter": self.adapter,
            "device": self.device,
            "steps": [s.to_dict() for s in self.steps],
            "logs": self.logs,
            "report_path": self.report_path,
        }

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent, ensure_ascii=False)
