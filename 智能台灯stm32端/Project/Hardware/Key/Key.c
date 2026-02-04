/* Hardware/Key/Key.c */
#include "Key.h"
#include "SystemSupport.h" // 提供 System_GetTick()

// 基础去抖时间 (ms)
#define KEY_DEBOUNCE_TIME   15

void Key_Init(Key_t *key, uint8_t id, GPIO_TypeDef* gpiox, uint16_t pin, uint8_t active_level)
{
    // 1. 保存参数
    key->GPIOx = gpiox;
    key->Pin = pin;
    key->ActiveLevel = active_level;
    key->ID = id;
    
    // 2. 初始化状态
    key->DebounceState = 0;
    key->DebounceTick = 0;
    key->IsPressed = 0;

    // 3. 硬件初始化
    // 注意：时钟需在外部开启，或在此处添加判断开启
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    if (active_level == 0) {
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 低电平有效 -> 上拉输入
    } else {
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; // 高电平有效 -> 下拉输入
    }
    GPIO_Init(gpiox, &GPIO_InitStructure);
}

void Key_Update(Key_t *key)
{
    // 读取物理电平
    uint8_t raw_level = GPIO_ReadInputDataBit(key->GPIOx, key->Pin);
    uint8_t is_active = (raw_level == key->ActiveLevel);

    // 简单的去抖状态机
    if (is_active != key->DebounceState) {
        // 状态发生变化，重置计时器
        key->DebounceState = is_active;
        key->DebounceTick = System_GetTick();
    } else {
        // 状态稳定
        if ((System_GetTick() - key->DebounceTick) >= KEY_DEBOUNCE_TIME) {
            key->IsPressed = is_active;
        }
    }
}

uint8_t Key_GetState(Key_t *key)
{
    return key->IsPressed;
}
