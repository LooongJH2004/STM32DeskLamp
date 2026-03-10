#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
物联网智能灯控制面板（Python MQTT客户端）
- GUI: tkinter
- MQTT: paho-mqtt
- 协议: JSON
"""

import json
import queue
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import ttk
from tkinter.scrolledtext import ScrolledText

try:
    import paho.mqtt.client as mqtt
except ImportError as exc:  # pragma: no cover
    raise SystemExit("缺少依赖库 paho-mqtt，请先执行：pip install paho-mqtt") from exc


class LampControlPanel:
    """智能灯控制面板主应用"""

    # 默认配置
    DEFAULT_CONFIG = {
        "broker": "192.168.10.8",
        "port": 1883,
        "client_id": "Python_Control_Panel",
        "topic_ctrl": "device/lamp/ctrl",
        "topic_status": "device/lamp/status",
    }

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("物联网智能灯控制面板")
        self.root.geometry("560x560")
        self.root.minsize(520, 520)
        self.config_path = Path(__file__).with_name("app_config.json")
        self.config = self._load_config()

        # 设备上报更新UI时，抑制本地控件回调发布，避免回环
        self.updating_from_device = False

        # 记录当前控制状态，便于发送部分字段
        self.state = {
            "power": 0,
            "brightness": 50,
            "color_temp": 50,
        }

        # MQTT线程与UI线程之间的消息队列
        self.ui_queue = queue.Queue()

        # Tk变量
        self.conn_text = tk.StringVar(value="MQTT：已断开")
        self.power_text = tk.StringVar(value="开灯")
        self.temp_text = tk.StringVar(value="温度：-- °C")
        self.hum_text = tk.StringVar(value="湿度：-- %")
        self.broker_var = tk.StringVar(value=self.config["broker"])
        self.port_var = tk.StringVar(value=str(self.config["port"]))
        self.client_id_var = tk.StringVar(value=self.config["client_id"])
        self.topic_ctrl_var = tk.StringVar(value=self.config["topic_ctrl"])
        self.topic_status_var = tk.StringVar(value=self.config["topic_status"])

        self.brightness_var = tk.IntVar(value=self.state["brightness"])
        self.color_temp_var = tk.IntVar(value=self.state["color_temp"])

        self._build_ui()
        self._init_mqtt()

        # 轮询处理来自MQTT线程的UI更新任务
        self.root.after(100, self._process_ui_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self) -> None:
        """构建GUI"""
        main = ttk.Frame(self.root, padding=12)
        main.pack(fill=tk.BOTH, expand=True)

        # MQTT配置
        mqtt_cfg_frame = ttk.LabelFrame(main, text="MQTT配置", padding=10)
        mqtt_cfg_frame.pack(fill=tk.X, pady=(0, 10))
        mqtt_cfg_frame.columnconfigure(1, weight=1)
        mqtt_cfg_frame.columnconfigure(3, weight=1)

        ttk.Label(mqtt_cfg_frame, text="代理地址").grid(row=0, column=0, sticky="w")
        ttk.Entry(mqtt_cfg_frame, textvariable=self.broker_var).grid(
            row=0, column=1, sticky="ew", padx=(6, 8)
        )
        ttk.Label(mqtt_cfg_frame, text="端口").grid(row=0, column=2, sticky="w")
        ttk.Entry(mqtt_cfg_frame, textvariable=self.port_var, width=10).grid(
            row=0, column=3, sticky="ew", padx=(6, 0)
        )

        ttk.Label(mqtt_cfg_frame, text="客户端ID").grid(row=1, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.client_id_var).grid(
            row=1, column=1, sticky="ew", padx=(6, 8), pady=(6, 0)
        )
        ttk.Label(mqtt_cfg_frame, text="控制主题").grid(row=1, column=2, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.topic_ctrl_var).grid(
            row=1, column=3, sticky="ew", padx=(6, 0), pady=(6, 0)
        )

        ttk.Label(mqtt_cfg_frame, text="状态主题").grid(row=2, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(mqtt_cfg_frame, textvariable=self.topic_status_var).grid(
            row=2, column=1, sticky="ew", padx=(6, 8), pady=(6, 0)
        )
        ttk.Button(
            mqtt_cfg_frame,
            text="保存并重连",
            command=self.on_save_and_reconnect,
        ).grid(row=2, column=3, sticky="e", pady=(6, 0))

        # 连接状态
        conn_frame = ttk.LabelFrame(main, text="连接状态", padding=10)
        conn_frame.pack(fill=tk.X, pady=(0, 10))
        self.conn_label = ttk.Label(conn_frame, textvariable=self.conn_text)
        self.conn_label.pack(anchor=tk.W)

        # 灯光控制
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
            ctrl_frame, text=f"当前值：{self.brightness_var.get()}"
        )
        self.brightness_value_label.pack(anchor=tk.W, pady=(2, 10))

        # 只在释放滑块时发送，避免拖动时过多消息
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
            ctrl_frame, text=f"当前值：{self.color_temp_var.get()}"
        )
        self.color_temp_value_label.pack(anchor=tk.W, pady=(2, 0))

        self.color_temp_scale.bind("<ButtonRelease-1>", self.on_color_temp_release)
        self.color_temp_scale.bind(
            "<B1-Motion>",
            lambda _e: self.color_temp_value_label.config(
                text=f"当前值：{int(round(self.color_temp_var.get()))}"
            ),
        )

        # 环境监测
        env_frame = ttk.LabelFrame(main, text="环境监测", padding=10)
        env_frame.pack(fill=tk.X, pady=(0, 10))
        ttk.Label(env_frame, textvariable=self.temp_text).pack(anchor=tk.W)
        ttk.Label(env_frame, textvariable=self.hum_text).pack(anchor=tk.W, pady=(4, 0))

        # 日志区域
        log_frame = ttk.LabelFrame(main, text="消息日志", padding=10)
        log_frame.pack(fill=tk.BOTH, expand=True)
        self.log_text = ScrolledText(log_frame, height=12, wrap=tk.WORD)
        self.log_text.pack(fill=tk.BOTH, expand=True)
        self.log_text.configure(state=tk.DISABLED)

        log_btn_row = ttk.Frame(log_frame)
        log_btn_row.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(log_btn_row, text="清空日志", command=self.clear_log).pack(anchor=tk.E)

        # 初始未连接时禁用控制区，连接成功后自动启用
        self._set_controls_enabled(False)

    def _init_mqtt(self) -> None:
        """初始化并连接MQTT客户端"""
        client_id = self.config["client_id"]
        self.client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message

        # 配置自动重连退避
        self.client.reconnect_delay_set(min_delay=1, max_delay=8)

        try:
            self.client.connect(self.config["broker"], int(self.config["port"]), keepalive=60)
            self.client.loop_start()
            self._log(
                f"[系统] 正在连接 MQTT 代理 {self.config['broker']}:{self.config['port']} ..."
            )
        except Exception as exc:
            self._set_conn_status(False)
            self._log(f"[错误] MQTT 连接失败：{exc}")

    def on_connect(self, client, _userdata, _flags, rc):
        """MQTT连接回调（MQTT线程）"""
        if rc == 0:
            client.subscribe(self.config["topic_status"], qos=0)
            self.ui_queue.put(("connected", None))
            self.ui_queue.put(
                ("log", f"[MQTT] 已连接，已订阅：{self.config['topic_status']}")
            )
        else:
            self.ui_queue.put(("disconnected", None))
            self.ui_queue.put(("log", f"[错误] MQTT连接被拒绝，返回码：{rc}"))

    def on_disconnect(self, _client, _userdata, rc):
        """MQTT断开回调（MQTT线程）"""
        self.ui_queue.put(("disconnected", None))
        if rc != 0:
            self.ui_queue.put(("log", "[警告] MQTT异常断开，客户端将自动重连"))
        else:
            self.ui_queue.put(("log", "[MQTT] 已断开连接"))

    def on_message(self, _client, _userdata, msg):
        """接收消息回调（MQTT线程）"""
        payload_text = msg.payload.decode("utf-8", errors="ignore")
        self.ui_queue.put(("log", f"[接收] {msg.topic}: {payload_text}"))

        try:
            data = json.loads(payload_text)
            if isinstance(data, dict):
                self.ui_queue.put(("device_status", data))
            else:
                self.ui_queue.put(("log", "[错误] 状态消息不是JSON对象"))
        except json.JSONDecodeError as exc:
            self.ui_queue.put(("log", f"[错误] 状态JSON解析失败：{exc}"))

    def _process_ui_queue(self) -> None:
        """在主线程处理MQTT线程投递的事件"""
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
            elif event == "device_status":
                self._apply_device_status(payload)

        self.root.after(100, self._process_ui_queue)

    def _set_conn_status(self, connected: bool) -> None:
        """更新连接状态标签"""
        self.conn_text.set("MQTT：已连接" if connected else "MQTT：已断开")
        self._set_controls_enabled(connected)

    def _set_controls_enabled(self, enabled: bool) -> None:
        """根据连接状态启用/禁用灯光控制控件"""
        target_state = tk.NORMAL if enabled else tk.DISABLED
        self.power_button.configure(state=target_state)
        self.brightness_scale.configure(state=target_state)
        self.color_temp_scale.configure(state=target_state)

    def _log(self, text: str) -> None:
        """追加日志到文本框"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, f"[{timestamp}] {text}\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def clear_log(self) -> None:
        """清空日志文本框"""
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.delete("1.0", tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _apply_device_status(self, data: dict) -> None:
        """根据设备上报状态更新UI，不触发二次发布"""
        self.updating_from_device = True
        try:
            if "power" in data:
                self.state["power"] = 1 if int(data["power"]) else 0
                self.power_text.set("关灯" if self.state["power"] else "开灯")

            if "brightness" in data:
                b = max(0, min(100, int(data["brightness"])))
                self.state["brightness"] = b
                self.brightness_var.set(b)
                self.brightness_value_label.config(text=f"当前值：{b}")

            if "color_temp" in data:
                c = max(0, min(100, int(data["color_temp"])))
                self.state["color_temp"] = c
                self.color_temp_var.set(c)
                self.color_temp_value_label.config(text=f"当前值：{c}")

            if "temp" in data:
                self.temp_text.set(f"温度：{data['temp']} °C")

            if "hum" in data:
                self.hum_text.set(f"湿度：{data['hum']} %")
        except (ValueError, TypeError, KeyError) as exc:
            self._log(f"[错误] 处理状态数据失败：{exc}")
        finally:
            self.updating_from_device = False

    def publish_control(self, partial_payload: dict) -> None:
        """发布控制消息（允许部分字段）"""
        if not partial_payload:
            return

        if not self.client.is_connected():
            self._log("[警告] MQTT未连接，控制消息未发送")
            return

        try:
            payload = json.dumps(partial_payload, ensure_ascii=False)
            result = self.client.publish(self.config["topic_ctrl"], payload=payload, qos=0)

            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                self._log(f"[发送] {self.config['topic_ctrl']}: {payload}")
            else:
                self._log(f"[错误] 发布失败，错误码：{result.rc}")
        except Exception as exc:
            self._log(f"[错误] 发布控制消息失败：{exc}")

    def _load_config(self) -> dict:
        """加载配置文件，若不存在则使用默认配置"""
        if not self.config_path.exists():
            self._save_config(self.DEFAULT_CONFIG)
            return dict(self.DEFAULT_CONFIG)

        try:
            with self.config_path.open("r", encoding="utf-8") as f:
                data = json.load(f)
            merged = dict(self.DEFAULT_CONFIG)
            merged.update(data)
            merged["port"] = int(merged["port"])
            return merged
        except Exception:
            # 配置损坏时回退默认值，保证程序可启动
            self._save_config(self.DEFAULT_CONFIG)
            return dict(self.DEFAULT_CONFIG)

    def _save_config(self, cfg: dict) -> None:
        """保存配置到JSON文件"""
        with self.config_path.open("w", encoding="utf-8") as f:
            json.dump(cfg, f, ensure_ascii=False, indent=2)

    def on_save_and_reconnect(self) -> None:
        """保存配置并重连MQTT"""
        try:
            new_cfg = {
                "broker": self.broker_var.get().strip() or self.DEFAULT_CONFIG["broker"],
                "port": int(self.port_var.get().strip()),
                "client_id": self.client_id_var.get().strip() or self.DEFAULT_CONFIG["client_id"],
                "topic_ctrl": self.topic_ctrl_var.get().strip()
                or self.DEFAULT_CONFIG["topic_ctrl"],
                "topic_status": self.topic_status_var.get().strip()
                or self.DEFAULT_CONFIG["topic_status"],
            }
            if new_cfg["port"] <= 0 or new_cfg["port"] > 65535:
                raise ValueError("端口必须在1-65535之间")
        except Exception as exc:
            self._log(f"[错误] 配置校验失败：{exc}")
            return

        self.config = new_cfg
        self._save_config(new_cfg)
        self._log("[系统] 配置已保存，正在重连MQTT...")

        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:
            pass
        self._init_mqtt()

    def on_power_toggle(self) -> None:
        """电源按钮事件"""
        if self.updating_from_device:
            return

        self.state["power"] = 0 if self.state["power"] else 1
        self.power_text.set("关灯" if self.state["power"] else "开灯")
        self.publish_control({"power": self.state["power"]})

    def on_brightness_release(self, _event=None) -> None:
        """亮度滑块释放事件"""
        value = int(round(self.brightness_var.get()))
        self.brightness_var.set(value)
        self.brightness_value_label.config(text=f"当前值：{value}")

        if self.updating_from_device:
            return

        self.state["brightness"] = value
        self.publish_control({"brightness": value})

    def on_color_temp_release(self, _event=None) -> None:
        """色温滑块释放事件"""
        value = int(round(self.color_temp_var.get()))
        self.color_temp_var.set(value)
        self.color_temp_value_label.config(text=f"当前值：{value}")

        if self.updating_from_device:
            return

        self.state["color_temp"] = value
        self.publish_control({"color_temp": value})

    def on_close(self) -> None:
        """窗口关闭时清理MQTT连接"""
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:
            pass
        self.root.destroy()


def main() -> None:
    # 创建根窗口失败时，给出明确报错（极少发生）
    try:
        root = tk.Tk()
    except Exception as exc:  # pragma: no cover
        raise SystemExit(f"GUI初始化失败：{exc}") from exc

    app = LampControlPanel(root)
    app._log("[系统] 控制面板已启动")
    root.mainloop()


if __name__ == "__main__":
    main()
