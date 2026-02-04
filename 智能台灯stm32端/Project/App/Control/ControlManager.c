/* App/Control/ControlManager.c */
/**
  ******************************************************************************
  * @file    ControlManager.c
  * @brief   业务逻辑控制器 (V13.0 KeyManager Support)
  * @note    适配新的按键事件流
  ******************************************************************************
  */
#include "ControlManager.h"
#include "LightCtrl.h"
#include "Protocol.h"
#include "USART_DMA.h"
#include "SystemModel.h"
#include "PAJ7620.h"
#include "SystemSupport.h"
#include <string.h>
#include <stdlib.h> // for abs()

// --- 配置参数 ---
#define PROX_STABILITY_TH       5       // 抖动容差
#define PROX_LOCK_TIME_MS      850      // 悬停锁定时间

// --- 状态定义 ---
typedef enum {
    CTRL_MODE_LOCAL = 0,
    CTRL_MODE_REMOTE_UI
} ControlMode_t;

static ControlMode_t s_Mode = CTRL_MODE_LOCAL;
static uint8_t s_IsLongPressing = 0; 

// --- 无极调光专用变量 ---
static uint8_t  s_ProxLocked = 0;
static uint8_t  s_ProxLastStableVal = 0;
static uint32_t s_ProxStableTick = 0;

// --- 内部回调 ---
static void _OnProto_Mode(uint8_t mode) {
    s_Mode = (mode == 0) ? CTRL_MODE_LOCAL : CTRL_MODE_REMOTE_UI;
    USART_DMA_Printf("[Ctrl] Mode -> %s\r\n", s_Mode == CTRL_MODE_LOCAL ? "LOCAL" : "UI");
}

static void _OnProto_Light(uint16_t warm, uint16_t cold) {
    LightCtrl_SetRawPWM(warm, cold);
}

static void Control_ToggleMode(void) {
    if (s_Mode == CTRL_MODE_LOCAL) {
        s_Mode = CTRL_MODE_REMOTE_UI;
    } else {
        s_Mode = CTRL_MODE_LOCAL;
    }
    USART_DMA_Printf("[Ctrl] Toggle Mode -> %s\r\n", s_Mode == CTRL_MODE_LOCAL ? "LOCAL" : "UI");
}

// --- 初始化 ---
void Control_Init(void) {
    LightCtrl_Init();
    Protocol_SetModeCallback(_OnProto_Mode);
    Protocol_SetLightCallback(_OnProto_Light);
}

// --- 事件处理 ---

void Control_OnEncoder(int16_t diff) {
    if(s_Mode == CTRL_MODE_REMOTE_UI){
        Protocol_Report_Encoder(diff);
    }
    
    if (s_Mode == CTRL_MODE_LOCAL) {
        if (s_IsLongPressing) {
            LightCtrl_AdjustColorTemp(diff);
        } else {
            if (g_SystemModel.Light.Focus == FOCUS_BRIGHTNESS) {
                LightCtrl_AdjustBrightness(diff);
            } else {
                LightCtrl_AdjustColorTemp(diff);
            }
        }
    }
}

// [修改] 适配新的事件流
void Control_OnKey(const char* key_name, const char* action) {
    
    // 1. 优先上报事件 (透传)
    Protocol_Report_Key(key_name, action);

    // 2. 本地业务逻辑处理
    if (strcmp(key_name, "ModeSW") == 0) 
    {
        // 长按开始
        if (strcmp(action, "hold") == 0) {
            s_IsLongPressing = 1;
            USART_DMA_Printf("[Ctrl] Long Press START (ColorTemp Mode)\r\n");
        }
        // 长按结束
        else if (strcmp(action, "release") == 0) {
            s_IsLongPressing = 0;
            // 长按结束后，通常重置回亮度模式，或者保持当前状态
            g_SystemModel.Light.Focus = FOCUS_BRIGHTNESS;
            USART_DMA_Printf("[Ctrl] Long Press END\r\n");
        }
        // 三连击 -> 切换控制模式
        else if (strcmp(action, "triple") == 0) {
            Control_ToggleMode();
        }
        // 单击 -> 切换焦点 (亮度/色温)
        else if (strcmp(action, "click") == 0) {
            if (g_SystemModel.Light.Focus == FOCUS_BRIGHTNESS) {
                g_SystemModel.Light.Focus = FOCUS_COLOR_TEMP;
                USART_DMA_Printf("[Ctrl] Focus -> CCT\r\n");
            } else {
                g_SystemModel.Light.Focus = FOCUS_BRIGHTNESS;
                USART_DMA_Printf("[Ctrl] Focus -> Bri\r\n");
            }
        }
        // 双击 -> 重置灯光 (中性光 50%)
        else if (strcmp(action, "double") == 0) {
            if (s_Mode == CTRL_MODE_LOCAL) {
                LightCtrl_SetRawPWM(250, 250); // 500总亮度，500色温
                USART_DMA_Printf("[Ctrl] Reset (Double Click)\r\n");
            }
        }
    }
}

