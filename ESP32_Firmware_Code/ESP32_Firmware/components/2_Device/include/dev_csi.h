#pragma once
#include <stdbool.h>

/**
 * @brief CSI 状态改变回调函数类型
 * @param is_present true: 检测到活动 (有人); false: 环境静止 (无人/离开)
 */
typedef void (*csi_presence_cb_t)(bool is_present);

/**
 * @brief 初始化 Wi-Fi CSI 监听模块
 * @param cb 状态改变时的回调函数
 * @note 必须在 Wi-Fi 初始化 (esp_wifi_init) 之后调用
 */
void Dev_CSI_Init(csi_presence_cb_t cb);
