/**
 * @file LDR.h
 * @brief 光敏电阻驱动，使用 ADC1_IN0 (PA0)
 */
#ifndef __LDR_H
#define __LDR_H

#include "stm32f10x.h"

/**
 * @brief 初始化 LDR 所需的 ADC 和 GPIO
 */
void LDR_Init(void);

/**
 * @brief 获取当前环境光强度原始值
 * @return uint16_t ADC 采样值 (0-4095)
 */
uint16_t LDR_GetRawValue(void);

/**
 * @brief 获取平滑处理后的光照强度百分比
 * @return uint16_t 0-1000 映射值
 */
uint16_t LDR_GetLuxPercentage(void);

#endif
