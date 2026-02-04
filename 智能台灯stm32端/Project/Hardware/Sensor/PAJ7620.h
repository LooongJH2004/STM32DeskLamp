#ifndef __PAJ7620_H
#define __PAJ7620_H

#include "stm32f10x.h"

// --- 寄存器定义 ---
#define PAJ_ADDR_PART_ID        0x00
#define PAJ_ADDR_INT_FLAG1      0x43
#define PAJ_ADDR_INT_FLAG2      0x44
#define PAJ_ADDR_OBJ_BRIGHTNESS 0xB0
#define PAJ_ADDR_OBJ_SIZE_L     0xB1
#define PAJ_ADDR_OBJ_SIZE_H     0xB2
#define PAJ_ADDR_VEL_X_L        0xC3
#define PAJ_ADDR_VEL_Y_L        0xC5

// --- 手势掩码 (统一命名) ---
#define PAJ7620_GESTURE_UP              0x01
#define PAJ7620_GESTURE_DOWN            0x02
#define PAJ7620_GESTURE_LEFT            0x04
#define PAJ7620_GESTURE_RIGHT           0x08
#define PAJ7620_GESTURE_FORWARD         0x10
#define PAJ7620_GESTURE_BACKWARD        0x20
#define PAJ7620_GESTURE_CLOCKWISE       0x40
#define PAJ7620_GESTURE_COUNTER_CW      0x80
#define PAJ7620_GESTURE_WAVE            0x01 // Flag2

// --- 状态机枚举 ---
typedef enum {
    PAJ_STATE_IDLE = 0,         // 指令模式 (等待手势)
    PAJ_STATE_PROXIMITY_CTRL    // 近距控制模式 (实时调光)
} PAJ_State_t;

// --- 数据结构 ---
typedef struct {
    uint8_t  GestureFlag1;
    uint8_t  GestureFlag2;
    uint8_t  ObjectBrightness;
    uint16_t ObjectSize;
    int16_t  VelocityX;
    int16_t  VelocityY;
    uint8_t  IsConnected;
} PAJ7620_Data_t;

// --- 接口 ---
uint8_t PAJ7620_Init(void);
void PAJ7620_ReadAllData(PAJ7620_Data_t *data);

/**
 * @brief 核心状态机处理函数 (需在主循环高速调用)
 */
void PAJ7620_Process_StateMachine(void);

// --- Hook 声明 ---
void PAJ7620_Hook_OnUp(void);
void PAJ7620_Hook_OnDown(void);
void PAJ7620_Hook_OnLeft(void);
void PAJ7620_Hook_OnRight(void);
void PAJ7620_Hook_OnForward(void);
void PAJ7620_Hook_OnBackward(void);
void PAJ7620_Hook_OnClockwise(void);
void PAJ7620_Hook_OnCounterClockwise(void);
void PAJ7620_Hook_OnWave(void);

/**
 * @brief 实时近距控制回调
 * @param brightness 当前亮度值 (0-255)
 */
void PAJ7620_Hook_OnProximity(uint8_t brightness);

#endif
