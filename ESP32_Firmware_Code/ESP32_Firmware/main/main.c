#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#include "agents/agent_baidu_tts.h" // [新增] 引入 TTS 代理

static const char *TAG = "MAIN";

#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

// ============================================================================
// [新增] 开机播报测试任务 (运行在 Core 0)
// ============================================================================
void tts_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    
    // 阻塞等待 Wi-Fi 连接成功
    while (Mgr_Wifi_GetStatus() != WIFI_STATUS_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 稍微等 1 秒，让网络底层彻底稳定
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Wi-Fi Connected! Starting TTS Test...");
    
    // 设置一个合适的音量 (0-100)
    Dev_Audio_Set_Volume(30); 
    
    // 触发流式语音合成与播放 (阻塞函数)
    Agent_TTS_Play("网络连接成功，我是你的智能台灯。很高兴为你服务！");
    
    ESP_LOGI(TAG, "TTS Test Task Finished.");
    vTaskDelete(NULL); // 测试完毕，销毁任务
}
// ============================================================================

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

    // 初始化音频服务 (内部会创建运行在 Core 1 的播放任务)
    Svc_Audio_Init();

    // [新增] 启动 TTS 测试任务
    xTaskCreatePinnedToCore(tts_test_task, "TTS_Test", 8192, NULL, 5, NULL, 0);

    // 初始化 STM32 串口通信
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
