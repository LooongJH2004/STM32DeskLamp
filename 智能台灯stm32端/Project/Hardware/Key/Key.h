/* Hardware/Key/Key.h */
#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"
#include <stdint.h>

// --- 1. 核心定义 ---

#define KEY_MAX_COUNT           32
typedef uint32_t KeyMask_t;

typedef enum {
    KEY_EVT_NONE = 0,
    
    KEY_EVT_DOWN,           // 按下 (组合确认后)
    KEY_EVT_UP,             // 松开
    
    KEY_EVT_CLICK,          // 单击
    KEY_EVT_DOUBLE_CLICK,   // 双击
    KEY_EVT_TRIPLE_CLICK,   // 三连击
    
    KEY_EVT_HOLD_START,     // 长按开始
    KEY_EVT_HOLDING,        // 长按保持
    KEY_EVT_HOLD_END,       // 长按结束
    
    KEY_EVT_MODIFIER_CLICK  // 修饰键点击
} KeyEventType_t;

typedef struct {
    KeyMask_t       Mask;       // 按键掩码
    KeyEventType_t  Type;       // 事件类型
    uint32_t        Timestamp;  // 事件发生时间
    uint32_t        Param;      // [新增] 附带参数 (如长按持续时间ms)
} KeyEvent_t;

// --- 2. 单键对象 ---

typedef struct {
    GPIO_TypeDef*   GPIOx;
    uint16_t        Pin;
    uint8_t         ActiveLevel;
    uint8_t         ID;
    
    uint8_t         DebounceState;
    uint32_t        DebounceTick;
    uint8_t         IsPressed;
} Key_t;

// --- 3. 接口 ---

void Key_Init(Key_t *key, uint8_t id, GPIO_TypeDef* gpiox, uint16_t pin, uint8_t active_level);
void Key_Update(Key_t *key);
uint8_t Key_GetState(Key_t *key);

#define KEY_MASK(id)    ((uint32_t)1 << (id))

#endif
