"""产线最小闭环编排（核心逻辑，第一个"缝"）。

``run_test`` 不依赖 GUI / CLI / 具体设备，输入工装配置、设备适配器与本次过站
输入（扫码、SN），输出 :class:`TestResult` 并落盘 JSON。GUI、CLI、未来夹具脚本
都调用同一个函数。

流程（与原型验证目标一一对应）：
    连接 -> 扫码 -> 识别型号 -> 写 SN -> 配置声明的 1~N 项自检 -> PASS/FAIL 判定 -> JSON
"""

from __future__ import annotations

import json
import os
import re
import time
from datetime import datetime

from adapters.base import DeviceError
from core import errors
from core.config import WorkflowConfig
from core.models import (
    TestResult, StepResult, DeviceInfo,
    PASS, FAIL, SKIP, ERROR, now_iso,
)


def _elapsed_ms(start_perf: float) -> int:
    return int(round((time.perf_counter() - start_perf) * 1000))


def _safe_name(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9_-]+", "_", text).strip("_") or "unknown"


def _code_source(code: int) -> str:
    """区分错误码来源：设备/HID 透传码 vs 工装流程码。"""
    return "device" if errors.is_device_code(code) else "tool"


def _skip_step(name, message, required=True):
    """构造一个"因前置致命失败而跳过"的步骤。"""
    return StepResult(name=name, status=SKIP, code=0, code_source="",
                      message=message, details={"required": required})


def _step(result: TestResult, name, fn, log, required=True):
    """执行单个步骤：计时、捕获异常、记录结构化结果。返回 StepResult。"""
    log(f"[{name}] 开始 ...")
    t0 = time.perf_counter()
    try:
        code, message, details, status = fn()
        step = StepResult(
            name=name, status=status, code=code,
            code_source=_code_source(code),
            message=message,
            elapsed_ms=_elapsed_ms(t0), details=dict(details or {}),
        )
    except DeviceError as e:
        step = StepResult(
            name=name, status=ERROR, code=e.code,
            code_source=_code_source(e.code), message=str(e),
            elapsed_ms=_elapsed_ms(t0), details={"required": required},
        )
    except Exception as e:  # 兜底：任何意外都转为结构化错误，不让工装崩给操作员
        step = StepResult(
            name=name, status=ERROR, code=errors.STEP_EXCEPTION,
            code_source="tool",
            message=f"步骤异常：{e!r}", elapsed_ms=_elapsed_ms(t0),
            details={"required": required},
        )
    step.details.setdefault("required", required)
    result.add_step(step)
    icon = {"pass": "OK", "fail": "FAIL", "skip": "SKIP", "error": "ERR"}[step.status]
    result.add_log(f"[{name}] {icon} ({step.code}) {step.message}")
    log(f"[{name}] {icon} {step.message}")
    return step


def _write_report(result: TestResult, report_dir: str, log) -> None:
    try:
        os.makedirs(report_dir, exist_ok=True)
        # 文件名时间戳精确到毫秒；同毫秒碰撞再追加序号，避免同一 SN 复测互相覆盖
        ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        base = f"{_safe_name(result.serial)}_{ts}"
        path = os.path.join(report_dir, f"{base}.json")
        seq = 1
        while os.path.exists(path):
            path = os.path.join(report_dir, f"{base}_{seq}.json")
            seq += 1
        with open(path, "w", encoding="utf-8") as f:
            f.write(result.to_json())
        result.report_path = os.path.abspath(path)
        log(f"[report] 已写入 {result.report_path}")
    except OSError as e:
        result.add_log(f"[report] 写报告失败：{e}")
        log(f"[report] 写报告失败：{e}")


def _finalize(result, overall, overall_t0, report_dir, log, device):
    """设定总判定、收尾时间戳、落盘报告并断开设备（各短路分支共用）。"""
    result.overall = overall
    result.finished_at = now_iso()
    result.total_elapsed_ms = _elapsed_ms(overall_t0)
    result.add_log(f"[judge] 最终判定：{overall.upper()}")
    log(f"[judge] 最终判定：{overall.upper()}")
    _write_report(result, report_dir, log)
    try:
        device.disconnect()
    except Exception:  # 断开不影响判定
        pass
    return result


