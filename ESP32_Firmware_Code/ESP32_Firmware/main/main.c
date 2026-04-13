#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "dev_stm32.h" 
#include "dev_csi.h"       // [新增] 雷达
#include "app_config.h"
#include "service_core.h"
#include "event_bus.h"
#include "data_center.h"
#include "storage_nvs.h"   // [新增] 掉电保存

#include "KeyManager.h"
#include "Key.h"
#include "svc_audio.h" 
#include "agents/agent_baidu_tts.h"

// --- UI 相关头文件 ---
#include "lvgl.h"
#include "ui_port.h"
#include "ui_main.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "MAIN";

#define MY_WIFI_SSID      "CMCC-2079"
#define MY_WIFI_PASS      "88888888"

SemaphoreHandle_t g_lvgl_mux;

// ============================================================================
// 1. CSI 雷达回调 (联动 DataCenter)
// ============================================================================
static void my_csi_presence_callback(bool is_present) {
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);

    if (is_present) {
        ESP_LOGW(TAG, "🚶‍♂️ 捕捉到动作！ -> 执行【开灯】");
        light.power = true;
    } else {
        ESP_LOGW(TAG, "👻 连续 15 秒无人！ -> 执行【关灯】");
        light.power = false;
    }
    
    // 写入数据中心 (内部会自动触发 3 秒防抖保存，并同步给 UI 和 STM32)
    DataCenter_Set_Lighting(&light);
}

// ============================================================================
// 2. LVGL 渲染任务 (Core 1)
// ============================================================================
static void gui_task(void *arg) {
    ESP_LOGI(TAG, "GUI Task Started on Core 1");
    g_lvgl_mux = xSemaphoreCreateMutex();

    UI_Port_Disp_Init();
    UI_Port_Touch_Init(); 

    xSemaphoreTake(g_lvgl_mux, portMAX_DELAY);
    UI_Main_Init(); 
    xSemaphoreGive(g_lvgl_mux);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
        if (xSemaphoreTake(g_lvgl_mux, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(g_lvgl_mux);
        }
    }
}

// ============================================================================
// 3. Wi-Fi 连接后的初始化任务 (启动雷达 & TTS 测试)
// ============================================================================
void on_wifi_connected_task(void *pvParameters) {
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    while (Mgr_Wifi_GetStatus() != WIFI_STATUS_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Wi-Fi Connected! Starting CSI and TTS Test...");
    
    // 1. 启动 CSI 雷达
    Dev_CSI_Init(my_csi_presence_callback);

    // 2. 播报开机语音
    Dev_Audio_Set_Volume(80); 
    Agent_TTS_Play("网络连接成功，我是你的智能台灯。很高兴为你服务！");
    
    ESP_LOGI(TAG, "Post-WiFi Init Task Finished.");
    vTaskDelete(NULL); 
}

// ============================================================================
// 4. 物理按键任务
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

// ============================================================================
// [新增] PC 串口指令监听任务 (用于动态切换 CSI 算法)
// ============================================================================
static void pc_uart_cmd_task(void *arg) {
    ESP_LOGI(TAG, "PC UART Command Listener Started. Type 'csi1', 'csi2', or 'csi3' to switch modes.");
    char line[128];
    
    while (1) {
        // 阻塞读取标准输入 (PC 串口助手发来的数据)
        if (fgets(line, sizeof(line), stdin) != NULL) {
            // 去除换行符
            line[strcspn(line, "\r\n")] = 0;
            
            if (strcmp(line, "csi1") == 0) {
                Dev_CSI_Set_Mode(1);
            } else if (strcmp(line, "csi2") == 0) {
                Dev_CSI_Set_Mode(2);
            } else if (strcmp(line, "csi3") == 0) {
                Dev_CSI_Set_Mode(3);
            } else if (strlen(line) > 0) {
                ESP_LOGW(TAG, "Unknown command: %s", line);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// ============================================================================
// 5. 主函数
// ============================================================================
void app_main(void) {
    ESP_LOGI(TAG, "=== System Start ===");

    // 0. 底层 NVS 初始化 (必须最先执行)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. 核心数据与事件总线
    EventBus_Init();
    DataCenter_Init(); 
    Storage_NVS_Init();      // 初始化防抖定时器
    Storage_NVS_Load_All();  // 从 Flash 加载上次关机前的数据

    // 2. 网络与音频底层
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

    // 3. 启动 GUI 任务 (Core 1)
    xTaskCreatePinnedToCore(gui_task, "GUI_Task", 1024 * 8, NULL, 5, NULL, 1);

    // 4. 初始化 STM32 串口通信
    Dev_STM32_Init();

    // 5. 按键初始化
    KeyManager_Init();
    static Key_t s_UserKey; 
    Key_Init(&s_UserKey, 1, BOARD_BUTTON_PIN, 0); 
    KeyManager_Register(&s_UserKey);
    xTaskCreatePinnedToCore(Key_Task, "Key_Task", 2048, NULL, 5, NULL, 0);

    // 6. 启动神经中枢 (业务逻辑状态机)
    Service_Core_Init();

    // 7. 启动网络连接
    Mgr_Wifi_Connect(MY_WIFI_SSID, MY_WIFI_PASS);

    // 8. 启动连网后的异步任务 (雷达 & TTS)
    xTaskCreatePinnedToCore(on_wifi_connected_task, "Post_WiFi", 8192, NULL, 4, NULL, 0);

    // [新增] 启动 PC 串口指令监听任务
    xTaskCreatePinnedToCore(pc_uart_cmd_task, "PC_Cmd_Task", 3072, NULL, 2, NULL, 0);


    // 9. 主循环空转
    int loop_count = 0;
    while(1) {
        if (loop_count % 5 == 0) {
            DataCenter_PrintStatus();
        }
        loop_count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
