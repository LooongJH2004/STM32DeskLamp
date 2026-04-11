#include "storage_nvs.h"
#include "data_center.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "Storage_NVS";
#define NVS_NAMESPACE "lamp_cfg"

// 3秒防抖定时器
static TimerHandle_t s_save_timer = NULL;

// 定时器超时回调：真正执行 Flash 写入的地方
static void nvs_save_timer_cb(TimerHandle_t xTimer) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle!");
        return;
    }

    // 1. 获取最新数据
    DC_LightingData_t light;
    DC_SystemData_t sys;
    DataCenter_Get_Lighting(&light);
    DataCenter_Get_System(&sys);

    // 2. 写入 NVS (nvs_set_blob 底层自带 diff 检查，相同数据不会真写 Flash)
    nvs_set_blob(my_handle, "light_cfg", &light, sizeof(DC_LightingData_t));
    nvs_set_blob(my_handle, "sys_cfg", &sys, sizeof(DC_SystemData_t));

    // 3. 提交更改
    nvs_commit(my_handle);
    nvs_close(my_handle);

    ESP_LOGI(TAG, "✅ Data successfully saved to Flash (Debounced).");
}

void Storage_NVS_Init(void) {
    // 创建一个 3000ms 的单次触发定时器
    s_save_timer = xTimerCreate("nvs_timer", pdMS_TO_TICKS(3000), pdFALSE, NULL, nvs_save_timer_cb);
    ESP_LOGI(TAG, "Storage NVS Initialized.");
}

void Storage_NVS_Load_All(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS empty or not found, using default values.");
        return;
    }

    DC_LightingData_t light;
    size_t light_size = sizeof(DC_LightingData_t);
    if (nvs_get_blob(my_handle, "light_cfg", &light, &light_size) == ESP_OK) {
        DataCenter_Set_Lighting(&light);
        ESP_LOGI(TAG, "Loaded Lighting: Pwr:%d, Bri:%d, CCT:%d", light.power, light.brightness, light.color_temp);
    }

    DC_SystemData_t sys;
    size_t sys_size = sizeof(DC_SystemData_t);
    if (nvs_get_blob(my_handle, "sys_cfg", &sys, &sys_size) == ESP_OK) {
        DataCenter_Set_System(&sys);
        ESP_LOGI(TAG, "Loaded System: Vol:%d, ScrBri:%d", sys.volume, sys.screen_brightness);
    }

    nvs_close(my_handle);
}

void Storage_NVS_RequestSave(void) {
    if (s_save_timer != NULL) {
        // 重置定时器。如果 3 秒内再次调用，定时器会重新开始计时！
        xTimerReset(s_save_timer, 0);
        // ESP_LOGD(TAG, "Save requested, timer reset.");
    }
}
