"""序列号（SN）规则校验与示例生成。"""

from __future__ import annotations

import re
import string
from dataclasses import dataclass

from core import errors


@dataclass
class SnRule:
    prefix: str = "P2"
    length: int = 12
    # 允许的字符集：大写字母 + 数字（条码常见）
    charset: str = string.ascii_uppercase + string.digits

    @classmethod
    def from_dict(cls, d):
        d = d or {}
        return cls(
            prefix=str(d.get("prefix", "P2")),
            length=int(d.get("length", 12)),
            charset=str(d.get("charset", string.ascii_uppercase + string.digits)),
        )

    def validate(self, sn: str):
        """返回 (ok, code, message)。"""
        if not sn:
            return False, errors.SCAN_FAIL, "序列号为空"
        if len(sn) != self.length:
            return False, errors.SN_RULE_FAIL, (
                f"序列号长度 {len(sn)} 不符合规则（要求 {self.length} 位）")
        if self.prefix and not sn.startswith(self.prefix):
            return False, errors.SN_RULE_FAIL, (
                f"序列号前缀应为 '{self.prefix}'，实际 '{sn[:len(self.prefix)]}'")
        invalid = [c for c in sn if c not in self.charset]
        if invalid:
            return False, errors.SN_RULE_FAIL, (
                f"序列号含非法字符：{''.join(sorted(set(invalid)))}")
        return True, errors.OK, "序列号规则校验通过"

    def generate(self, seq: int) -> str:
        """按规则生成一个示例 SN（prefix + 序号补零），供演示 / Mock 使用。"""
        body_width = max(0, self.length - len(self.prefix))
        body = str(seq % (10 ** body_width)).zfill(body_width) if body_width else ""
        return f"{self.prefix}{body}"
