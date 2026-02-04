#ifndef __USART_DMA_H
#define __USART_DMA_H

#include "stm32f10x.h"
#include <stdio.h>

// --- 配置 ---
#define USART_DMA_BAUDRATE      115200
#define USART_DMA_TX_BUF_SIZE   1024   // 1KB 缓冲区 (必须是2的幂次方便优化，这里暂用取模)

// --- 接口 ---

/**
  * @brief  初始化 USART1 + DMA1_Channel4
  */
void USART_DMA_Init(void);

/**
  * @brief  非阻塞格式化发送 (类似 printf)
  * @param  fmt: 格式化字符串
  * @return 1=成功写入缓冲区, 0=缓冲区满(丢弃)
  */
int USART_DMA_Printf(const char *fmt, ...);

/**
  * @brief  发送原始数据块
  */
int USART_DMA_Send(uint8_t *data, uint16_t len);

/**
  * @brief  获取发送缓冲区占用率 (0-100)
  */
uint8_t USART_DMA_GetUsage(void);

#endif
