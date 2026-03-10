#pragma once
#include <stdbool.h>

/**
 * @brief 初始化并启动 MQTT 客户端
 * @note  应在 Wi-Fi 连接成功后调用
 */
void Agent_MQTT_Init(void);

/**
 * @brief 停止 MQTT 客户端
 */
void Agent_MQTT_Stop(void);

/**
 * @brief 主动发布当前设备状态 (Lighting + Env) 到 MQTT_TOPIC_STATUS
 * @note  通常在数据中心发生变化时调用
 */
void Agent_MQTT_Publish_Status(void);
