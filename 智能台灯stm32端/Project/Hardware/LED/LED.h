/**
  ******************************************************************************
  * @file    LED.h
  * @author  XYY
  * @version V1.1
  * @date    2023-10-27
  * @brief   LED驱动模块头文件，提供双色温PWM控制接口 (1000级分辨率)
  ******************************************************************************
  */

#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/**
  * @brief  LED PWM 初始化函数
  * @param  无
  * @retval 无
  * @note   初始化 PA6(TIM3_CH1) 和 PA7(TIM3_CH2) 为复用推挽输出，并启动 PWM
  */
void LED_Init(void);

/**
  * @brief  设置暖光（黄灯）亮度
  * @param  Brightness 亮度占空比，范围 0~1000
  * @retval 无
  */
void LED_SetWarm(uint16_t Brightness);

/**
  * @brief  设置冷光（蓝灯）亮度
  * @param  Brightness 亮度占空比，范围 0~1000
  * @retval 无
  */
void LED_SetCold(uint16_t Brightness);

/**
  * @brief  同时设置双色温亮度
  * @param  WarmBri 暖光亮度 (0~1000)
  * @param  ColdBri 冷光亮度 (0~1000)
  * @retval 无
  */
void LED_SetDualColor(uint16_t WarmBri, uint16_t ColdBri);

#endif
