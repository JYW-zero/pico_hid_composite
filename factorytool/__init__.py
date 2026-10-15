"""FactoryTool 原型包。

最小闭环：扫码 -> 识别型号 -> 写 SN -> 1~2 项自检 -> PASS/FAIL -> JSON 报告。
核心逻辑在 core/，设备抽象在 adapters/，ui(GUI) 与 cli 都只是薄壳。
原型仅依赖 Python 标准库，设备用 MockDevice，不接真实硬件。
"""

__version__ = "0.1.0-prototype"
