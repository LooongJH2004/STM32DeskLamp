#include "SystemSupport.h"

// 全局系统节拍计数器
static volatile uint32_t g_SystemTick = 0;

/**
  * @brief  系统初始化
  */
void System_Init(void)
{
    // [关键修复] 配置中断优先级分组 (2位抢占，2位响应)
    // 必须在任何外设初始化之前调用！
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 1. 配置 SysTick */
    // SysTick_Config 函数会自动配置重装值、开启中断并启动计数器
    if (SysTick_Config(SystemCoreClock / SYSTEM_TICK_FREQ))
    {
        while (1);
    }
}

/**
  * @brief  获取当前 Tick
  */
uint32_t System_GetTick(void)
{
    return g_SystemTick;
}

/**
  * @brief  Tick 递增 (在 ISR 中调用)
  */
void System_IncTick(void)
{
    g_SystemTick++;
}

/* ============================================================
 *                 Delay Functions (阻塞式)
 * ============================================================ */

void Delay_us(uint32_t us)
{
    uint32_t i;
    while(us--)
    {
        i = 10; 
        while(i--);
    }
}

void Delay_ms(uint32_t ms)
{
    uint32_t startTick = System_GetTick();
    while ((System_GetTick() - startTick) < ms);
}
