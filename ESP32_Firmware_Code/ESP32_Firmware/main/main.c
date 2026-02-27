#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "dev_stm32.h" // [新增]
#include "app_config.h"
#include "service_core.h"
#include "event_bus.h"
#include "data_center.h"

#include "KeyManager.h"
#include "Key.h"

static const char *TAG = "MAIN";

#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

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
        .sample_rate = AUDIO_SAMPLE_RATE
    };
    Dev_Audio_Init(&audio_cfg);

    // [新增] 初始化 STM32 串口通信
    Dev_STM32_Init();

    // 2. 按键初始化
    KeyManager_Init();
    static Key_t s_UserKey; 
    Key_Init(&s_UserKey, 1, BOARD_BUTTON_PIN, 0); 
    KeyManager_Register(&s_UserKey);
    xTaskCreate(Key_Task, "Key_Task", 2048, NULL, 5, NULL);

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