def run_test(config: WorkflowConfig, device, scan_code: str = "",
             serial: str = "", adapter_name: str = "mock",
             log=print, report_dir: str = "") -> TestResult:
    """执行一次完整过站流程。

    :param config: 工装配置（型号 / SN 规则 / 自检步骤 / 报告目录）
    :param device: 已构造但未连接的 :class:`adapters.base.Device`
    :param scan_code: 扫码枪读到的条码（barcode_is_serial 时即作为 SN）
    :param serial: 显式指定 SN（优先于扫码值，调试 / 手动工位用）
    :param adapter_name: 适配器名（mock/hid/...，记入报告）
    :param log: 日志回调
    :param report_dir: 覆盖配置中的报告目录（测试用临时目录）
    """
    report_dir = report_dir or config.report_dir
    result = TestResult(model=config.model, adapter=adapter_name,
                        started_at=now_iso())
    overall_t0 = time.perf_counter()
    fatal_fail = False

    def fail(step, fatal=True):
        nonlocal fatal_fail
        if fatal and not step.ok:
            fatal_fail = True

    # 1) 连接 ---------------------------------------------------------------
    def do_connect():
        device.connect()
        return errors.OK, "设备已连接", {}, PASS
    if not _step(result, "connect", do_connect, log).ok:
        # 设备都没连上，后续步骤无法执行，标记跳过并直接出报告
        for s in ("scan", "identify", "write_sn"):
            result.add_step(_skip_step(s, "因连接失败而跳过"))
        for t in config.tests:
            result.add_step(_skip_step(f"self_test:{t.test_id}",
                                       "因连接失败而跳过", t.required))
        return _finalize(result, FAIL, overall_t0, report_dir, log, device)

    # 2) 扫码 ---------------------------------------------------------------
    scan_code = (scan_code or "").strip()

    def do_scan():
        if not scan_code:
            return errors.SCAN_FAIL, "未扫到条码", {}, FAIL
        return errors.OK, f"已扫码：{scan_code}", {"barcode": scan_code}, PASS
    scan_step = _step(result, "scan", do_scan, log)
    fail(scan_step)
    scan_fatal = not scan_step.ok

    # 确定本次序列号
    sn = (serial or "").strip()
    if not sn and config.barcode_is_serial:
        sn = scan_code
    result.serial = sn

    # 3) 识别型号 -----------------------------------------------------------
    def do_identify():
        info = device.identify()  # type: DeviceInfo
        result.device = info.to_dict()
        if info.model != config.model:
            return (errors.MODEL_MISMATCH,
                    f"型号不符：设备为 '{info.model}'，工单要求 '{config.model}'",
                    {"device_model": info.model, "expected_model": config.model}, FAIL)
        return (errors.OK,
                f"型号匹配：{info.model}（固件 {info.firmware} / 协议 {info.protocol}）",
                result.device, PASS)
    id_step = _step(result, "identify", do_identify, log)
    fail(id_step)
    if not id_step.ok:
        # 型号不符 / 识别失败：不得写号，自检无意义，后续步骤全部跳过
        result.add_step(_skip_step("write_sn", "因型号识别失败而跳过"))
        for t in config.tests:
            result.add_step(_skip_step(f"self_test:{t.test_id}",
                                       "因型号识别失败而跳过", t.required))
        return _finalize(result, FAIL, overall_t0, report_dir, log, device)

    # 4) 写 SN（先规则校验，再发设备）--------------------------------------
    if scan_fatal:
        # 没有条码 / SN，写号无从执行；扫码失败已致命，此处标记跳过而非再报一次失败
        result.add_step(_skip_step("write_sn", "因扫码失败、无序列号而跳过"))
    else:
        def do_write_sn():
            ok, code, msg = config.sn_rule.validate(sn)
            if not ok:
                return code, msg, {"serial": sn}, FAIL
            if not device.is_mfg_unlocked():
                return errors.MFG_LOCKED, "制造能力已锁定，无法写号", {"serial": sn}, FAIL
            resp = device.write_serial(sn)
            if not resp.ok:
                return resp.code, resp.message, {"serial": sn, **resp.details}, FAIL
            return errors.OK, resp.message, {"serial": sn}, PASS
        fail(_step(result, "write_sn", do_write_sn, log))

    # 5) 自检步骤（配置化）--------------------------------------------------
    for t in config.tests:
        def do_self_test(t=t):
            resp = device.run_self_test(t.test_id)
            if not resp.ok:
                return resp.code, f"{t.label} 未通过", resp.details, FAIL
            return errors.OK, f"{t.label} 通过", resp.details, PASS
        step = _step(result, f"self_test:{t.test_id}", do_self_test, log,
                     required=t.required)
        if not step.ok and t.required:
            fatal_fail = True

    # 6) 判定 + 报告 --------------------------------------------------------
    return _finalize(result, FAIL if fatal_fail else PASS,
                     overall_t0, report_dir, log, device)
