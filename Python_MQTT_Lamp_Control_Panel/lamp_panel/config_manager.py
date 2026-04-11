from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict


@dataclass
class MqttProfile:
    """单个 MQTT 主机配置档案。"""

    broker: str
    port: int
    client_id: str


@dataclass
class AppConfig:
    """应用配置模型。

    profiles: 可维护多套主机配置，便于在台式机/笔记本之间切换。
    active_profile: 当前启用的档案名。
    """

    active_profile: str
    profiles: Dict[str, MqttProfile]
    topic_ctrl: str
    topic_status: str


class ConfigManager:
    """负责配置文件加载、迁移、保存。"""

    DEFAULT_PROFILE_NAME = "desktop_local"

    DEFAULT_PROFILES = {
        # 同机部署 Broker 的推荐配置：不依赖网卡 IP 变化。
        "desktop_local": MqttProfile(
            broker="127.0.0.1",
            port=1883,
            client_id="Python_Control_Panel_PC",
        ),
        # 毕业演示常用配置：保留原笔记本 LAN IP。
        "laptop_demo": MqttProfile(
            broker="192.168.10.8",
            port=1883,
            client_id="Python_Control_Panel_Laptop",
        ),
    }

    DEFAULT_TOPIC_CTRL = "device/lamp/ctrl"
    DEFAULT_TOPIC_STATUS = "device/lamp/status"

    def __init__(self, config_path: Path) -> None:
        self.config_path = config_path

    def load(self, profile_override: str | None = None) -> AppConfig:
        """读取配置；若是旧版单配置结构则自动迁移。"""
        if not self.config_path.exists():
            cfg = self._build_default_config()
            self.save(cfg)
            return cfg

        try:
            with self.config_path.open("r", encoding="utf-8") as f:
                raw = json.load(f)
        except Exception:
            cfg = self._build_default_config()
            self.save(cfg)
            return cfg

        cfg = self._normalize(raw)
        if profile_override and profile_override in cfg.profiles:
            cfg.active_profile = profile_override
        self.save(cfg)
        return cfg

    def save(self, cfg: AppConfig) -> None:
        """将内存配置落盘为 JSON。"""
        data = {
            "active_profile": cfg.active_profile,
            "profiles": {
                name: {
                    "broker": profile.broker,
                    "port": int(profile.port),
                    "client_id": profile.client_id,
                }
                for name, profile in cfg.profiles.items()
            },
            "topic_ctrl": cfg.topic_ctrl,
            "topic_status": cfg.topic_status,
        }
        with self.config_path.open("w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)

    def _build_default_config(self) -> AppConfig:
        profiles = {
            name: MqttProfile(p.broker, int(p.port), p.client_id)
            for name, p in self.DEFAULT_PROFILES.items()
        }
        return AppConfig(
            active_profile=self.DEFAULT_PROFILE_NAME,
            profiles=profiles,
            topic_ctrl=self.DEFAULT_TOPIC_CTRL,
            topic_status=self.DEFAULT_TOPIC_STATUS,
        )

    def _normalize(self, raw: dict) -> AppConfig:
        """把磁盘 JSON 归一化为 AppConfig。

        支持两类结构：
        1) 新结构：active_profile + profiles
        2) 旧结构：broker/port/client_id 平铺字段
        """
        topic_ctrl = str(raw.get("topic_ctrl", self.DEFAULT_TOPIC_CTRL)).strip() or self.DEFAULT_TOPIC_CTRL
        topic_status = str(raw.get("topic_status", self.DEFAULT_TOPIC_STATUS)).strip() or self.DEFAULT_TOPIC_STATUS

        profiles: Dict[str, MqttProfile] = {}

        raw_profiles = raw.get("profiles")
        if isinstance(raw_profiles, dict):
            for name, item in raw_profiles.items():
                if not isinstance(item, dict):
                    continue
                broker = str(item.get("broker", "")).strip()
                client_id = str(item.get("client_id", "")).strip()
                port = self._safe_port(item.get("port"), default=1883)
                if broker and client_id:
                    profiles[str(name)] = MqttProfile(broker=broker, port=port, client_id=client_id)

        # 兼容旧版配置：自动生成 custom_migrated 档案。
        if not profiles and "broker" in raw:
            broker = str(raw.get("broker", "")).strip()
            client_id = str(raw.get("client_id", "Python_Control_Panel")).strip() or "Python_Control_Panel"
            port = self._safe_port(raw.get("port"), default=1883)
            if broker:
                profiles["custom_migrated"] = MqttProfile(
                    broker=broker,
                    port=port,
                    client_id=client_id,
                )

        # 若配置无效，补齐默认档案，保证程序可启动。
        for name, default_profile in self.DEFAULT_PROFILES.items():
            profiles.setdefault(
                name,
                MqttProfile(default_profile.broker, default_profile.port, default_profile.client_id),
            )

        active_profile = str(raw.get("active_profile", "")).strip()
        if active_profile not in profiles:
            active_profile = (
                "custom_migrated"
                if "custom_migrated" in profiles
                else self.DEFAULT_PROFILE_NAME
            )

        return AppConfig(
            active_profile=active_profile,
            profiles=profiles,
            topic_ctrl=topic_ctrl,
            topic_status=topic_status,
        )

    @staticmethod
    def _safe_port(value: object, default: int) -> int:
        try:
            port = int(value)
            if 1 <= port <= 65535:
                return port
        except Exception:
            pass
        return default
