#pragma once
#include <stdint.h>

// --- 系统状态枚举 (System State Machine) ---
typedef enum {
    SYS_STATE_IDLE = 0,      // 空闲/待机 (显示时间/天气)
    SYS_STATE_LISTENING,     // 聆听中 (VAD工作，录音中)
    SYS_STATE_PROCESSING,    // 处理中 (ASR/LLM 请求中)
    SYS_STATE_SPEAKING,      // 表达中 (TTS 播放中)
    SYS_STATE_ERROR,         // 故障 (网络断开/硬件错误)
    SYS_STATE_OTA,           // 升级中 (禁止一切操作)
    SYS_STATE_DEEP_SLEEP     // 软关机
} SystemState_t;

// --- 事件类型枚举 (Event Topics) ---
typedef enum {
    // 1. 系统级事件
    EVT_SYS_STATE_CHANGE = 0x100, // 状态改变 (参数: 新状态)
    EVT_SYS_ERROR,                // 系统错误 (参数: 错误码)
    
    // 2. 网络事件
    EVT_NET_CONNECTED = 0x200,    // Wi-Fi 已连接
    EVT_NET_DISCONNECTED,         // Wi-Fi 断开
    EVT_TIME_SYNCED,              // SNTP 时间已同步

    // 3. 交互事件 (输入)
    EVT_KEY_CLICK = 0x300,        // 按键单击
    EVT_KEY_DOUBLE_CLICK,         // 按键双击
    EVT_KEY_LONG_PRESS,           // 按键长按
    EVT_TOUCH_GESTURE,            // 触摸手势 (参数: 方向/类型)
    
    // 4. 语音事件 (Audio Pipeline)
    EVT_AUDIO_VAD_START = 0x400,  // 检测到人声开始
    EVT_AUDIO_VAD_STOP,           // 检测到人声结束 (静音超时)
    EVT_ASR_RESULT,               // ASR 识别结果 (参数: 字符串指针，需释放)
    EVT_LLM_RESULT,               // LLM 回复内容 (参数: 字符串指针)
    EVT_TTS_PLAY_START,           // TTS 开始播放
    EVT_TTS_PLAY_FINISH,          // TTS 播放结束

    // 5. 设备控制 (Output)
    EVT_LIGHT_SET_COLOR = 0x500,  // 设置灯光颜色 (参数: RGB/CCT)
    EVT_LIGHT_SET_BRIGHTNESS,     // 设置亮度 (参数: 0-100)

    // 6. 数据中心变更事件 (Data Center Updates) [新增]
    EVT_DATA_LIGHT_CHANGED = 0x600, // 灯光数据已更新
    EVT_DATA_ENV_CHANGED,           // 环境数据已更新 (温湿度/天气)
    EVT_DATA_TIMER_CHANGED,         // 定时器数据已更新
    EVT_DATA_SYS_CHANGED            // 系统状态已更新
} EventType_t;

// --- 事件结构体 ---
typedef struct {
    EventType_t type;   // 事件类型
    void *data;         // 负载数据 (指针，可选)
    int len;            // 数据长度 (可选)
    uint32_t timestamp; // 时间戳
} SystemEvent_t;
