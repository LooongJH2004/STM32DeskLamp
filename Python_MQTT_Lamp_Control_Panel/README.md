# 物联网智能灯控制面板（Python MQTT 客户端）

## 1. 功能说明
- 通过 MQTT 控制 ESP32 智能灯的电源、亮度、色温。
- 实时接收并显示设备上报的温度、湿度。
- 支持消息日志查看（发送/接收 JSON）。
- 设备状态回写 UI 时抑制回环发布，避免无限循环。
- 支持 GUI 内修改 MQTT 参数并保存到本地配置文件，点击后自动重连。

## 2. 运行环境
- Python 3.8+
- MQTT Broker（默认 `192.168.10.8:1883`）

## 3. 安装依赖
```bash
pip install -r requirements.txt
```

## 4. 启动方式
```bash
python lamp_control_panel.py
```

首次启动会在同目录自动生成 `app_config.json`。

## 5. MQTT 协议
### 控制主题（发布）
- 主题：`device/lamp/ctrl`
- 方向：`Python -> ESP32`
- 示例：
```json
{
  "power": 1,
  "brightness": 80,
  "color_temp": 50
}
```

### 状态主题（订阅）
- 主题：`device/lamp/status`
- 方向：`ESP32 -> Python`
- 示例：
```json
{
  "power": 1,
  "brightness": 80,
  "color_temp": 50,
  "temp": 25,
  "hum": 60
}
```

## 6. 配置文件
配置文件路径：`app_config.json`

示例：
```json
{
  "broker": "192.168.10.8",
  "port": 1883,
  "client_id": "Python_Control_Panel",
  "topic_ctrl": "device/lamp/ctrl",
  "topic_status": "device/lamp/status"
}
```

你可以通过 GUI 的“MQTT配置”区域修改并点击“保存并重连”。

## 7. Windows 打包 EXE
在项目目录执行：
```bat
build_exe.bat
```

打包完成后可执行文件位于：`dist\智能灯控制面板.exe`
