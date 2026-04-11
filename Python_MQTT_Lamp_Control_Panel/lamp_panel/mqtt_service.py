from __future__ import annotations

import json
import queue
from typing import Any

try:
    import paho.mqtt.client as mqtt
except ImportError as exc:  # pragma: no cover
    raise SystemExit("缺少依赖库 paho-mqtt，请先执行：pip install paho-mqtt") from exc

from .config_manager import MqttProfile


class MqttService:
    """MQTT 通信层。

    通过事件队列把消息投递到 UI 线程，避免跨线程直接操作 tkinter。
    """

    def __init__(self, ui_queue: "queue.Queue[tuple[str, Any]]") -> None:
        self.ui_queue = ui_queue
        self.client: mqtt.Client | None = None
        self.topic_status = ""

    def connect(self, profile: MqttProfile, topic_status: str) -> None:
        """按给定档案连接 broker，并启动网络循环线程。"""
        self.disconnect()
        self.topic_status = topic_status

        self.client = mqtt.Client(client_id=profile.client_id, protocol=mqtt.MQTTv311)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        self.client.reconnect_delay_set(min_delay=1, max_delay=8)

        try:
            self.client.connect(profile.broker, int(profile.port), keepalive=60)
            self.client.loop_start()
            self.ui_queue.put(("log", f"[系统] 正在连接 MQTT 代理 {profile.broker}:{profile.port} ..."))
        except Exception as exc:
            self.ui_queue.put(("disconnected", None))
            self.ui_queue.put(("log", f"[错误] MQTT 连接失败：{exc}"))
            self.ui_queue.put(
                (
                    "log",
                    "[提示] 若 Mosquitto 与本程序在同一台电脑，请将代理地址设为 127.0.0.1 或 localhost。",
                )
            )

    def disconnect(self) -> None:
        """主动断开连接并停止循环线程。"""
        if not self.client:
            return
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:
            pass

    def is_connected(self) -> bool:
        return bool(self.client and self.client.is_connected())

    def publish_json(self, topic: str, payload: dict) -> bool:
        """发布 JSON 消息；成功返回 True。"""
        if not self.client or not self.client.is_connected():
            self.ui_queue.put(("log", "[警告] MQTT未连接，控制消息未发送"))
            return False

        try:
            text = json.dumps(payload, ensure_ascii=False)
            result = self.client.publish(topic, payload=text, qos=0)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                self.ui_queue.put(("log", f"[发送] {topic}: {text}"))
                return True
            self.ui_queue.put(("log", f"[错误] 发布失败，错误码：{result.rc}"))
            return False
        except Exception as exc:
            self.ui_queue.put(("log", f"[错误] 发布控制消息失败：{exc}"))
            return False

    # ---------------- 回调：运行在 MQTT 线程 ----------------

    def _on_connect(self, client: mqtt.Client, _userdata, _flags, rc):
        if rc == 0:
            client.subscribe(self.topic_status, qos=0)
            self.ui_queue.put(("connected", None))
            self.ui_queue.put(("log", f"[MQTT] 已连接，已订阅：{self.topic_status}"))
        else:
            self.ui_queue.put(("disconnected", None))
            self.ui_queue.put(("log", f"[错误] MQTT连接被拒绝，返回码：{rc}"))

    def _on_disconnect(self, _client: mqtt.Client, _userdata, rc):
        self.ui_queue.put(("disconnected", None))
        if rc != 0:
            self.ui_queue.put(("log", "[警告] MQTT异常断开，客户端将自动重连"))
        else:
            self.ui_queue.put(("log", "[MQTT] 已断开连接"))

    def _on_message(self, _client: mqtt.Client, _userdata, msg):
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
