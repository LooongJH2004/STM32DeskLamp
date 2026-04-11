# 物联网智能灯控制面板（Python MQTT 客户端）

## 1. 功能说明
- 通过 MQTT 控制 ESP32 智能灯的电源、亮度、色温。
- 实时接收并显示设备上报的温度、湿度。
- 支持消息日志查看（发送/接收 JSON）。
- 支持多主机配置档案（台式机开发/笔记本演示）快速切换。

## 2. 项目结构（已模块化）
```text
lamp_control_panel.py           # 程序入口（参数解析 + 启动）
lamp_panel/
  config_manager.py             # 配置加载/迁移/保存
  mqtt_service.py               # MQTT 通信层
  ui_app.py                     # tkinter 界面和交互
app_config.json                 # 配置文件（支持 profiles）
start.bat                       # 一键启动（可传档案名）
start_desktop.bat              # 一键启动：台式机档案
start_laptop.bat               # 一键启动：笔记本演示档案
```

## 3. 运行环境
- Python 3.8+
- MQTT Broker（默认 `127.0.0.1:1883`）

## 4. 安装依赖
```bash
pip install -r requirements.txt
```

## 5. 启动方式
- 通用启动（可指定档案）
```bash
python lamp_control_panel.py --profile desktop_local
python lamp_control_panel.py --profile laptop_demo
```
- Windows 一键启动
```bat
start_desktop.bat
start_laptop.bat
```
或
```bat
start.bat desktop_local
start.bat laptop_demo
```

## 6. 配置文件说明
配置文件路径：`app_config.json`

示例：
```json
{
  "active_profile": "desktop_local",
  "profiles": {
    "desktop_local": {
      "broker": "127.0.0.1",
      "port": 1883,
      "client_id": "Python_Control_Panel_PC"
    },
    "laptop_demo": {
      "broker": "192.168.10.8",
      "port": 1883,
      "client_id": "Python_Control_Panel_Laptop"
    }
  },
  "topic_ctrl": "device/lamp/ctrl",
  "topic_status": "device/lamp/status"
}
```

说明：
- `desktop_local` 适合在当前主机本机运行 Mosquitto。
- `laptop_demo` 预留给毕业演示笔记本环境。
- 旧版单配置格式会在启动时自动迁移为新结构。

## 7. MQTT 协议
### 控制主题（发布）
- 主题：`device/lamp/ctrl`
- 方向：`Python -> ESP32`

### 状态主题（订阅）
- 主题：`device/lamp/status`
- 方向：`ESP32 -> Python`

## 8. 主机更换排查建议
- 控制面板和 Broker 同机时，优先用 `127.0.0.1`。
- 如果 ESP32 通过局域网连接 Broker，请确保 broker 监听 LAN（如 `listener 1883 0.0.0.0`）并放行防火墙 1883。

## 9. Windows 打包 EXE
在项目目录执行：
```bat
build_exe.bat
```

打包完成后可执行文件位于：`dist\智能灯控制面板.exe`
