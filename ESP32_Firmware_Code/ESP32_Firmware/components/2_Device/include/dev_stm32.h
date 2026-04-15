#pragma once
#include <stdint.h>

/**
 * @brief 初始化与 STM32 通信的 UART 外设及接收任务
 */
void Dev_STM32_Init(void);

/**
 * @brief 向 STM32 发送灯光控制指令
 * @param warm 暖光 PWM (0-1000)
 * @param cold 冷光 PWM (0-1000)
 */
void Dev_STM32_Set_Light(uint16_t warm, uint16_t cold);

/**
 * @brief 向 STM32 发送模式切换指令
 * @param mode 0: Local 模式, 1: Remote UI 模式
 */
void Dev_STM32_Set_Mode(uint8_t mode);

/**
 * @brief 动态切换 CRC 发送策略 (用于误码率与拦截测试)
 * @param mode 0: 正确 CRC (crc_right); 1: 错误 CRC (crc_error)
 */
void Dev_STM32_Set_CRC_Mode(uint8_t mode);
