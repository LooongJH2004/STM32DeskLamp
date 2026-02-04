#ifndef __SYSTEM_SUPPORT_H
#define __SYSTEM_SUPPORT_H

#include "stm32f10x.h"
#include "Config.h"

/**
  * @brief  系统基础服务初始化
  * @note   配置 SysTick 产生系统节拍
  */
void System_Init(void);

/**
  * @brief  获取系统启动以来的总节拍数 (Tick)
  * @return uint32_t 当前 Tick 值 (通常 1 Tick = 1 ms)
  */
uint32_t System_GetTick(void);

/**
  * @brief  SysTick 中断处理函数
  * @note   需要在 stm32f10x_it.c 的 SysTick_Handler 中调用
  */
void System_IncTick(void);

/* ============================================================
 *                 Delay Functions (阻塞式)
 * ============================================================ */

/**
  * @brief  微秒级阻塞延时 (粗略)
  * @param  us 延时微秒数
  */
void Delay_us(uint32_t us);

/**
  * @brief  毫秒级阻塞延时 (基于 SysTick 轮询)
  * @param  ms 延时毫秒数
  */
void Delay_ms(uint32_t ms);

#endif
