import json
import os
import tempfile
import unittest

from core.config import load_config
from core.workflow import run_test
from adapters.mock_device import MockDevice
from core import errors

CONFIG = os.path.join(os.path.dirname(os.path.dirname(__file__)),
                      "configs", "model_pico2.json")
SN = "P20000000001"


class TestWorkflow(unittest.TestCase):
    def setUp(self):
        self.cfg = load_config(CONFIG)
        self.rep = tempfile.mkdtemp(prefix="ft_test_")

    def run_flow(self, faults=None, scan=SN, sn="", mfg_locked=False):
        d = MockDevice(model=self.cfg.model, faults=faults, mfg_locked=mfg_locked)
        return run_test(self.cfg, d, scan_code=scan, serial=sn,
                        adapter_name="mock", log=lambda *_: None, report_dir=self.rep)

    def step(self, result, name):
        return next(s for s in result.steps if s.name == name)

    def test_full_pass(self):
        r = self.run_flow()
        self.assertTrue(r.passed)
        self.assertEqual(r.overall, "pass")
        self.assertEqual(r.serial, SN)
        self.assertEqual(r.model, self.cfg.model)
        self.assertEqual(len(r.steps), 6)  # connect/scan/identify/write_sn + 2 tests
        self.assertTrue(all(s.ok for s in r.steps))
        self.assertTrue(r.report_path and os.path.isfile(r.report_path))

    def test_report_json_schema(self):
        r = self.run_flow()
        with open(r.report_path, "r", encoding="utf-8") as f:
            d = json.load(f)
        for key in ("serial", "model", "overall", "passed", "started_at",
                    "finished_at", "total_elapsed_ms", "adapter", "device",
                    "steps", "logs", "report_path"):
            self.assertIn(key, d)
        self.assertTrue(d["passed"])
        self.assertEqual(d["device"]["model"], self.cfg.model)
        step_names = {s["name"] for s in d["steps"]}
        self.assertIn("write_sn", step_names)
        self.assertIn("self_test:keypad", step_names)
        self.assertIn("self_test:sensor", step_names)

    def test_connect_failure_marks_skips(self):
        r = self.run_flow(faults=["connect"])
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "connect").status, "error")
        self.assertEqual(self.step(r, "scan").status, "skip")
        self.assertEqual(self.step(r, "self_test:keypad").status, "skip")

    def test_no_scan(self):
        r = self.run_flow(scan="")
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "scan").code, errors.SCAN_FAIL)

    def test_wrong_model(self):
        r = self.run_flow(faults=["wrong_model"])
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "identify").code, errors.MODEL_MISMATCH)

    def test_write_sn_flash_failure(self):
        r = self.run_flow(faults=["write_sn"])
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "write_sn").code, errors.FLASH_WRITE)

    def test_mfg_locked(self):
        r = self.run_flow(mfg_locked=True)
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "write_sn").code, errors.MFG_LOCKED)

    def test_self_test_failure(self):
        r = self.run_flow(faults=["selftest:keypad"])
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "self_test:keypad").code, errors.SELF_TEST_FAIL)
        self.assertTrue(self.step(r, "self_test:sensor").ok)

    def test_bad_sn_rule(self):
        r = self.run_flow(scan="ABC")
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "write_sn").code, errors.SN_RULE_FAIL)

    def test_explicit_sn_overrides_scan(self):
        r = self.run_flow(scan="IGNORED-BARCODE", sn=SN)
        self.assertTrue(r.passed)
        self.assertEqual(r.serial, SN)

    # ---- 前置致命失败短路（P2）----

    def test_wrong_model_skips_downstream_and_does_not_write_sn(self):
        d = MockDevice(model=self.cfg.model, faults=["wrong_model"])
        r = run_test(self.cfg, d, scan_code=SN, adapter_name="mock",
                     log=lambda *_: None, report_dir=self.rep)
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "identify").code, errors.MODEL_MISMATCH)
        # 型号不符：写号与所有自检必须跳过，且绝不能真的写号
        self.assertEqual(self.step(r, "write_sn").status, "skip")
        self.assertEqual(self.step(r, "self_test:keypad").status, "skip")
        self.assertEqual(self.step(r, "self_test:sensor").status, "skip")
        self.assertEqual(d._serial_written, "")

    def test_no_scan_skips_write_sn(self):
        r = self.run_flow(scan="")
        self.assertFalse(r.passed)
        self.assertEqual(self.step(r, "scan").code, errors.SCAN_FAIL)
        # 没有条码 / SN：写号标记跳过，而不是再报一次 SN 规则失败
        self.assertEqual(self.step(r, "write_sn").status, "skip")

    # ---- 错误码来源（P2）----

    def test_code_source_classifies_device_and_tool_codes(self):
        r_dev = self.run_flow(faults=["write_sn"])
        self.assertEqual(self.step(r_dev, "write_sn").code, errors.FLASH_WRITE)
        self.assertEqual(self.step(r_dev, "write_sn").code_source, "device")

        r_scan = self.run_flow(scan="")
        self.assertEqual(self.step(r_scan, "scan").code, errors.SCAN_FAIL)
        self.assertEqual(self.step(r_scan, "scan").code_source, "tool")

        r_ok = self.run_flow()
        # code=0(OK) 也属于设备码域
        self.assertEqual(self.step(r_ok, "connect").code_source, "device")

    def test_skip_steps_have_empty_code_source(self):
        r = self.run_flow(faults=["wrong_model"])
        self.assertEqual(self.step(r, "write_sn").code_source, "")
        self.assertEqual(self.step(r, "write_sn").code, 0)

    # ---- 报告文件名防覆盖（P2）----

    def test_reports_not_overwritten_for_same_sn(self):
        r1 = self.run_flow()
        r2 = self.run_flow()
        self.assertNotEqual(r1.report_path, r2.report_path)
        self.assertTrue(os.path.isfile(r1.report_path))
        self.assertTrue(os.path.isfile(r2.report_path))


if __name__ == "__main__":
    unittest.main()
