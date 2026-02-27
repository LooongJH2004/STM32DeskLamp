#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "app_config.h"
#include "service_core.h"
#include "event_bus.h"

// 引入按键库
#include "KeyManager.h"
#include "Key.h"

static const char *TAG = "MAIN";

#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

// --- 按键扫描任务 ---
// 负责周期性调用 KeyManager_Tick 并将事件转发到 EventBus
void Key_Task(void *pvParameters) {
    ESP_LOGI(TAG, "Key Task Started");
    
    KeyEvent_t key_evt;
    
    while (1) {
        // 1. 驱动核心逻辑 (建议 10ms 周期)
        KeyManager_Tick();
        
        // 2. 检查是否有事件产生
        if (KeyManager_GetEvent(&key_evt)) {
            // 3. 转发到系统 EventBus
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
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 轮询间隔
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "System Start...");

    // 1. 基础初始化
    Mgr_Wifi_Init();
    
    Audio_Config_t audio_cfg = {
        .bck_io_num = AUDIO_I2S_BCK_PIN,
        .ws_io_num = AUDIO_I2S_WS_PIN,
        .data_in_num = AUDIO_I2S_DATA_IN_PIN,
        .sample_rate = AUDIO_SAMPLE_RATE
    };
    Dev_Audio_Init(&audio_cfg);

    // 2. 按键初始化
    KeyManager_Init();
    static Key_t s_UserKey; // 改个名字，叫 UserKey 更合适

    // 参数说明: 
    // &s_UserKey: 按键对象
    // 1: ID (用于区分不同按键)
    // BOARD_BUTTON_PIN: 引脚号 (21)
    // 0: 有效电平 (0表示按下是低电平，驱动会自动开启内部上拉电阻)
    Key_Init(&s_UserKey, 1, BOARD_BUTTON_PIN, 0); 
    
    KeyManager_Register(&s_UserKey);
    
    // 创建按键扫描任务 (保持不变)
    xTaskCreate(Key_Task, "Key_Task", 2048, NULL, 5, NULL);

    // 3. 启动神经中枢
    Service_Core_Init();

    // 4. 启动网络连接
    Mgr_Wifi_Connect(MY_WIFI_SSID, MY_WIFI_PASS);

    // 5. 主循环空转 (任务都在后台运行)
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
