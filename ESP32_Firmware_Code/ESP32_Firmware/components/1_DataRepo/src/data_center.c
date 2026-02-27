#include "data_center.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "event_bus.h"
#include "app_config.h" // 引入调试宏
#include <string.h>

static const char *TAG = "DataCenter";

// 内部全局数据树
typedef struct {
    DC_LightingData_t lighting;
    DC_EnvData_t      env;
    DC_TimerData_t    timer;
} GlobalDataTree_t;

static GlobalDataTree_t s_DataTree;
static SemaphoreHandle_t s_Mutex = NULL;

// ============================================================
// 初始化与调试
// ============================================================

void DataCenter_Init(void) {
    if (s_Mutex == NULL) {
        s_Mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    
    // 赋予合理的初始默认值
    s_DataTree.lighting.power = false;
    s_DataTree.lighting.brightness = 50;
    s_DataTree.lighting.color_temp = 50; // 中性光

    s_DataTree.env.indoor_temp = 25;
    s_DataTree.env.indoor_hum = 50;
    strcpy(s_DataTree.env.outdoor_weather, "未知");
    s_DataTree.env.outdoor_temp = 0;

    s_DataTree.timer.state = TIMER_IDLE;
    s_DataTree.timer.remain_sec = 0;
    s_DataTree.timer.total_sec = 0;

    xSemaphoreGive(s_Mutex);
    
    APP_LOGI(TAG, "Data Center Initialized.");
}

void DataCenter_PrintStatus(void) {
#if (APP_DEBUG_PRINT == 1)
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    
    APP_LOGI(TAG, "=== Data Center Status ===");
    APP_LOGI(TAG, "[Light] Power:%d, Bri:%d%%, CCT:%d%%", 
             s_DataTree.lighting.power, s_DataTree.lighting.brightness, s_DataTree.lighting.color_temp);
    APP_LOGI(TAG, "[Env]   InTemp:%dC, InHum:%d%%, OutWeather:%s, OutTemp:%dC", 
             s_DataTree.env.indoor_temp, s_DataTree.env.indoor_hum, 
             s_DataTree.env.outdoor_weather, s_DataTree.env.outdoor_temp);
    APP_LOGI(TAG, "[Timer] State:%d, Remain:%lu s", 
             s_DataTree.timer.state, s_DataTree.timer.remain_sec);
    APP_LOGI(TAG, "==========================");
    
    xSemaphoreGive(s_Mutex);
#endif
}

// ============================================================
// Getter / Setter 实现 (深拷贝 + 事件触发)
// ============================================================

void DataCenter_Get_Lighting(DC_LightingData_t *out_data) {
    if (!out_data) return;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    *out_data = s_DataTree.lighting; // 结构体深拷贝
    xSemaphoreGive(s_Mutex);
}

void DataCenter_Set_Lighting(const DC_LightingData_t *in_data) {
    if (!in_data) return;
    bool changed = false;

    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    // 简单判断是否发生变化 (可根据需要细化)
    if (memcmp(&s_DataTree.lighting, in_data, sizeof(DC_LightingData_t)) != 0) {
        s_DataTree.lighting = *in_data;
        changed = true;
    }
    xSemaphoreGive(s_Mutex);

    // 如果数据变了，抛出事件通知其他模块 (如 LVGL, STM32 串口任务)
    if (changed) {
        EventBus_Send(EVT_DATA_LIGHT_CHANGED, NULL, 0);
        APP_LOGI(TAG, "Lighting Data Updated -> Event Sent");
    }
}

void DataCenter_Get_Env(DC_EnvData_t *out_data) {
    if (!out_data) return;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    *out_data = s_DataTree.env;
    xSemaphoreGive(s_Mutex);
}

void DataCenter_Set_Env(const DC_EnvData_t *in_data) {
    if (!in_data) return;
    bool changed = false;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    if (memcmp(&s_DataTree.env, in_data, sizeof(DC_EnvData_t)) != 0) {
        s_DataTree.env = *in_data;
        changed = true;
    }
    xSemaphoreGive(s_Mutex);
    if (changed) EventBus_Send(EVT_DATA_ENV_CHANGED, NULL, 0);
}

void DataCenter_Get_Timer(DC_TimerData_t *out_data) {
    if (!out_data) return;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    *out_data = s_DataTree.timer;
    xSemaphoreGive(s_Mutex);
}

void DataCenter_Set_Timer(const DC_TimerData_t *in_data) {
    if (!in_data) return;
    bool changed = false;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    if (memcmp(&s_DataTree.timer, in_data, sizeof(DC_TimerData_t)) != 0) {
        s_DataTree.timer = *in_data;
        changed = true;
    }
    xSemaphoreGive(s_Mutex);
    if (changed) EventBus_Send(EVT_DATA_TIMER_CHANGED, NULL, 0);
}
