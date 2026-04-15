# 智能台灯系统测试指令集 (Test Command Set v1.0)

## 1. 串口配置指令 (PC -> ESP32)
这些指令通过 PC 串口助手发送给 ESP32，用于初始化网络或切换运行模式。

| 指令名称 | 指令字符串 | 功能描述 | 预期响应 |
| :--- | :--- | :--- | :--- |
| **设置SSID** | `wifiname <SSID>` | 配置目标 Wi-Fi 的名称 | `I (xxx) Mgr_Wifi: WiFi SSID set to: <SSID>` |
| **设置密码** | `wifipassword <PWD>` | 配置目标 Wi-Fi 的密码 | `I (xxx) Mgr_Wifi: WiFi Password set.` |
| **触发连接** | `wificonnect` | 使用已设定的信息发起连接 | `I (xxx) Mgr_Wifi: Connecting to SSID...` |
| **CSI模式1** | `csi1` | 切换至：归一化轮廓绝对差值法 | `W (xxx) Dev_CSI: >>> Switched to Mode 1 <<<` |
| **CSI模式2** | `csi2` | 切换至：宏观总振幅极差法 | `W (xxx) Dev_CSI: >>> Switched to Mode 2 <<<` |
| **CSI模式3** | `csi3` | 切换至：平方MSE+截尾均值滤波 | `W (xxx) Dev_CSI: >>> Switched to Mode 3 <<<` |
| **正常CRC** | `crc0` | 恢复正常的 CRC16 发送策略 | `W (xxx) Dev_STM32: >>> Switched to CRC Mode: RIGHT <<<` |
| **错误注入** | `crc1` | 开启 CRC 错误注入（用于拦截测试） | `W (xxx) Dev_STM32: >>> Switched to CRC Mode: ERROR <<<` |

---

## 2. 跨芯片控制指令 (ESP32 -> STM32)
这些是系统内部 UART 传输的 JSON 帧，用于验证双 MCU 通信协议。

| 功能模块 | JSON 负载示例 | 协议后缀 | 说明 |
| :--- | :--- | :--- | :--- |
| **灯光调节** | `{"cmd":"light","warm":500,"cold":200}` | `\|<CRC16>\r\n` | 设置暖光 50% 亮度，冷光 20% 亮度 |
| **模式切换** | `{"cmd":"mode","val":1}` | `\|<CRC16>\r\n` | 切换 STM32 为 Remote UI 模式 |
| **全关指令** | `{"cmd":"light","warm":0,"cold":0}` | `\|<CRC16>\r\n` | 熄灭所有灯珠 |

---

## 3. 状态上报指令 (STM32 -> ESP32)
用于验证数据中心（DataCenter）的同步能力。

| 事件类型 | JSON 负载示例 | 协议后缀 | 说明 |
| :--- | :--- | :--- | :--- |
| **环境数据** | `{"ev":"env","t":26,"h":55,"l":400}` | `\|<CRC16>\r\n` | 上报温度 26℃，湿度 55%，光强 400lux |
| **编码器交互** | `{"ev":"enc","diff":-15}` | `\|<CRC16>\r\n` | 上报编码器反转 15 个单位增量 |
| **状态对齐** | `{"ev":"state","warm":100,"cold":100}` | `\|<CRC16>\r\n` | STM32 反馈当前物理执行的 PWM 状态 |

---

## 4. 自动化测试脚本数据抓取特征
用于 Python 脚本解析的日志特征字符串（Regex Keywords）。

*   **7.1.2 拦截测试**：搜索关键字 `[TX_ERROR]`。
    *   特征：`W (时间戳) Dev_STM32: [TX_ERROR] {JSON}|CRC|FFFF`
*   **7.1.3 吞吐量测试**：搜索关键字 `[RX]` 且包含 `ev":"enc"`。
    *   特征：`I (时间戳) Dev_STM32: [RX] {"ev":"enc",...}|CRC|0000`
*   **数据中心校验**：搜索关键字 `=== Data Center Status ===`。
    *   特征：由 `DataCenter_PrintStatus()` 周期性打印。

---

## 5. 测试注意事项
1.  **波特率**：所有串口通信均固定为 `115200 bps`。
2.  **换行符**：所有指令必须以 `\r\n` (CRLF) 结尾，否则滑动窗口算法无法判定帧结束。
3.  **CRC计算**：手动发送指令测试时，若 STM32 开启了校验，请务必计算正确的 CRC16-CCITT 码并拼接在 `|` 之后。
4.  **共地**：确保 PC、ESP32、STM32 三者 GND 连通，防止电平漂移导致误码。

---