void Control_OnGesture(uint8_t gesture) {
    Protocol_Report_Gesture(gesture);

    if (s_Mode == CTRL_MODE_LOCAL) {
        switch(gesture) {
            case PAJ7620_GESTURE_UP:
                LightCtrl_AdjustBrightness(200);
                break;
            case PAJ7620_GESTURE_DOWN:
                LightCtrl_AdjustBrightness(-200);
                break;
            case PAJ7620_GESTURE_LEFT:
                LightCtrl_AdjustColorTemp(200);
                break;
            case PAJ7620_GESTURE_RIGHT:
                LightCtrl_AdjustColorTemp(-200);
                break;
            case PAJ7620_GESTURE_FORWARD:
                s_ProxLocked = 0;
                s_ProxLastStableVal = 0;
                s_ProxStableTick = System_GetTick();
                USART_DMA_Printf("[Ctrl] Local: Enter Proximity Mode\r\n");
                break;
            case PAJ7620_GESTURE_BACKWARD:
                LightCtrl_SetRawPWM(0, 0);
                USART_DMA_Printf("[Ctrl] Local: OFF\r\n");
                break;
            default: break;
        }
    }
}

void Control_OnProximity(uint8_t brightness) {
    if (s_Mode != CTRL_MODE_LOCAL) return;
    if (s_ProxLocked) return;

    int diff = abs((int)brightness - (int)s_ProxLastStableVal);

    if (diff > PROX_STABILITY_TH) {
        s_ProxLastStableVal = brightness;
        s_ProxStableTick = System_GetTick();
    } 
    else {
        if (System_GetTick() - s_ProxStableTick > PROX_LOCK_TIME_MS) {
            s_ProxLocked = 1;
            USART_DMA_Printf("\r\n>> [Prox] Auto-Locked! <<\r\n");
            return;
        }
    }

    int16_t target_val = 0;
    if (brightness > 20) {
        target_val = (brightness - 20) * 5;
    }
    if (target_val > 1000) target_val = 1000;
    if (target_val < 0) target_val = 0;

    static uint32_t last_update_tick = 0;
    if (System_GetTick() - last_update_tick > 50) {
        last_update_tick = System_GetTick();

        if (g_SystemModel.Light.Focus == FOCUS_BRIGHTNESS) {
            g_SystemModel.Light.Brightness = target_val;
        } else {
            g_SystemModel.Light.ColorTemp = target_val;
        }

        uint16_t bri = g_SystemModel.Light.Brightness;
        uint16_t cct = g_SystemModel.Light.ColorTemp;
        
        float bri_factor = bri / 1000.0f;
        float cct_factor = cct / 1000.0f;
        
        uint16_t warm = (uint16_t)((1.0f - cct_factor) * 1000 * bri_factor);
        uint16_t cold = (uint16_t)(cct_factor * 1000 * bri_factor);
        
        LightCtrl_SetRawPWM(warm, cold);
    }
}

void Control_Task(void) {
    LightCtrl_Task();
}

uint8_t Control_GetFocus(void) {
    return (uint8_t)g_SystemModel.Light.Focus;
}
