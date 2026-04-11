#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""物联网智能灯控制面板启动入口。"""

from __future__ import annotations

import argparse
from pathlib import Path
import tkinter as tk

from lamp_panel.config_manager import ConfigManager
from lamp_panel.ui_app import LampControlPanelApp


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="智能灯 MQTT 控制面板")
    parser.add_argument(
        "--profile",
        default=None,
        help="启动时指定配置档案名（例如 desktop_local 或 laptop_demo）",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config_path = Path(__file__).with_name("app_config.json")

    try:
        root = tk.Tk()
    except Exception as exc:  # pragma: no cover
        raise SystemExit(f"GUI初始化失败：{exc}") from exc

    manager = ConfigManager(config_path)
    app = LampControlPanelApp(root=root, config_manager=manager, profile_override=args.profile)
    app._log("[系统] 控制面板已启动")
    if args.profile:
        app._log(f"[系统] 启动参数指定档案：{args.profile}")
    root.mainloop()


if __name__ == "__main__":
    main()
