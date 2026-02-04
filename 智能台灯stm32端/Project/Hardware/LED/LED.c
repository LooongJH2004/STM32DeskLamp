/**
  ******************************************************************************
  * @file    LED.c
  * @author  XYY
  * @version V1.1
  * @date    2023-10-27
  * @brief   LED驱动模块实现，基于 TIM3 PWM 模式
  ******************************************************************************
  */

#include "LED.h"

/**
  * @brief  LED PWM 初始化函数
  * @note   配置 TIM3 为 PWM 模式 1
  *         频率计算: 72MHz / (PSC+1) / (ARR+1)
  *         设定频率 1kHz, 分辨率 1000级:
  *         72,000,000 / 72 / 1000 = 1000Hz
  */
void LED_Init(void)
{
    /* 1. 开启时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);    // 开启 TIM3 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   // 开启 GPIOA 时钟

    /* 2. 配置 GPIO (PA6, PA7) */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;         // 复用推挽输出 (PWM必须用复用)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 3. 配置时基单元 (Time Base) */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    
    // 目标频率 1kHz, 分辨率 1000级
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;       // PSC: 预分频器 (72M / 72 = 1MHz 计数频率)
    TIM_TimeBaseInitStructure.TIM_Period = 1000 - 1;        // ARR: 自动重装值 (1MHz / 1000 = 1kHz PWM频率)
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

    /* 4. 配置输出比较通道 (Output Compare) - PWM模式1 */
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);                 // 给结构体赋默认值
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;       // PWM模式1: CNT < CCR 时输出有效电平
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 有效电平为高电平 (LED正极接IO时)

    // 配置通道 1 (PA6 - Warm/Yellow)
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    
    // 配置通道 2 (PA7 - Cold/Blue)
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);

    /* 5. 启动定时器 */
    TIM_Cmd(TIM3, ENABLE);
}

/**
  * @brief  设置暖光（黄灯）亮度
  * @param  Brightness 亮度占空比，范围 0~1000
  * @retval 无
  */
void LED_SetWarm(uint16_t Brightness)
{
    // 限制范围防止越界
    if (Brightness > 1000) Brightness = 1000;
    TIM_SetCompare1(TIM3, Brightness); // 修改 CCR1
}

/**
  * @brief  设置冷光（蓝灯）亮度
  * @param  Brightness 亮度占空比，范围 0~1000
  * @retval 无
  */
void LED_SetCold(uint16_t Brightness)
{
    // 限制范围防止越界
    if (Brightness > 1000) Brightness = 1000;
    TIM_SetCompare2(TIM3, Brightness); // 修改 CCR2
}

/**
  * @brief  同时设置双色温亮度
  * @param  WarmBri 暖光亮度 (0~1000)
  * @param  ColdBri 冷光亮度 (0~1000)
  * @retval 无
  */
void LED_SetDualColor(uint16_t WarmBri, uint16_t ColdBri)
{
    LED_SetWarm(WarmBri);
    LED_SetCold(ColdBri);
}
