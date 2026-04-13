#pragma once
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief CSI 状态改变回调函数类型
 */
typedef void (*csi_presence_cb_t)(bool is_present);

/**
 * @brief 初始化 Wi-Fi CSI 监听模块
 */
void Dev_CSI_Init(csi_presence_cb_t cb);

/**
 * @brief 动态切换 CSI 算法模式
 * @param mode 1: 归一化轮廓绝对差值法; 2: 宏观总振幅极差法; 3: 轮廓均方误差与截尾滤波法
 */
void Dev_CSI_Set_Mode(uint8_t mode);
