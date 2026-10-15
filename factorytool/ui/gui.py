"""极简演示 GUI（第一个外壳，Tkinter 标准库，零第三方依赖）。

只负责采集输入、调用 :func:`core.workflow.run_test`、展示结构化结果；
所有判定逻辑都在 core，后台线程执行避免界面卡死。
"""

from __future__ import annotations

import os
import queue
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog, messagebox

from core.config import load_config, ConfigError
from adapters.factory import build_device, FAULT_CHOICES
from core.workflow import run_test

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # factorytool/
DEFAULT_CONFIG = os.path.join(_ROOT, "configs", "model_pico2.json")

_STATUS_COLOR = {"pass": "#1a7f37", "fail": "#c0392b",
                 "error": "#c0392b", "skip": "#7a7a7a"}


class FactoryToolApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.q = queue.Queue()
        self.worker = None
        self.cfg = None

        root.title("FactoryTool 原型（Mock 最小闭环）")
        root.geometry("820x680")
        root.minsize(720, 600)

        self._build_form()
        self._build_result_area()
        self._load_default_config()
        self._poll()

    # ---------------- 界面构建 ----------------
    def _build_form(self):
        form = ttk.LabelFrame(self.root, text="过站输入")
        form.pack(fill="x", padx=10, pady=(10, 6))

        # 配置文件
        row = ttk.Frame(form); row.pack(fill="x", padx=8, pady=4)
        ttk.Label(row, text="工装配置：", width=10).pack(side="left")
        self.var_config = tk.StringVar(value=DEFAULT_CONFIG)
        ttk.Entry(row, textvariable=self.var_config).pack(side="left", fill="x", expand=True)
        ttk.Button(row, text="浏览…", command=self._browse_config).pack(side="left", padx=(6, 0))
        ttk.Button(row, text="重载", command=self._load_default_config).pack(side="left", padx=(6, 0))

        # 扫码 / SN / 故障 / Mock
        row = ttk.Frame(form); row.pack(fill="x", padx=8, pady=4)
        ttk.Label(row, text="扫码(SN)：", width=10).pack(side="left")
        self.var_scan = tk.StringVar()
        ttk.Entry(row, textvariable=self.var_scan, width=18).pack(side="left")
        ttk.Button(row, text="生成示例SN", command=self._gen_sn).pack(side="left", padx=(6, 16))

        ttk.Label(row, text="故障注入：").pack(side="left")
        self.var_fault = tk.StringVar(value="none")
        ttk.Combobox(row, textvariable=self.var_fault, values=FAULT_CHOICES,
                     state="readonly", width=16).pack(side="left", padx=(0, 16))

        self.var_mock = tk.BooleanVar(value=True)
        ttk.Checkbutton(row, text="Mock 设备（原型仅支持 Mock）",
                        variable=self.var_mock, state="disabled").pack(side="left")

        row = ttk.Frame(form); row.pack(fill="x", padx=8, pady=(2, 8))
        self.btn_run = ttk.Button(row, text="▶ 运行过站流程", command=self._on_run)
        self.btn_run.pack(side="left")
        self.var_model = tk.StringVar(value="（未加载配置）")
        ttk.Label(row, textvariable=self.var_model, foreground="#555").pack(side="left", padx=12)

    def _build_result_area(self):
        # 大判定
        top = ttk.Frame(self.root); top.pack(fill="x", padx=10)
        self.lbl_verdict = tk.Label(top, text="待运行", font=("Segoe UI", 26, "bold"),
                                    fg="#7a7a7a")
        self.lbl_verdict.pack(side="left")
        self.lbl_report = ttk.Label(top, text="", foreground="#555", wraplength=420)
        self.lbl_report.pack(side="left", padx=16)

        # 步骤表
        table = ttk.LabelFrame(self.root, text="步骤结果")
        table.pack(fill="both", expand=False, padx=10, pady=(8, 4))
        cols = ("step", "status", "ms", "message")
        self.tree = ttk.Treeview(table, columns=cols, show="headings", height=7)
        for c, w, t in [("step", 160, "步骤"), ("status", 70, "结果"),
                        ("ms", 70, "耗时(ms)"), ("message", 460, "信息")]:
            self.tree.heading(c, text=t)
            self.tree.column(c, width=w, anchor="center" if c != "message" else "w")
        self.tree.tag_configure("pass", foreground=_STATUS_COLOR["pass"])
        self.tree.tag_configure("fail", foreground=_STATUS_COLOR["fail"])
        self.tree.tag_configure("error", foreground=_STATUS_COLOR["error"])
        self.tree.tag_configure("skip", foreground=_STATUS_COLOR["skip"])
        self.tree.pack(fill="x", padx=6, pady=6)

        # 日志
        logf = ttk.LabelFrame(self.root, text="日志")
        logf.pack(fill="both", expand=True, padx=10, pady=(4, 10))
        self.txt_log = scrolledtext.ScrolledText(logf, height=10, state="disabled",
                                                  font=("Consolas", 9))
        self.txt_log.pack(fill="both", expand=True, padx=6, pady=6)

    # ---------------- 行为 ----------------
    def _browse_config(self):
        path = filedialog.askopenfilename(
            title="选择工装配置", initialdir=os.path.join(_ROOT, "configs"),
            filetypes=[("JSON 配置", "*.json"), ("所有文件", "*.*")])
        if path:
            self.var_config.set(path)
            self._load_default_config()

    def _load_default_config(self):
        try:
            self.cfg = load_config(self.var_config.get())
        except ConfigError as e:
            self.cfg = None
            self.var_model.set("配置加载失败")
            messagebox.showerror("配置错误", str(e))
            return
        self.var_model.set(f"型号：{self.cfg.model}    自检项：{len(self.cfg.tests)}")
        if not self.var_scan.get():
            self._gen_sn()
        self._log(f"已加载配置：{self.var_config.get()}")

    def _gen_sn(self):
        if self.cfg:
            import time as _t
            self.var_scan.set(self.cfg.sn_rule.generate(int(_t.time()) % 100000))

    def _log(self, line: str):
        self.txt_log.configure(state="normal")
        self.txt_log.insert("end", line + "\n")
        self.txt_log.see("end")
        self.txt_log.configure(state="disabled")

    def _on_run(self):
        if self.worker and self.worker.is_alive():
            return
        if self.cfg is None:
            messagebox.showwarning("提示", "请先加载有效的工装配置")
            return
        scan = self.var_scan.get().strip()
        if not scan:
            messagebox.showwarning("提示", "请先扫码或生成示例 SN")
            return

        self.tree.delete(*self.tree.get_children())
        self.lbl_verdict.config(text="运行中…", fg="#b8860b")
        self.lbl_report.config(text="")
        self.btn_run.config(state="disabled")

        cfg = self.cfg
        fault = self.var_fault.get()
        use_mock = self.var_mock.get()
        self.worker = threading.Thread(
            target=self._worker, args=(cfg, scan, fault, use_mock), daemon=True)
        self.worker.start()

    def _worker(self, cfg, scan, fault, use_mock):
        try:
            device = build_device(cfg, use_mock=use_mock, fault=fault)
            result = run_test(cfg, device, scan_code=scan,
                              adapter_name="mock" if use_mock else "hid",
                              log=lambda s: self.q.put(("log", s)))
            self.q.put(("done", result))
        except NotImplementedError as e:
            self.q.put(("error", str(e)))
        except Exception as e:  # 外壳兜底
            self.q.put(("error", f"运行异常：{e!r}"))

    def _poll(self):
        try:
            while True:
                kind, payload = self.q.get_nowait()
                if kind == "log":
                    self._log(payload)
                elif kind == "done":
                    self._fill_result(payload)
                elif kind == "error":
                    self.lbl_verdict.config(text="错误", fg=_STATUS_COLOR["error"])
                    self.btn_run.config(state="normal")
                    messagebox.showerror("运行错误", payload)
        except queue.Empty:
            pass
        self.root.after(100, self._poll)

    def _fill_result(self, result):
        for s in result.steps:
            self.tree.insert("", "end", values=(s.name, s.status.upper(),
                                                s.elapsed_ms, s.message), tags=(s.status,))
        if result.passed:
            self.lbl_verdict.config(text="PASS", fg=_STATUS_COLOR["pass"])
        else:
            self.lbl_verdict.config(text="FAIL", fg=_STATUS_COLOR["fail"])
        self.lbl_report.config(
            text=f"SN：{result.serial or '(空)'}\n总耗时：{result.total_elapsed_ms} ms\n"
                 f"报告：{result.report_path or '(未生成)'}")
        self.btn_run.config(state="normal")


def launch():
    root = tk.Tk()
    FactoryToolApp(root)
    root.mainloop()


if __name__ == "__main__":
    launch()
