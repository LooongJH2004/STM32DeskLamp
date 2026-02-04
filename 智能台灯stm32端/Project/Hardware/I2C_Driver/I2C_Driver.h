/**
  ******************************************************************************
  * @file    I2C_Driver.h
  * @brief   通用软件模拟 I2C 驱动 (高可靠版)
  * @note    完全替代不稳定的硬件 I2C，接口保持兼容
  ******************************************************************************
  */
#ifndef __I2C_DRIVER_H
#define __I2C_DRIVER_H

#include "stm32f10x.h"

// --- 接口定义 (保持与原硬件驱动一致) ---

/**
  * @brief  初始化 I2C GPIO
  * @param  I2Cx: 仅用于区分引脚组
  *         - I2C1: SCL=PB8,  SDA=PB9  (用于 OLED)
  *         - I2C2: SCL=PB10, SDA=PB11 (用于 PAJ7620)
  */
void I2C_Lib_Init(I2C_TypeDef* I2Cx);

/**
  * @brief  写寄存器
  */
uint8_t I2C_Lib_Write(I2C_TypeDef* I2Cx, uint8_t DevAddr, uint8_t RegAddr, uint8_t* pData, uint16_t Size);

/**
  * @brief  读寄存器
  */
uint8_t I2C_Lib_Read(I2C_TypeDef* I2Cx, uint8_t DevAddr, uint8_t RegAddr, uint8_t* pData, uint16_t Size);

/**
  * @brief  直接写数据 (无寄存器地址，用于 OLED)
  */
uint8_t I2C_Lib_WriteDirect(I2C_TypeDef* I2Cx, uint8_t DevAddr, uint8_t* pData, uint16_t Size);

/**
  * @brief  检查设备是否在线
  */
uint8_t I2C_Lib_IsDeviceReady(I2C_TypeDef* I2Cx, uint8_t DevAddr);

#endif
