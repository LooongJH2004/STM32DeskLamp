#include "data_center.h"
#include "storage_nvs.h" // [新增] 引入 NVS 存储模块
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "event_bus.h"
#include "app_config.h"
#include <string.h>

static const char *TAG = "DataCenter";

typedef struct {
    DC_LightingData_t lighting;
    DC_SystemData_t   system; // [新增]
    DC_EnvData_t      env;
    DC_TimerData_t    timer;
} GlobalDataTree_t;

static GlobalDataTree_t s_DataTree;
static SemaphoreHandle_t s_Mutex = NULL;

void DataCenter_Init(void) {
    if (s_Mutex == NULL) s_Mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    
    // 默认值 (如果 NVS 中有数据，稍后会被覆盖)
    s_DataTree.lighting.power = true; 
    s_DataTree.lighting.brightness = 50;
    s_DataTree.lighting.color_temp = 50; 

    s_DataTree.system.volume = 50;            // 默认音量 50%
    s_DataTree.system.screen_brightness = 80; // 默认屏幕亮度 80%

    s_DataTree.env.indoor_temp = 25;
    s_DataTree.env.indoor_hum = 50;
    s_DataTree.env.indoor_lux = 0; 
    strcpy(s_DataTree.env.outdoor_weather, "未知");
    s_DataTree.env.outdoor_temp = 0;

    s_DataTree.timer.state = TIMER_IDLE;
    s_DataTree.timer.remain_sec = 0;
    s_DataTree.timer.total_sec = 0;

    xSemaphoreGive(s_Mutex);
    APP_LOGI(TAG, "Data Center Initialized.");
}

// --- Lighting ---
void DataCenter_Get_Lighting(DC_LightingData_t *out_data) {
    if (!out_data) return;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    *out_data = s_DataTree.lighting; 
    xSemaphoreGive(s_Mutex);
}

void DataCenter_Set_Lighting(const DC_LightingData_t *in_data) {
    if (!in_data) return;
    bool changed = false;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    if (memcmp(&s_DataTree.lighting, in_data, sizeof(DC_LightingData_t)) != 0) {
        s_DataTree.lighting = *in_data;
        changed = true;
    }
    xSemaphoreGive(s_Mutex);

    if (changed) {
        EventBus_Send(EVT_DATA_LIGHT_CHANGED, NULL, 0);
        Storage_NVS_RequestSave(); // [关键] 触发防抖保存
    }
}

// --- System [新增] ---
void DataCenter_Get_System(DC_SystemData_t *out_data) {
    if (!out_data) return;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    *out_data = s_DataTree.system; 
    xSemaphoreGive(s_Mutex);
}

void DataCenter_Set_System(const DC_SystemData_t *in_data) {
    if (!in_data) return;
    bool changed = false;
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    if (memcmp(&s_DataTree.system, in_data, sizeof(DC_SystemData_t)) != 0) {
        s_DataTree.system = *in_data;
        changed = true;
    }
    xSemaphoreGive(s_Mutex);

    if (changed) {
        EventBus_Send(EVT_DATA_SYS_CHANGED, NULL, 0);
        Storage_NVS_RequestSave(); // [关键] 触发防抖保存
    }
}

// --- Env & Timer (保持不变，且不触发 NVS 保存) ---
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

void DataCenter_PrintStatus(void) {
#if (APP_DEBUG_PRINT == 1)
    xSemaphoreTake(s_Mutex, portMAX_DELAY);
    
    APP_LOGI(TAG, "=== Data Center Status ===");
    APP_LOGI(TAG, "[Light] Power:%d, Bri:%d%%, CCT:%d%%", 
             s_DataTree.lighting.power, s_DataTree.lighting.brightness, s_DataTree.lighting.color_temp);
    APP_LOGI(TAG, "[Env]   InTemp:%dC, InHum:%d%%, InLux:%d, OutWeather:%s, OutTemp:%dC", 
             s_DataTree.env.indoor_temp, s_DataTree.env.indoor_hum, s_DataTree.env.indoor_lux,
             s_DataTree.env.outdoor_weather, s_DataTree.env.outdoor_temp);
    APP_LOGI(TAG, "[Timer] State:%d, Remain:%lu s", 
             s_DataTree.timer.state, s_DataTree.timer.remain_sec);
    APP_LOGI(TAG, "==========================");
    
    xSemaphoreGive(s_Mutex);
#endif
}
