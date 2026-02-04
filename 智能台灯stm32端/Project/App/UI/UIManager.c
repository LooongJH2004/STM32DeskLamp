/**
  * @file    UIManager.c
  * @brief   UI 管理器实现 (V6.3 Force Refresh)
  */
#include "UIManager.h"
#include "SystemModel.h"
#include "OLED.h"
#include <stdio.h>
#include <string.h>

static SystemModel_t s_LastModel;

static void UI_Draw_Home_Page(void);

void UIManager_Init(void)
{
    OLED_Init();
    
    // 强制清屏并显示标题，不检查 IsReady
    OLED_Clear();
    OLED_ShowString(1, 1, "--- SMART LAMP ---");

    // 初始化上一帧数据为非法值，确保第一次进入 Task 时强制全屏刷新
    memset(&s_LastModel, 0xFF, sizeof(SystemModel_t));
}

void UIManager_Task(void)
{
    // 【修改点】暂时移除离线检测，强制刷新，排除 I2C ACK 失败导致的黑屏
    // if (OLED_IsReady() == 0) return;

    UI_Draw_Home_Page();
    s_LastModel = g_SystemModel;
}

static void UI_Draw_Home_Page(void)
{
    char str[18];
    SystemModel_t *curr = &g_SystemModel;
    SystemModel_t *last = &s_LastModel;

    // --- 1. 刷新亮度 ---
    if (curr->Light.Brightness != last->Light.Brightness || 
        curr->Light.Focus != last->Light.Focus)
    {
        sprintf(str, "Bri: %-4d  %s", 
                curr->Light.Brightness, 
                (curr->Light.Focus == FOCUS_BRIGHTNESS) ? "[F]" : "   ");
        OLED_ShowString(2, 1, str);
    }

    // --- 2. 刷新色温 ---
    if (curr->Light.ColorTemp != last->Light.ColorTemp || 
        curr->Light.Focus != last->Light.Focus)
    {
        sprintf(str, "CCT: %-4d  %s", 
                curr->Light.ColorTemp, 
                (curr->Light.Focus == FOCUS_COLOR_TEMP) ? "[F]" : "   ");
        OLED_ShowString(3, 1, str);
    }

    // --- 3. 刷新环境数据 ---
    if (curr->Sensor.Temperature != last->Sensor.Temperature || 
        curr->Sensor.Humidity != last->Sensor.Humidity ||
        curr->Sensor.Lux != last->Sensor.Lux)
    {
        if (curr->Sensor.Temperature <= -90.0f)
        {
            OLED_ShowString(4, 1, "Sensor Error!   ");
        }
        else
        {
            int lux_percent = (int)(curr->Sensor.Lux / 10.0f);
            if(lux_percent > 100) lux_percent = 100;

            sprintf(str, "%2.0fC %2.0f%% L:%3d%%", 
                    curr->Sensor.Temperature, 
                    curr->Sensor.Humidity,
                    lux_percent);
            OLED_ShowString(4, 1, str);
        }
    }
}
