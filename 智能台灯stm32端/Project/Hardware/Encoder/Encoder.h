#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

/**
  * @brief  编码器初始化 (TIM4 硬件模式)
  * @note   PB6(TIM4_CH1), PB7(TIM4_CH2) 用于旋转检测
  *         按键功能已移至独立的 Key 模块
  */
void Encoder_Init(void);

/**
  * @brief  获取编码器增量值
  * @return int16_t 增量值 (正数=顺时针, 负数=逆时针)
  * @note   调用后会自动清零计数器，适合轮询使用
  */
int16_t Encoder_Get(void);

// Encoder_GetButton() 声明已被移除

#endif
