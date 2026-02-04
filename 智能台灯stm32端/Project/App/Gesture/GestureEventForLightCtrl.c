/**
  ******************************************************************************
  * @file    GestureEventForLightCtrl.c
  * @author  不知道是谁
  * @version V1.0
  * @date    2025-01-22
  * @brief   手势事件到灯光控制的映射实现
  * @note    
  *          【功能映射】
  *          - 上挥 ↑: 亮度增加
  *          - 下挥 ↓: 亮度减少
  *          - 左挥 ←: 色温变暖 (减少)
  *          - 右挥 →: 色温变冷 (增加)
  *          - 前推: 开灯 (亮度设为最大)
  *          - 后拉: 关灯 (亮度设为0)
  *          - 顺时针/逆时针: 预留
  *          - 挥手: 预留
  ******************************************************************************
  */
#include "PAJ7620.h"
#include "SystemModel.h"
#include "LightCtrl.h"
#include "OLED.h"
#include <stdio.h>

// 在 GestureEventForLightCtrl.c 顶部
//#include "LED.h"  // 直接使用 LED 驱动

/* ============================================================
 *                      私有宏定义
 * ============================================================ */

/** @brief 手势调节步进值 */
#define GESTURE_BRIGHTNESS_STEP     100     /*!< 亮度调节步进 (0-1000) */
#define GESTURE_COLORTEMP_STEP      100     /*!< 色温调节步进 (0-1000) */

/* ============================================================
 *                      私有函数
 * ============================================================ */

/**
  * @brief  限制数值在有效范围内
  * @param  value: 输入值
  * @param  min: 最小值
  * @param  max: 最大值
  * @retval 限制后的值
  */
static int16_t Clamp(int16_t value, int16_t min, int16_t max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
  * @brief  更新 LED 输出 (根据当前模型)
  * @note   内部函数，手势调节后立即更新灯光
  */
static void UpdateLightOutput(void)
{
    uint16_t warm, cold;
    float bri_factor = g_SystemModel.Light.Brightness / 1000.0f;
    float cct_factor = g_SystemModel.Light.ColorTemp / 1000.0f;

    warm = (uint16_t)((1.0f - cct_factor) * 1000 * bri_factor);
    cold = (uint16_t)(cct_factor * 1000 * bri_factor);

    LED_SetDualColor(warm, cold);
}

/**
  * @brief  显示手势提示 (OLED 第4行)
  * @param  msg: 提示信息
  */
static void ShowGestureHint(const char* msg)
{
    /* 清除第4行并显示新信息 */
    OLED_ShowString(4, 1, "                ");  /* 16个空格清行 */
    OLED_ShowString(4, 1, (char*)msg);
}

/* ============================================================
 *                 Hook 函数实现
 * ============================================================ */

/**
  * @brief  上挥手势: 亮度增加
  */
void PAJ7620_Hook_OnUp(void)
{
    printf("[手势] ↑ 上挥 - 亮度增加\r\n");
    
    g_SystemModel.Light.Brightness += GESTURE_BRIGHTNESS_STEP;
    g_SystemModel.Light.Brightness = Clamp(g_SystemModel.Light.Brightness, 0, 1000);
    
    UpdateLightOutput();
    ShowGestureHint("Up: Bri+");
    
    printf("       当前亮度: %d\r\n", g_SystemModel.Light.Brightness);
}

/**
  * @brief  下挥手势: 亮度减少
  */
void PAJ7620_Hook_OnDown(void)
{
    printf("[手势] ↓ 下挥 - 亮度减少\r\n");
    
    g_SystemModel.Light.Brightness -= GESTURE_BRIGHTNESS_STEP;
    g_SystemModel.Light.Brightness = Clamp(g_SystemModel.Light.Brightness, 0, 1000);
    
    UpdateLightOutput();
    ShowGestureHint("Down: Bri-");
    
    printf("       当前亮度: %d\r\n", g_SystemModel.Light.Brightness);
}

/**
  * @brief  左挥手势: 色温变暖 (偏黄)
  */
void PAJ7620_Hook_OnLeft(void)
{
    printf("[手势] ← 左挥 - 色温变暖\r\n");
    
    g_SystemModel.Light.ColorTemp -= GESTURE_COLORTEMP_STEP;
    g_SystemModel.Light.ColorTemp = Clamp(g_SystemModel.Light.ColorTemp, 0, 1000);
    
    UpdateLightOutput();
    ShowGestureHint("Left: Warm");
    
    printf("       当前色温: %d\r\n", g_SystemModel.Light.ColorTemp);
}

/**
  * @brief  右挥手势: 色温变冷 (偏白)
  */
void PAJ7620_Hook_OnRight(void)
{
    printf("[手势] → 右挥 - 色温变冷\r\n");
    
    g_SystemModel.Light.ColorTemp += GESTURE_COLORTEMP_STEP;
    g_SystemModel.Light.ColorTemp = Clamp(g_SystemModel.Light.ColorTemp, 0, 1000);
    
    UpdateLightOutput();
    ShowGestureHint("Right: Cool");
    
    printf("       当前色温: %d\r\n", g_SystemModel.Light.ColorTemp);
}

/**
  * @brief  前推手势: 开灯
  */
void PAJ7620_Hook_OnForward(void)
{
    printf("[手势] ○ 前推 - 开灯\r\n");
    
    /* 如果当前亮度为0，则恢复到默认亮度 */
    if (g_SystemModel.Light.Brightness == 0)
    {
        g_SystemModel.Light.Brightness = 500;
    }
    
    /* 也可以设置为最大亮度 */
    // g_SystemModel.Light.Brightness = 1000;
    
    UpdateLightOutput();
    ShowGestureHint("Forward: ON");
    
    printf("       灯光已开启，亮度: %d\r\n", g_SystemModel.Light.Brightness);
}

/**
  * @brief  后拉手势: 关灯
  */
void PAJ7620_Hook_OnBackward(void)
{
    printf("[手势] ○ 后拉 - 关灯\r\n");
    
    g_SystemModel.Light.Brightness = 0;
    
    UpdateLightOutput();
    ShowGestureHint("Backward: OFF");
    
    printf("       灯光已关闭\r\n");
}

/**
  * @brief  顺时针旋转手势: 预留
  */
void PAJ7620_Hook_OnClockwise(void)
{
    printf("[手势] ↻ 顺时针旋转\r\n");
    
    ShowGestureHint("Clockwise");
    
    /* 预留功能: 可用于切换模式等 */
}

/**
  * @brief  逆时针旋转手势: 预留
  */
void PAJ7620_Hook_OnCounterClockwise(void)
{
    printf("[手势] ↺ 逆时针旋转\r\n");
    
    ShowGestureHint("Counter-CW");
    
    /* 预留功能: 可用于切换模式等 */
}

/**
  * @brief  挥手手势: 预留
  */
void PAJ7620_Hook_OnWave(void)
{
    printf("[手势] 👋 挥手\r\n");
    
    ShowGestureHint("Wave!");
    
    /* 预留功能: 可用于唤醒/休眠等 */
}
