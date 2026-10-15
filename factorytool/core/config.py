"""工装配置加载（JSON）。

配置描述"某型号怎么过站"：型号名、SN 规则、要跑哪些自检步骤、报告目录、
Mock 设备行为。原型用 JSON（标准库零依赖）；后续如需 YAML，只需在此模块加
一个可选 loader，核心与外壳不受影响。
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field

from core.sn import SnRule


class ConfigError(Exception):
    """工装配置文件缺失 / 非法。"""


@dataclass
class TestSpec:
    test_id: str
    label: str = ""
    required: bool = True


@dataclass
class WorkflowConfig:
    model: str
    sn_rule: SnRule
    tests: list = field(default_factory=list)          # list[TestSpec]
    report_dir: str = "reports"
    description: str = ""
    barcode_is_serial: bool = True
    mock: dict = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: dict):
        if not isinstance(d, dict):
            raise ConfigError("配置根节点必须是 JSON 对象")
        if "model" not in d or not str(d["model"]).strip():
            raise ConfigError("配置缺少必填字段 'model'")

        tests = []
        for i, t in enumerate(d.get("tests", [])):
            if "test_id" not in t:
                raise ConfigError(f"tests[{i}] 缺少 test_id")
            tests.append(TestSpec(
                test_id=str(t["test_id"]),
                label=str(t.get("label", t["test_id"])),
                required=bool(t.get("required", True)),
            ))

        # report_dir 为相对路径时相对工具运行工作目录（通常是 factorytool/），
        # 不绑死配置文件所在目录，避免不同型号配置把报告写散到 configs/ 下。
        report_dir = d.get("report_dir", "reports")

        scan = d.get("scan", {}) or {}
        return cls(
            model=str(d["model"]).strip(),
            sn_rule=SnRule.from_dict(d.get("sn_rule", {})),
            tests=tests,
            report_dir=report_dir,
            description=str(d.get("description", "")),
            barcode_is_serial=bool(scan.get("barcode_is_serial", True)),
            mock=d.get("mock", {}) or {},
        )


def load_config(path: str) -> WorkflowConfig:
    """从 JSON 文件加载工装配置。"""
    if not os.path.isfile(path):
        raise ConfigError(f"配置文件不存在：{path}")
    try:
        with open(path, "r", encoding="utf-8") as f:
            d = json.load(f)
    except json.JSONDecodeError as e:
        raise ConfigError(f"配置 JSON 解析失败：{e}") from e
    return WorkflowConfig.from_dict(d)
