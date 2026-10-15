#!/usr/bin/env python3
"""FactoryTool 原型入口。

    python run.py                      # 默认启动极简 GUI
    python run.py gui                  # 显式启动 GUI
    python run.py run --mock --scan P20000000001
    python run.py run --mock --fault write_sn

仅依赖 Python 标准库；设备默认 Mock，不接真实硬件。
"""

import os
import sys

# 保证无论从哪个目录调用，都能 import 到项目内的 core/adapters/ui/cli 包
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from cli.cli import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
