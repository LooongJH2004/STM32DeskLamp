/* User/main.c */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   主程序 (V13.0 KeyManager Integration)
  * @note    集成 KeyManager V2.0，支持多键、连击与长按
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "SystemSupport.h"
#include "USART_DMA.h"
#include "Protocol.h"
#include "ControlManager.h"
#include "SensorHub.h"
#include "UIManager.h"
#include "SystemModel.h"

// --- 硬件驱动 ---
#include "Encoder.h"
#include "PAJ7620.h"
#include "KeyManager.h" // 新的按键管理器

/* ============================================================
 *      按键定义与 ID 映射
 * ============================================================ */
static Key_t Key_Mode;

// 定义 ID (用于掩码位移)
#define KID_MODE    0

// 定义掩码 (方便判断)
#define MASK_MODE   (1 << KID_MODE)

/* ============================================================
 *      回调函数实现 (连接驱动层与业务层)
 * ============================================================ */

// --- 手势事件回调 (离散) ---
void PAJ7620_Hook_OnUp(void)        { Control_OnGesture(PAJ7620_GESTURE_UP); }
void PAJ7620_Hook_OnDown(void)      { Control_OnGesture(PAJ7620_GESTURE_DOWN); }
void PAJ7620_Hook_OnLeft(void)      { Control_OnGesture(PAJ7620_GESTURE_LEFT); }
void PAJ7620_Hook_OnRight(void)     { Control_OnGesture(PAJ7620_GESTURE_RIGHT); }
void PAJ7620_Hook_OnForward(void)   { Control_OnGesture(PAJ7620_GESTURE_FORWARD); }
void PAJ7620_Hook_OnBackward(void)  { Control_OnGesture(PAJ7620_GESTURE_BACKWARD); }
void PAJ7620_Hook_OnClockwise(void) { Control_OnGesture(PAJ7620_GESTURE_CLOCKWISE); }
void PAJ7620_Hook_OnCounterClockwise(void) { Control_OnGesture(PAJ7620_GESTURE_COUNTER_CW); }
void PAJ7620_Hook_OnWave(void)      { Control_OnGesture(PAJ7620_GESTURE_WAVE); }

// --- 手势事件回调 (实时) ---
void PAJ7620_Hook_OnProximity(uint8_t brightness) {
    Control_OnProximity(brightness);
}

/* ============================================================
 *      硬件初始化辅助函数
 * ============================================================ */
void Hardware_Init_Keys(void)
{
    // 1. 开启 GPIO 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 2. 初始化 KeyManager
    KeyManager_Init();

    // 3. 初始化并注册按键
    // ModeSW: PB1, 低电平有效 (0)
    Key_Init(&Key_Mode, KID_MODE, GPIOB, GPIO_Pin_1, 0);
    KeyManager_Register(&Key_Mode);
}

/* ============================================================
 *      主函数
 * ============================================================ */
int main(void)
{
    // 1. 基础设施初始化
    System_Init();
    Delay_ms(100); // 等待电源稳定
    USART_DMA_Init();
    
    printf("\r\n=== Smart Lamp System V13.0 (KeyManager) ===\r\n");

    // 2. 数据模型初始化 (必须最先)
    SystemModel_Init();

    // 3. 业务层初始化
    Protocol_Init();
    Control_Init();   
    SensorHub_Init(); 
    UIManager_Init(); 

    // 4. 输入设备初始化
    Encoder_Init();
    Hardware_Init_Keys(); // 初始化新按键库
    
    if (PAJ7620_Init() != 0) {
        printf("[Main] Gesture Init Failed.\r\n");
    } else {
        printf("[Main] Gesture Ready.\r\n");
    }

    // 5. 调度器计时器
    uint32_t tick_50ms = 0;
    uint32_t tick_100ms = 0;
    uint32_t tick_2000ms = 0;
    uint32_t hb_count = 0;

    while (1)
    {
        uint32_t now = System_GetTick();

        // --- L1: 实时任务 (尽可能快) ---
        Protocol_Process(); 
        
        // 编码器处理
        int16_t enc_diff = Encoder_Get();
        if (enc_diff != 0) {
            Control_OnEncoder(enc_diff);
        }

        // 按键状态机处理 (核心逻辑)
        KeyManager_Tick();

        // 按键事件分发 (适配层)
        KeyEvent_t k_evt;
        if (KeyManager_GetEvent(&k_evt))
        {
            // 仅处理 Mode 键 (未来可扩展组合键)
            if (k_evt.Mask == MASK_MODE)
            {
                char* act_str = NULL;
                switch (k_evt.Type) {
                    case KEY_EVT_CLICK:         act_str = "click";   break;
                    case KEY_EVT_DOUBLE_CLICK:  act_str = "double";  break;
                    case KEY_EVT_TRIPLE_CLICK:  act_str = "triple";  break;
                    case KEY_EVT_HOLD_START:    act_str = "hold";    break;
                    case KEY_EVT_HOLD_END:      act_str = "release"; break;
                    default: break;
                }
                
                if (act_str != NULL) {
                    // 转发给业务层
                    Control_OnKey("ModeSW", act_str);
                    
                    // 如果是长按结束，还可以打印时长用于调试
                    if (k_evt.Type == KEY_EVT_HOLD_END) {
                        printf("[Main] Hold Duration: %d ms\r\n", k_evt.Param);
                    }
                }
            }
        }

        // 手势状态机处理
        PAJ7620_Process_StateMachine();

        // --- L2: 50ms 任务 (业务逻辑) ---
        if (now - tick_50ms >= 50)
        {
            tick_50ms = now;
            Control_Task(); 
        }

        // --- L3: 100ms 任务 (UI 刷新) ---
        if (now - tick_100ms >= 100)
        {
            tick_100ms = now;
            UIManager_Task(); 
        }

        // --- L4: 2000ms 任务 (传感器 & 心跳) ---
        if (now - tick_2000ms >= 2000)
        {
            tick_2000ms = now;
            Protocol_Report_Heartbeat(hb_count++);
            SensorHub_Task();
        }
    }
}
