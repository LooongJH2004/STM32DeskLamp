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
