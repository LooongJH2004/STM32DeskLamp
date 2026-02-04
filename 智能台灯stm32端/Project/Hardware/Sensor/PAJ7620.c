/**
  ******************************************************************************
  * @file    PAJ7620.c
  * @brief   PAJ7620U2 驱动 (V10.1 Fixed Macros)
  * @note    修复宏定义命名不一致问题，包含反向手势滤波
  ******************************************************************************
  */
#include "PAJ7620.h"
#include "I2C_Driver.h"
#include "USART_DMA.h"
#include "SystemSupport.h"
#include <string.h>

// --- 配置 ---
#define PAJ_I2C_PORT            I2C2    
#define PAJ_I2C_ADDR            0xE6    

// 阈值参数
#define PAJ_PROXIMITY_EXIT_TH   20   // 退出近距模式的亮度阈值
#define PAJ_REVERSE_FILTER_TIME 600  // 反向动作过滤时间 (ms)

// --- 内部变量 ---
static PAJ_State_t s_State = PAJ_STATE_IDLE;
static uint8_t s_LastGesture = 0;    // 记录上一次的有效手势
static uint32_t s_LastGestureTick = 0;

// --- 官方初始化数组 ---
static const uint8_t PAJ7620_Init_Regs[][2] = {
    {0xEF,0x00}, {0x41,0xFF}, {0x42,0x01}, {0x46,0x2D}, {0x47,0x0F}, 
    {0x48,0x80}, {0x49,0x00}, {0x4A,0x40}, {0x4B,0x00}, {0x4C,0x20}, 
    {0x4D,0x00}, {0x51,0x10}, {0x5C,0x02}, {0x5E,0x10}, {0x80,0x41}, 
    {0x81,0x44}, {0x82,0x0C}, {0x83,0x20}, {0x84,0x20}, {0x85,0x00}, 
    {0x86,0x10}, {0x87,0x00}, {0x8B,0x01}, {0x8D,0x00}, {0x90,0x0C}, 
    {0x91,0x0C}, {0x93,0x0D}, {0x94,0x0A}, {0x95,0x0A}, {0x96,0x0C}, 
    {0x97,0x05}, {0x9A,0x14}, {0x9C,0x3F}, {0x9F,0xF9}, {0xA0,0x48}, 
    {0xA5,0x19}, {0xCC,0x19}, {0xCD,0x0B}, {0xCE,0x13}, {0xCF,0x62}, 
    {0xD0,0x21}, {0xEF,0x01}, {0x00,0x1E}, {0x01,0x1E}, {0x02,0x0F}, 
    {0x03,0x0F}, {0x04,0x02}, {0x25,0x01}, {0x26,0x00}, {0x27,0x39}, 
    {0x28,0x7F}, {0x29,0x08}, {0x30,0x03}, {0x3E,0xFF}, {0x5E,0x3D}, 
    {0x65,0xAC}, {0x66,0x00}, {0x67,0x97}, {0x68,0x01}, {0x69,0xCD}, 
    {0x6A,0x01}, {0x6B,0xB0}, {0x6C,0x04}, {0x6D,0x2C}, {0x6E,0x01}, 
    {0x72,0x01}, {0x73,0x35}, {0x74,0x00}, {0x77,0x01}, {0xEF,0x00}
};

// --- 底层 I2C ---
static uint8_t PAJ_Write(uint8_t reg, uint8_t val) {
    return I2C_Lib_Write(PAJ_I2C_PORT, PAJ_I2C_ADDR, reg, &val, 1);
}
static uint8_t PAJ_Read(uint8_t reg, uint8_t *val) {
    return I2C_Lib_Read(PAJ_I2C_PORT, PAJ_I2C_ADDR, reg, val, 1);
}

// --- 初始化 ---
uint8_t PAJ7620_Init(void)
{
    uint8_t part_id = 0;
    I2C_Lib_Init(PAJ_I2C_PORT);
    Delay_ms(10);
    PAJ_Read(0x00, &part_id); Delay_ms(5);
    
    if (PAJ_Read(PAJ_ADDR_PART_ID, &part_id) != 0 || part_id != 0x20) return 1;
    
    for (int i = 0; i < sizeof(PAJ7620_Init_Regs)/2; i++) {
        PAJ_Write(PAJ7620_Init_Regs[i][0], PAJ7620_Init_Regs[i][1]);
    }
    PAJ_Write(0xEF, 0x00);
    return 0;
}

// --- 读取全量数据 ---
void PAJ7620_ReadAllData(PAJ7620_Data_t *data)
{
    uint8_t buf[2];
    memset(data, 0, sizeof(PAJ7620_Data_t));
    
    if (PAJ_Write(0xEF, 0x00) != 0) { data->IsConnected = 0; return; }
    
    PAJ_Read(PAJ_ADDR_INT_FLAG1, &data->GestureFlag1);
    PAJ_Read(PAJ_ADDR_INT_FLAG2, &data->GestureFlag2);
    PAJ_Read(PAJ_ADDR_OBJ_BRIGHTNESS, &data->ObjectBrightness);
    PAJ_Read(PAJ_ADDR_OBJ_SIZE_L, &buf[0]);
    PAJ_Read(PAJ_ADDR_OBJ_SIZE_H, &buf[1]);
    data->ObjectSize = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    
    uint8_t vx, vy;
    PAJ_Read(PAJ_ADDR_VEL_X_L, &vx);
    PAJ_Read(PAJ_ADDR_VEL_Y_L, &vy);
    data->VelocityX = (int8_t)vx;
    data->VelocityY = (int8_t)vy;
    
    data->IsConnected = 1;
}

