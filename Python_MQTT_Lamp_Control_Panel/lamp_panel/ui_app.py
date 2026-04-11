from __future__ import annotations

import queue
from datetime import datetime
import tkinter as tk
from tkinter import ttk
from tkinter.scrolledtext import ScrolledText

from .config_manager import AppConfig, ConfigManager, MqttProfile
from .mqtt_service import MqttService


class LampControlPanelApp:
    """灯控 GUI 应用。

    职责说明：
    1) 维护 UI 状态和交互。
    2) 调用 ConfigManager 读写配置档案。
    3) 调用 MqttService 收发 MQTT。
    """

    def __init__(
        self,
        root: tk.Tk,
        config_manager: ConfigManager,
        profile_override: str | None = None,
    ) -> None:
        self.root = root
        self.root.title("物联网智能灯控制面板")
        self.root.geometry("620x620")
        self.root.minsize(560, 560)

        self.config_manager = config_manager
        self.config: AppConfig = self.config_manager.load(profile_override=profile_override)

        # 设备上报更新控件时的保护标记，避免触发回环发布。
        self.updating_from_device = False

        # 当前灯具状态缓存。
        self.state = {"power": 0, "brightness": 50, "color_temp": 50}

        # MQTT 线程 -> UI 线程事件队列。
        self.ui_queue: "queue.Queue[tuple[str, object]]" = queue.Queue()
        self.mqtt_service = MqttService(self.ui_queue)

        # ---------- Tk 变量 ----------
        self.conn_text = tk.StringVar(value="MQTT：已断开")
        self.power_text = tk.StringVar(value="开灯")
        self.temp_text = tk.StringVar(value="温度：-- °C")
        self.hum_text = tk.StringVar(value="湿度：-- %")

        self.active_profile_var = tk.StringVar(value=self.config.active_profile)
        self.broker_var = tk.StringVar()
        self.port_var = tk.StringVar()
        self.client_id_var = tk.StringVar()
        self.topic_ctrl_var = tk.StringVar(value=self.config.topic_ctrl)
        self.topic_status_var = tk.StringVar(value=self.config.topic_status)

        self.brightness_var = tk.IntVar(value=self.state["brightness"])
        self.color_temp_var = tk.IntVar(value=self.state["color_temp"])

        # 把当前档案的 broker/port/client_id 写入输入框。
        self._load_profile_to_form(self.config.active_profile)

        self._build_ui()
        self._connect_current_profile()

        self.root.after(100, self._process_ui_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=12)
        main.pack(fill=tk.BOTH, expand=True)

        mqtt_cfg_frame = ttk.LabelFrame(main, text="MQTT配置", padding=10)
        mqtt_cfg_frame.pack(fill=tk.X, pady=(0, 10))
        mqtt_cfg_frame.columnconfigure(1, weight=1)
        mqtt_cfg_frame.columnconfigure(3, weight=1)

        ttk.Label(mqtt_cfg_frame, text="配置档案").grid(row=0, column=0, sticky="w")
        self.profile_combo = ttk.Combobox(
            mqtt_cfg_frame,
            textvariable=self.active_profile_var,
            state="readonly",
            values=list(self.config.profiles.keys()),
        )
        self.profile_combo.grid(row=0, column=1, sticky="ew", padx=(6, 8))
        ttk.Button(mqtt_cfg_frame, text="加载档案", command=self.on_load_profile).grid(
            row=0, column=2, sticky="w"
        )
        ttk.Button(mqtt_cfg_frame, text="保存并重连", command=self.on_save_and_reconnect).grid(
            row=0, column=3, sticky="e"
        )

        ttk.Label(mqtt_cfg_frame, text="代理地址").grid(row=1, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.broker_var).grid(
            row=1, column=1, sticky="ew", padx=(6, 8), pady=(6, 0)
        )
        ttk.Label(mqtt_cfg_frame, text="端口").grid(row=1, column=2, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.port_var, width=10).grid(
            row=1, column=3, sticky="ew", padx=(6, 0), pady=(6, 0)
        )

        ttk.Label(mqtt_cfg_frame, text="客户端ID").grid(row=2, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.client_id_var).grid(
            row=2, column=1, sticky="ew", padx=(6, 8), pady=(6, 0)
        )
        ttk.Label(mqtt_cfg_frame, text="控制主题").grid(row=2, column=2, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.topic_ctrl_var).grid(
            row=2, column=3, sticky="ew", padx=(6, 0), pady=(6, 0)
        )

        ttk.Label(mqtt_cfg_frame, text="状态主题").grid(row=3, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.topic_status_var).grid(
            row=3, column=1, sticky="ew", padx=(6, 8), pady=(6, 0)
        )

        conn_frame = ttk.LabelFrame(main, text="连接状态", padding=10)
        conn_frame.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(conn_frame, textvariable=self.conn_text).pack(anchor=tk.W)

        ctrl_frame = ttk.LabelFrame(main, text="灯光控制", padding=10)
        ctrl_frame.pack(fill=tk.X, pady=(0, 10))

        self.power_button = ttk.Button(
            ctrl_frame,
            textvariable=self.power_text,
            command=self.on_power_toggle,
            width=10,
        )
        self.power_button.pack(anchor=tk.W, pady=(0, 12))

        ttk.Label(ctrl_frame, text="亮度（0-100）").pack(anchor=tk.W)
        self.brightness_scale = ttk.Scale(
            ctrl_frame,
            from_=0,
            to=100,
            orient=tk.HORIZONTAL,
            variable=self.brightness_var,
        )
        self.brightness_scale.pack(fill=tk.X)
        self.brightness_value_label = ttk.Label(
            ctrl_frame,
            text=f"当前值：{self.brightness_var.get()}",
        )
        self.brightness_value_label.pack(anchor=tk.W, pady=(2, 10))

        self.brightness_scale.bind("<ButtonRelease-1>", self.on_brightness_release)
        self.brightness_scale.bind(
            "<B1-Motion>",
            lambda _e: self.brightness_value_label.config(
                text=f"当前值：{int(round(self.brightness_var.get()))}"
            ),
        )

        ttk.Label(ctrl_frame, text="色温（暖 0 <-> 100 冷）").pack(anchor=tk.W)
        self.color_temp_scale = ttk.Scale(
            ctrl_frame,
            from_=0,
            to=100,
            orient=tk.HORIZONTAL,
            variable=self.color_temp_var,
        )
        self.color_temp_scale.pack(fill=tk.X)
        self.color_temp_value_label = ttk.Label(
            ctrl_frame,
            text=f"当前值：{self.color_temp_var.get()}",
        )
        self.color_temp_value_label.pack(anchor=tk.W, pady=(2, 0))

        self.color_temp_scale.bind("<ButtonRelease-1>", self.on_color_temp_release)
        self.color_temp_scale.bind(
            "<B1-Motion>",
            lambda _e: self.color_temp_value_label.config(
                text=f"当前值：{int(round(self.color_temp_var.get()))}"
            ),
        )

        env_frame = ttk.LabelFrame(main, text="环境监测", padding=10)
        env_frame.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(env_frame, textvariable=self.temp_text).pack(anchor=tk.W)
        ttk.Label(env_frame, textvariable=self.hum_text).pack(anchor=tk.W, pady=(4, 0))

        log_frame = ttk.LabelFrame(main, text="消息日志", padding=10)
        log_frame.pack(fill=tk.BOTH, expand=True)
        self.log_text = ScrolledText(log_frame, height=12, wrap=tk.WORD)
        self.log_text.pack(fill=tk.BOTH, expand=True)
        self.log_text.configure(state=tk.DISABLED)

        log_btn_row = ttk.Frame(log_frame)
        log_btn_row.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(log_btn_row, text="清空日志", command=self.clear_log).pack(anchor=tk.E)

        self._set_controls_enabled(False)

    # ---------------- 配置档案 ----------------

    def _load_profile_to_form(self, profile_name: str) -> None:
        """将指定档案加载到输入框。"""
        profile = self.config.profiles.get(profile_name)
        if not profile:
            return
        self.active_profile_var.set(profile_name)
        self.broker_var.set(profile.broker)
        self.port_var.set(str(profile.port))
        self.client_id_var.set(profile.client_id)

    def _collect_profile_from_form(self) -> MqttProfile:
        broker = self.broker_var.get().strip()
        client_id = self.client_id_var.get().strip()
        if not broker:
            raise ValueError("代理地址不能为空")
        if not client_id:
            raise ValueError("客户端ID不能为空")

        port_text = self.port_var.get().strip()
        port = int(port_text)
        if port <= 0 or port > 65535:
            raise ValueError("端口必须在1-65535之间")

        return MqttProfile(broker=broker, port=port, client_id=client_id)

    def on_load_profile(self) -> None:
        """点击“加载档案”后，把档案值填入输入框。"""
        selected = self.active_profile_var.get().strip()
        if selected not in self.config.profiles:
            self._log(f"[错误] 档案不存在：{selected}")
            return
        self._load_profile_to_form(selected)
        self._log(f"[系统] 已加载配置档案：{selected}")

    def on_save_and_reconnect(self) -> None:
        """保存当前表单到档案并重连 MQTT。"""
        try:
            profile_name = self.active_profile_var.get().strip() or self.config.active_profile
            if not profile_name:
                raise ValueError("配置档案名不能为空")
            profile = self._collect_profile_from_form()

            topic_ctrl = self.topic_ctrl_var.get().strip() or "device/lamp/ctrl"
            topic_status = self.topic_status_var.get().strip() or "device/lamp/status"
        except Exception as exc:
            self._log(f"[错误] 配置校验失败：{exc}")
            return

        self.config.profiles[profile_name] = profile
        self.config.active_profile = profile_name
        self.config.topic_ctrl = topic_ctrl
        self.config.topic_status = topic_status
        self.config_manager.save(self.config)

        # 下拉框可能新增/变化，刷新显示。
        self.profile_combo.configure(values=list(self.config.profiles.keys()))
        self._log(f"[系统] 配置已保存（档案：{profile_name}），正在重连 MQTT...")
        self._connect_current_profile()

    # ---------------- MQTT 生命周期 ----------------

    def _connect_current_profile(self) -> None:
        profile = self.config.profiles[self.config.active_profile]
        self.mqtt_service.connect(profile=profile, topic_status=self.config.topic_status)

    def _process_ui_queue(self) -> None:
        while True:
            try:
                event, payload = self.ui_queue.get_nowait()
            except queue.Empty:
                break

            if event == "connected":
                self._set_conn_status(True)
            elif event == "disconnected":
                self._set_conn_status(False)
            elif event == "log":
                self._log(str(payload))
            elif event == "device_status" and isinstance(payload, dict):
                self._apply_device_status(payload)

        self.root.after(100, self._process_ui_queue)

    def _set_conn_status(self, connected: bool) -> None:
        self.conn_text.set("MQTT：已连接" if connected else "MQTT：已断开")
        self._set_controls_enabled(connected)

    def _set_controls_enabled(self, enabled: bool) -> None:
        state = tk.NORMAL if enabled else tk.DISABLED
        self.power_button.configure(state=state)
        self.brightness_scale.configure(state=state)
        self.color_temp_scale.configure(state=state)

    # ---------------- 日志与状态更新 ----------------

    def _log(self, text: str) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, f"[{timestamp}] {text}\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def clear_log(self) -> None:
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _apply_device_status(self, data: dict) -> None:
        self.updating_from_device = True
        try:
            if "power" in data:
                self.state["power"] = 1 if int(data["power"]) else 0
                self.power_text.set("关灯" if self.state["power"] else "开灯")

            if "brightness" in data:
                brightness = max(0, min(100, int(data["brightness"])))
                self.state["brightness"] = brightness
                self.brightness_var.set(brightness)
                self.brightness_value_label.config(text=f"当前值：{brightness}")

            if "color_temp" in data:
                color_temp = max(0, min(100, int(data["color_temp"])))
                self.state["color_temp"] = color_temp
                self.color_temp_var.set(color_temp)
                self.color_temp_value_label.config(text=f"当前值：{color_temp}")

            if "temp" in data:
                self.temp_text.set(f"温度：{data['temp']} °C")
            if "hum" in data:
                self.hum_text.set(f"湿度：{data['hum']} %")
        except (ValueError, TypeError, KeyError) as exc:
            self._log(f"[错误] 处理状态数据失败：{exc}")
        finally:
            self.updating_from_device = False

    # ---------------- 控件事件 ----------------

    def _publish_control(self, partial_payload: dict) -> None:
        if not partial_payload:
            return
        self.mqtt_service.publish_json(self.config.topic_ctrl, partial_payload)

    def on_power_toggle(self) -> None:
        if self.updating_from_device:
            return
        self.state["power"] = 0 if self.state["power"] else 1
        self.power_text.set("关灯" if self.state["power"] else "开灯")
        self._publish_control({"power": self.state["power"]})

    def on_brightness_release(self, _event=None) -> None:
        value = int(round(self.brightness_var.get()))
        self.brightness_var.set(value)
        self.brightness_value_label.config(text=f"当前值：{value}")
        if self.updating_from_device:
            return
        self.state["brightness"] = value
        self._publish_control({"brightness": value})

    def on_color_temp_release(self, _event=None) -> None:
        value = int(round(self.color_temp_var.get()))
        self.color_temp_var.set(value)
        self.color_temp_value_label.config(text=f"当前值：{value}")
        if self.updating_from_device:
            return
        self.state["color_temp"] = value
        self._publish_control({"color_temp": value})

    def on_close(self) -> None:
        self.mqtt_service.disconnect()
        self.root.destroy()
