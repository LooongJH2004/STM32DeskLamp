#include "USART_DMA.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

// --- 内部变量 ---
static uint8_t  s_TxBuf[USART_DMA_TX_BUF_SIZE];
static volatile uint16_t s_Head = 0; // 写入位置 (CPU)
static volatile uint16_t s_Tail = 0; // 读取位置 (DMA)
static volatile uint8_t  s_DmaBusy = 0;
static volatile uint16_t s_LastSendLen = 0; // 记录DMA正在传输的长度

// --- 内部函数声明 ---
static void _CheckAndStartDMA(void);

// --- 初始化 ---
void USART_DMA_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // PA9 TX
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA10 RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = USART_DMA_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // 开启接收中断

    // DMA1 Channel4 (USART1_TX)
    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA1_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)s_TxBuf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    // DMA TX Interrupt
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // USART RX Interrupt
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);
    
    s_Head = 0;
    s_Tail = 0;
    s_DmaBusy = 0;
}

// --- 核心逻辑 ---

// 尝试启动 DMA (需在关中断或中断回调中调用)
static void _CheckAndStartDMA(void)
{
    if (s_DmaBusy) return;      // DMA 忙
    if (s_Head == s_Tail) return; // 无数据

    uint16_t sendLen;
    uint16_t head = s_Head;
    uint16_t tail = s_Tail;

    // 计算连续数据块长度
    if (head > tail)
    {
        sendLen = head - tail;
    }
    else
    {
        sendLen = USART_DMA_TX_BUF_SIZE - tail;
    }

    s_LastSendLen = sendLen; // 记录本次发送长度
    s_DmaBusy = 1;

    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA1_Channel4->CMAR = (uint32_t)&s_TxBuf[tail];
    DMA1_Channel4->CNDTR = sendLen;
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

int USART_DMA_Send(uint8_t *data, uint16_t len)
{
    if (len == 0) return 0;
    if (len > USART_DMA_TX_BUF_SIZE) return 0;

    // 计算剩余空间
    uint16_t head = s_Head;
    uint16_t tail = s_Tail;
    uint16_t used = (head >= tail) ? (head - tail) : (USART_DMA_TX_BUF_SIZE + head - tail);
    uint16_t free = USART_DMA_TX_BUF_SIZE - 1 - used;

    if (len > free) return 0; // 缓冲区满，丢弃

    // 复制数据
    uint16_t chunk1 = USART_DMA_TX_BUF_SIZE - head;
    if (len <= chunk1)
    {
        memcpy(&s_TxBuf[head], data, len);
        s_Head = (head + len) % USART_DMA_TX_BUF_SIZE;
    }
    else
    {
        memcpy(&s_TxBuf[head], data, chunk1);
        memcpy(&s_TxBuf[0], data + chunk1, len - chunk1);
        s_Head = len - chunk1;
    }

    // 启动 DMA (临界区保护)
    __disable_irq();
    _CheckAndStartDMA();
    __enable_irq();

    return 1;
}

int USART_DMA_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len > 0) return USART_DMA_Send((uint8_t*)buf, len);
    return 0;
}

uint8_t USART_DMA_GetUsage(void)
{
    uint16_t head = s_Head;
    uint16_t tail = s_Tail;
    uint16_t used = (head >= tail) ? (head - tail) : (USART_DMA_TX_BUF_SIZE + head - tail);
    return (uint8_t)((uint32_t)used * 100 / USART_DMA_TX_BUF_SIZE);
}

// --- [关键修复] 重定向 fputc ---
// 这使得 PAJ7620.c 等旧代码中的 printf() 能够正常工作，而不会导致 HardFault
int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;
    // 尝试发送，如果缓冲区满则丢弃（防止阻塞死机）
    USART_DMA_Send(&c, 1);
    return ch;
}

// --- 中断处理 ---

void DMA1_Channel4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC4))
    {
        DMA_ClearITPendingBit(DMA1_IT_TC4);

        // 更新 Tail
        s_Tail = (s_Tail + s_LastSendLen) % USART_DMA_TX_BUF_SIZE;
        s_DmaBusy = 0;

        // 检查是否还有剩余数据 (接力发送)
        _CheckAndStartDMA();
    }
}

// 接收中断钩子
extern void Protocol_OnRxData(uint8_t data);

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t)USART_ReceiveData(USART1);
        Protocol_OnRxData(data); // 喂给协议层
    }
}
