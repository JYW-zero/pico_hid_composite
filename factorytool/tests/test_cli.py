"""CLI 参数校验测试（--fault 必须取自 FAULT_CHOICES，拼错即报错而非静默跑 PASS）。"""

import os
import tempfile
import unittest

from cli.cli import main

CONFIG = os.path.join(os.path.dirname(os.path.dirname(__file__)),
                      "configs", "model_pico2.json")


class TestCliFaultChoices(unittest.TestCase):
    def test_invalid_fault_is_rejected(self):
        # argparse 遇到非法 choices 会以 exit code 2 退出，而不是静默当作无故障
        with self.assertRaises(SystemExit) as cm:
            main(["run", "--config", CONFIG, "--fault", "write_snn"])
        self.assertEqual(cm.exception.code, 2)

    def test_valid_fault_runs_and_reports_fail(self):
        # 合法故障键应被接受并正常跑完（wrong_model -> 判定 FAIL -> exit 1）
        with tempfile.TemporaryDirectory() as d:
            rc = main(["run", "--config", CONFIG, "--fault", "wrong_model",
                       "--report-dir", d])
        self.assertEqual(rc, 1)

    def test_valid_none_runs_and_reports_pass(self):
        with tempfile.TemporaryDirectory() as d:
            rc = main(["run", "--config", CONFIG, "--scan", "P20000000001",
                       "--report-dir", d])
        self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