// --- 辅助函数：判断是否为反向手势 ---
static uint8_t Is_Reverse_Gesture(uint8_t curr, uint8_t last) {
    if (curr == PAJ7620_GESTURE_RIGHT && last == PAJ7620_GESTURE_LEFT) return 1;
    if (curr == PAJ7620_GESTURE_LEFT && last == PAJ7620_GESTURE_RIGHT) return 1;
    if (curr == PAJ7620_GESTURE_UP && last == PAJ7620_GESTURE_DOWN) return 1;
    if (curr == PAJ7620_GESTURE_DOWN && last == PAJ7620_GESTURE_UP) return 1;
    if (curr == PAJ7620_GESTURE_BACKWARD && last == PAJ7620_GESTURE_FORWARD) return 1;
    return 0;
}

// --- V10.1 核心逻辑 ---
void PAJ7620_Process_StateMachine(void)
{
    PAJ7620_Data_t data;
    PAJ7620_ReadAllData(&data);
    if (!data.IsConnected) return;

    uint8_t g1 = data.GestureFlag1;
    uint8_t g2 = data.GestureFlag2;
    uint32_t now = System_GetTick();

    // ---------------------------------------------------------
    // 0. 反向手势滤波 (Anti-Rebound Filter)
    // ---------------------------------------------------------
    if (g1 != 0) {
        if (Is_Reverse_Gesture(g1, s_LastGesture)) {
            if (now - s_LastGestureTick < PAJ_REVERSE_FILTER_TIME) {
                return; // 忽略回正动作
            }
        }
    }

    // ---------------------------------------------------------
    // 1. 优先级仲裁 (Priority Arbiter)
    // ---------------------------------------------------------
    if (g1 & (PAJ7620_GESTURE_FORWARD | PAJ7620_GESTURE_BACKWARD)) {
        g1 &= ~(PAJ7620_GESTURE_UP | PAJ7620_GESTURE_DOWN | PAJ7620_GESTURE_LEFT | PAJ7620_GESTURE_RIGHT | PAJ7620_GESTURE_CLOCKWISE | PAJ7620_GESTURE_COUNTER_CW);
    }
    else if (g1 & (PAJ7620_GESTURE_CLOCKWISE | PAJ7620_GESTURE_COUNTER_CW)) {
        g1 &= ~(PAJ7620_GESTURE_UP | PAJ7620_GESTURE_DOWN | PAJ7620_GESTURE_LEFT | PAJ7620_GESTURE_RIGHT);
    }

    // ---------------------------------------------------------
    // 2. 状态机分发
    // ---------------------------------------------------------
    switch (s_State)
    {
        // --- 模式 A: 指令模式 (IDLE) ---
        case PAJ_STATE_IDLE:
            // 记录有效手势
            if (g1 != 0) {
                s_LastGesture = g1;
                s_LastGestureTick = now;
            }

            if (g1 & PAJ7620_GESTURE_FORWARD) {
                PAJ7620_Hook_OnForward();
                s_State = PAJ_STATE_PROXIMITY_CTRL;
                USART_DMA_Printf("[PAJ] 进入无极调光\r\n");
            }
            else if (g1 & PAJ7620_GESTURE_BACKWARD)     PAJ7620_Hook_OnBackward();
            else if (g1 & PAJ7620_GESTURE_CLOCKWISE)    PAJ7620_Hook_OnClockwise();
            else if (g1 & PAJ7620_GESTURE_COUNTER_CW)   PAJ7620_Hook_OnCounterClockwise();
            else if (g1 & PAJ7620_GESTURE_UP)           PAJ7620_Hook_OnUp();
            else if (g1 & PAJ7620_GESTURE_DOWN)         PAJ7620_Hook_OnDown();
            else if (g1 & PAJ7620_GESTURE_LEFT)         PAJ7620_Hook_OnLeft();
            else if (g1 & PAJ7620_GESTURE_RIGHT)        PAJ7620_Hook_OnRight();
            else if (g2 & PAJ7620_GESTURE_WAVE)         PAJ7620_Hook_OnWave();
            break;

        // --- 模式 B: 近距控制模式 (PROXIMITY) ---
        case PAJ_STATE_PROXIMITY_CTRL:
            if (data.ObjectBrightness < PAJ_PROXIMITY_EXIT_TH) {
                s_State = PAJ_STATE_IDLE;
                USART_DMA_Printf("[PAJ] 退出调光\r\n");
                
                // 退出时设置上一次动作为 FORWARD，防止误触 BACKWARD
                s_LastGesture = PAJ7620_GESTURE_FORWARD;
                s_LastGestureTick = now;
            }
            else {
                PAJ7620_Hook_OnProximity(data.ObjectBrightness);
            }
            break;
    }
}

// --- Weak Hooks ---
__weak void PAJ7620_Hook_OnUp(void) {}
__weak void PAJ7620_Hook_OnDown(void) {}
__weak void PAJ7620_Hook_OnLeft(void) {}
__weak void PAJ7620_Hook_OnRight(void) {}
__weak void PAJ7620_Hook_OnForward(void) {}
__weak void PAJ7620_Hook_OnBackward(void) {}
__weak void PAJ7620_Hook_OnClockwise(void) {}
__weak void PAJ7620_Hook_OnCounterClockwise(void) {}
__weak void PAJ7620_Hook_OnWave(void) {}
__weak void PAJ7620_Hook_OnProximity(uint8_t brightness) {}
