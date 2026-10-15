import json
import unittest
from core.models import TestResult, StepResult, DeviceInfo, PASS, FAIL
from core import errors


class TestModels(unittest.TestCase):
    def test_step_to_dict(self):
        s = StepResult(name="write_sn", status=PASS, code=errors.OK,
                       message="序列号写入成功：P20000000001", elapsed_ms=12)
        d = s.to_dict()
        self.assertEqual(d["name"], "write_sn")
        self.assertEqual(d["code"], 0)
        self.assertEqual(d["elapsed_ms"], 12)

    def test_result_passed_flag(self):
        r = TestResult(serial="X", overall=PASS)
        self.assertTrue(r.passed)
        r.overall = FAIL
        self.assertFalse(r.passed)

    def test_json_roundtrip_and_chinese(self):
        r = TestResult(serial="P20000000001", model="M", overall=PASS)
        r.device = DeviceInfo(model="M", connected=True).to_dict()
        r.add_step(StepResult(name="scan", message="已扫码：P20000000001"))
        text = r.to_json()
        self.assertIn("已扫码", text)  # ensure_ascii=False，中文不转义
        d = json.loads(text)
        self.assertTrue(d["passed"])
        self.assertEqual(d["steps"][0]["message"], "已扫码：P20000000001")
        self.assertEqual(d["device"]["model"], "M")


if __name__ == "__main__":
    unittest.main()
