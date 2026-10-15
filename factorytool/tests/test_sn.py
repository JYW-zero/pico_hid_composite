import unittest
from core.sn import SnRule
from core import errors


class TestSnRule(unittest.TestCase):
    def setUp(self):
        self.rule = SnRule(prefix="P2", length=12)

    def test_valid_sn(self):
        ok, code, _ = self.rule.validate("P20000000001")
        self.assertTrue(ok)
        self.assertEqual(code, errors.OK)

    def test_empty(self):
        ok, code, _ = self.rule.validate("")
        self.assertFalse(ok)
        self.assertEqual(code, errors.SCAN_FAIL)

    def test_wrong_length(self):
        ok, code, _ = self.rule.validate("ABC")
        self.assertFalse(ok)
        self.assertEqual(code, errors.SN_RULE_FAIL)

    def test_wrong_prefix(self):
        ok, code, _ = self.rule.validate("XX0000000001")
        self.assertFalse(ok)
        self.assertEqual(code, errors.SN_RULE_FAIL)

    def test_illegal_charset(self):
        ok, code, _ = self.rule.validate("P2-00000000")
        self.assertFalse(ok)
        self.assertEqual(code, errors.SN_RULE_FAIL)

    def test_generate_conforms(self):
        sn = self.rule.generate(7)
        self.assertEqual(len(sn), 12)
        self.assertTrue(sn.startswith("P2"))
        ok, _, _ = self.rule.validate(sn)
        self.assertTrue(ok)

    def test_from_dict_defaults(self):
        r = SnRule.from_dict({})
        self.assertEqual(r.prefix, "P2")
        self.assertEqual(r.length, 12)


if __name__ == "__main__":
    unittest.main()
