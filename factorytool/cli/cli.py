"""最小 CLI 调试入口（第二个外壳，只做一个 run 命令，不做完整命令体系）。

用法：
    python run.py gui                      # 启动极简 GUI（默认）
    python run.py run --mock --scan P2..   # 跑一次过站流程
    python run.py run --mock --fault write_sn
"""

from __future__ import annotations

import argparse
import os

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # factorytool/
DEFAULT_CONFIG = os.path.join(_ROOT, "configs", "model_pico2.json")


def cmd_run(args) -> int:
    from core.config import load_config, ConfigError
    from adapters.factory import build_device
    from core.workflow import run_test

    try:
        cfg = load_config(args.config)
    except ConfigError as e:
        print(f"[config] {e}")
        return 2

    try:
        device = build_device(cfg, use_mock=args.mock, fault=args.fault,
                              mfg_locked=args.locked)
    except NotImplementedError as e:
        print(f"[device] {e}")
        return 2

    adapter = "mock" if args.mock else "hid"
    print(f"=== FactoryTool 原型（{adapter}）型号 {cfg.model} ===")
    result = run_test(cfg, device, scan_code=args.scan or "", serial=args.sn or "",
                      adapter_name=adapter, log=print,
                      report_dir=args.report_dir or cfg.report_dir)

    print("-" * 56)
    for s in result.steps:
        print(f"  {s.status.upper():<4} {s.name:<20} {s.elapsed_ms:>5} ms  {s.message}")
    print("-" * 56)
    verdict = "PASS" if result.passed else "FAIL"
    print(f"判定：{verdict}    SN：{result.serial or '(空)'}    总耗时：{result.total_elapsed_ms} ms")
    if result.report_path:
        print(f"报告：{result.report_path}")
    return 0 if result.passed else 1


def cmd_gui(_args) -> int:
    from ui.gui import launch
    launch()
    return 0


def main(argv=None) -> int:
    from adapters.factory import FAULT_CHOICES

    parser = argparse.ArgumentParser(
        prog="factorytool",
        description="FactoryTool 原型：扫码->识别->写SN->自检->PASS/FAIL（默认 Mock）")
    sub = parser.add_subparsers(dest="command")

    p_run = sub.add_parser("run", help="执行一次过站流程")
    p_run.add_argument("--config", "-c", default=DEFAULT_CONFIG, help="工装配置 JSON")
    p_run.add_argument("--mock", dest="mock", action="store_true", default=True,
                       help="使用 Mock 设备（原型默认）")
    p_run.add_argument("--no-mock", dest="mock", action="store_false",
                       help="使用真实设备（原型阶段尚未实现）")
    p_run.add_argument("--scan", default="", help="扫码枪条码（默认即 SN）")
    p_run.add_argument("--sn", default="", help="显式序列号（优先于扫码）")
    p_run.add_argument("--fault", default="none", choices=FAULT_CHOICES,
                       help="注入故障：" + "/".join(FAULT_CHOICES))
    p_run.add_argument("--locked", action="store_true", help="模拟设备已出厂锁定")
    p_run.add_argument("--report-dir", default="", help="覆盖报告输出目录")
    p_run.set_defaults(func=cmd_run)

    p_gui = sub.add_parser("gui", help="启动极简 GUI")
    p_gui.set_defaults(func=cmd_gui)

    args = parser.parse_args(argv)
    if not getattr(args, "command", None):
        return cmd_gui(args)
    return args.func(args)
