#pragma once
#include <stdbool.h>
#include "esp_err.h"

// --- 配置宏 (请修改为你自己的) ---
#define BAIDU_API_KEY       "w3Jwped6RKLCjRecVZN2qedh"
#define BAIDU_SECRET_KEY    "IeOjLgwKyyr9ixv3ldD9OVJafazxaRq0"

// 百度 ASR 接口地址 (HTTP 速度更快，且音频数据不敏感)
#define BAIDU_ASR_URL       "http://vop.baidu.com/server_api"
#define BAIDU_TOKEN_URL     "https://aip.baidubce.com/oauth/2.0/token"

/**
 * @brief 初始化 ASR 代理 (获取 Token)
 */
void Agent_ASR_Init(void);

/**
 * @brief 开始一次语音识别会话 (作为独立任务运行)
 * 
 * @note 此函数是阻塞的，建议通过 xTaskCreate 调用。
 *       识别结果将通过 EventBus 发送 EVT_ASR_RESULT 事件。
 *       任务执行完毕后会自动删除自身。
 * 
 * @param pvParameters 传入最大录音时长 (int)，单位 ms。例如 (void*)5000
 */
void Agent_ASR_Run_Session(void *pvParameters);

/**
 * @brief 强制停止当前的录音会话 (用于按键松开或 VAD 截断)
 */
void Agent_ASR_Stop(void);