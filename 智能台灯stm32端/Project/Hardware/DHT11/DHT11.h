/**
  ******************************************************************************
  * @file    DHT11.h
  * @brief   DHT11 温湿度传感器驱动
  * @note    单总线协议，对时序要求严格
  *          引脚: PA1
  ******************************************************************************
  */
#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

/**
  * @brief  DHT11 初始化
  * @retval 0: 成功 (检测到设备), 1: 失败
  */
uint8_t DHT11_Init(void);

/**
  * @brief  读取温湿度数据
  * @param  temp: 温度值指针 (范围 0-50°C)
  * @param  humi: 湿度值指针 (范围 20-90%)
  * @retval 0: 读取成功, 1: 读取失败
  */
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);

#endif
