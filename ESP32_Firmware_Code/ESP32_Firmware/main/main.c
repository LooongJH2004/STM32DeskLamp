#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "dev_stm32.h" 
#include "app_config.h"
#include "service_core.h"
#include "event_bus.h"
#include "data_center.h"

#include "KeyManager.h"
#include "Key.h"
#include "svc_audio.h" 
#include "agents/agent_baidu_tts.h"

// --- 新增 UI 相关头文件 ---
#include "lvgl.h"
#include "ui_port.h"
#include "ui_main.h"

static const char *TAG = "MAIN";

#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

// LVGL 全局互斥锁 (LVGL 的 API 不是线程安全的，跨任务调用必须加锁)
SemaphoreHandle_t g_lvgl_mux;

// ============================================================================
// [新增] LVGL 渲染与心跳任务 (运行在 Core 1)
// ============================================================================
static void gui_task(void *arg) {
    ESP_LOGI(TAG, "GUI Task Started on Core 1");

    // 1. 创建 LVGL 互斥锁
    g_lvgl_mux = xSemaphoreCreateMutex();

    // 2. 初始化屏幕底层与 LVGL
    UI_Port_Disp_Init();
    UI_Port_Touch_Init(); // <--- 新增：初始化触摸

    // 3. 构建主界面 (加锁)
    xSemaphoreTake(g_lvgl_mux, portMAX_DELAY);
    UI_Main_Init(); // <--- 替换在这里
    xSemaphoreGive(g_lvgl_mux);

    // 4. LVGL 核心循环
    while (1) {
        // 延时 10ms
        vTaskDelay(pdMS_TO_TICKS(10));

        // 告诉 LVGL 过去了 10ms (如果你在 lv_conf.h 中开启了 LV_TICK_CUSTOM，这句可以注释掉，但保留也无妨)
        lv_tick_inc(10);

        // 运行 LVGL 渲染引擎 (加锁)
        if (xSemaphoreTake(g_lvgl_mux, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(g_lvgl_mux);
        }
    }
}

// ============================================================================
// 原有的测试任务与按键任务
// ============================================================================
void tts_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    while (Mgr_Wifi_GetStatus() != WIFI_STATUS_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Wi-Fi Connected! Starting TTS Test...");
    Dev_Audio_Set_Volume(80); 
    Agent_TTS_Play("网络连接成功，我是你的智能台灯。很高兴为你服务！");
    ESP_LOGI(TAG, "TTS Test Task Finished.");
    vTaskDelete(NULL); 
}

void Key_Task(void *pvParameters) {
    ESP_LOGI(TAG, "Key Task Started");
    KeyEvent_t key_evt;
    while (1) {
        KeyManager_Tick();
        if (KeyManager_GetEvent(&key_evt)) {
            switch (key_evt.Type) {
                case KEY_EVT_CLICK:
                    ESP_LOGI(TAG, "Physical Key Click!");
                    EventBus_Send(EVT_KEY_CLICK, NULL, 0);
                    break;
                case KEY_EVT_DOUBLE_CLICK:
                    ESP_LOGI(TAG, "Physical Key Double Click!");
                    EventBus_Send(EVT_KEY_DOUBLE_CLICK, NULL, 0);
                    break;
                case KEY_EVT_HOLD_START:
                    ESP_LOGI(TAG, "Physical Key Hold Start");
                    EventBus_Send(EVT_KEY_LONG_PRESS, NULL, 0);
                    break;
                default:
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "System Start...");

    // 1. 基础初始化
    DataCenter_Init(); 
    Mgr_Wifi_Init();
    
    Audio_Config_t audio_cfg = {
        .bck_io_num = AUDIO_I2S_BCK_PIN,
        .ws_io_num = AUDIO_I2S_WS_PIN,
        .data_in_num = AUDIO_I2S_DATA_IN_PIN,
        .data_out_num = AUDIO_I2S_DATA_OUT_PIN, 
        .sample_rate = AUDIO_SAMPLE_RATE
    };
    Dev_Audio_Init(&audio_cfg);
    Svc_Audio_Init();

    // [新增] 启动 GUI 任务，绑定到 Core 1，优先级设为 5 (较高)
    xTaskCreatePinnedToCore(gui_task, "GUI_Task", 1024 * 8, NULL, 5, NULL, 1);

    // 启动 TTS 测试任务 (Core 0)
    xTaskCreatePinnedToCore(tts_test_task, "TTS_Test", 8192, NULL, 4, NULL, 0);

    // 初始化 STM32 串口通信
    Dev_STM32_Init();

    // 2. 按键初始化
    KeyManager_Init();
    static Key_t s_UserKey; 
    Key_Init(&s_UserKey, 1, BOARD_BUTTON_PIN, 0); 
    KeyManager_Register(&s_UserKey);
    xTaskCreatePinnedToCore(Key_Task, "Key_Task", 2048, NULL, 5, NULL, 0);

    // 3. 启动神经中枢
    Service_Core_Init();

    // 4. 启动网络连接
    Mgr_Wifi_Connect(MY_WIFI_SSID, MY_WIFI_PASS);

    // 5. 主循环空转
    int loop_count = 0;
    while(1) {
        if (loop_count % 5 == 0) {
            DataCenter_PrintStatus();
        }
        loop_count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
