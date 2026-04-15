/**
 * @file    main.c
 * @brief   系统主入口文件
 * @details 负责全栈模块的初始化、FreeRTOS 任务的创建与调度、以及核心事件的串联。
 *          采用双核亲和性调度策略：Core 0 负责网络与逻辑，Core 1 负责音频与 UI。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "manager/mgr_wifi.h"
#include "dev_audio.h"
#include "dev_stm32.h" 
#include "dev_csi.h"       
#include "app_config.h"
#include "service_core.h"
#include "event_bus.h"
#include "data_center.h"
#include "storage_nvs.h"   

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

/** @brief LVGL 全局互斥锁，防止多线程并发访问 UI 导致崩溃 */
SemaphoreHandle_t g_lvgl_mux;

// ============================================================================
// 1. CSI 雷达回调 (联动 DataCenter)
// ============================================================================
/**
 * @brief CSI 人体存在检测状态改变回调函数
 * @param is_present true: 检测到人; false: 无人
 */
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
/**
 * @brief GUI 渲染与触控轮询任务
 * @note  绑定在 Core 1 运行，确保高频 UI 刷新不被 Wi-Fi 协议栈阻塞
 * @param arg 任务参数 (未使用)
 */
static void gui_task(void *arg) {
    ESP_LOGI(TAG, "GUI Task Started on Core 1");
    g_lvgl_mux = xSemaphoreCreateMutex();

    // 初始化底层显示与触摸驱动
    UI_Port_Disp_Init();
    UI_Port_Touch_Init(); 

    // 初始化主界面 UI 组件
    xSemaphoreTake(g_lvgl_mux, portMAX_DELAY);
    UI_Main_Init(); 
    xSemaphoreGive(g_lvgl_mux);

    // LVGL 核心心跳循环
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
/**
 * @brief Wi-Fi 连接成功后的异步初始化任务
 * @note  等待网络就绪后，启动依赖网络的 CSI 雷达和 TTS 语音播报
 * @param pvParameters 任务参数 (未使用)
 */
void on_wifi_connected_task(void *pvParameters) {
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    
    // 阻塞等待 Wi-Fi 状态变为已连接
    while (Mgr_Wifi_GetStatus() != WIFI_STATUS_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Wi-Fi Connected! Starting CSI and TTS Test...");
    
    // 1. 启动 CSI 雷达 (依赖 Wi-Fi Ping 包)
    Dev_CSI_Init(my_csi_presence_callback);

    // 2. 播报开机语音 (依赖网络请求百度 API)
    Dev_Audio_Set_Volume(80); 
    Agent_TTS_Play("网络连接成功，我是你的智能台灯。很高兴为你服务！");
    
    ESP_LOGI(TAG, "Post-WiFi Init Task Finished.");
    vTaskDelete(NULL); // 任务完成后自行销毁
}

// ============================================================================
// 4. 物理按键任务
// ============================================================================
/**
 * @brief 物理按键状态机轮询任务
 * @note  周期性调用 KeyManager_Tick，并将按键事件转化为系统全局事件
 * @param pvParameters 任务参数 (未使用)
 */
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
// 5. PC 串口指令监听任务 (动态配置与调试)
// ============================================================================
/**
 * @brief 串口指令监听任务
 * @note  用于接收用户通过串口助手发送的配置指令 (Wi-Fi 配置、CSI 切换、CRC 注入等)
 * @param arg 任务参数 (未使用)
 */
static void pc_uart_cmd_task(void *arg) {
    ESP_LOGI(TAG, "PC UART Command Listener Started.");
    char line[128];
    
    while (1) {
        // 阻塞读取标准输入 (串口 RX)
        if (fgets(line, sizeof(line), stdin) != NULL) {
            // 移除字符串末尾的换行符
            line[strcspn(line, "\r\n")] = 0;
            
            // 1. 解析 Wi-Fi 名称 (格式: wifiname SSID)
            if (strncmp(line, "wifiname ", 9) == 0) {
                set_wifi_name(line + 9);
            } 
            // 2. 解析 Wi-Fi 密码 (格式: wifipassword PASSWORD)
            else if (strncmp(line, "wifipassword ", 13) == 0) {
                set_wifi_password(line + 13);
            }
            // 3. 手动触发连接指令
            else if (strcmp(line, "wificonnect") == 0) {
                if (is_wifi_info_ready()) {
                    Mgr_Wifi_Connect(get_wifi_name(), get_wifi_password());
                } else {
                    ESP_LOGW(TAG, "WiFi info not ready! Please set name and password first.");
                }
            }
            // 4. CSI 算法动态切换
            else if (strcmp(line, "csi1") == 0) {
                Dev_CSI_Set_Mode(1);
            } else if (strcmp(line, "csi2") == 0) {
                Dev_CSI_Set_Mode(2);
            } else if (strcmp(line, "csi3") == 0) {
                Dev_CSI_Set_Mode(3);
            } 
            // 5. CRC 动态切换指令 (用于误码率拦截测试)
            else if (strcmp(line, "crc0") == 0) {
                Dev_STM32_Set_CRC_Mode(0); // 恢复正常 CRC
            } else if (strcmp(line, "crc1") == 0) {
                Dev_STM32_Set_CRC_Mode(1); // 开启错误 CRC 注入
            } 
            else if (strlen(line) > 0) {
                ESP_LOGW(TAG, "Unknown command: %s", line);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// 6. 系统主函数
// ============================================================================
/**
 * @brief 应用程序入口
 */
void app_main(void) {
    ESP_LOGI(TAG, "=== System Start ===");

    // 0. 底层 NVS 初始化 (Wi-Fi 驱动和持久化存储必须最先执行)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. 核心数据与事件总线初始化
    EventBus_Init();
    DataCenter_Init(); 
    Storage_NVS_Init();      // 初始化防抖定时器
    Storage_NVS_Load_All();  // 从 Flash 加载上次关机前的数据

    // 2. 初始化 Wi-Fi 信息结构体 (清空缓存)
    wifi_information_init();

    // 3. 网络与音频底层初始化
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

    // 4. 启动 GUI 任务 (绑定至 Core 1)
    xTaskCreatePinnedToCore(gui_task, "GUI_Task", 1024 * 8, NULL, 5, NULL, 1);

    // 5. 初始化 STM32 串口通信
    Dev_STM32_Init();

    // 6. 按键初始化与任务创建
    KeyManager_Init();
    static Key_t s_UserKey; 
    Key_Init(&s_UserKey, 1, BOARD_BUTTON_PIN, 0); 
    KeyManager_Register(&s_UserKey);
    xTaskCreatePinnedToCore(Key_Task, "Key_Task", 2048, NULL, 5, NULL, 0);

    // 7. 启动神经中枢 (业务逻辑状态机)
    Service_Core_Init();

    // 8. 启动 PC 串口指令监听任务
    xTaskCreatePinnedToCore(pc_uart_cmd_task, "PC_Cmd_Task", 3072, NULL, 2, NULL, 0);

    // 9. 阻塞等待用户通过串口配置 Wi-Fi
    ESP_LOGW(TAG, "Waiting for WiFi configuration via UART...");
    ESP_LOGW(TAG, "Please input: 'wifiname <ssid>' and 'wifipassword <password>'");
    
    while (!is_wifi_info_ready()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 10. 获取到 Wi-Fi 信息后，发起连接
    ESP_LOGI(TAG, "WiFi info received. Connecting to %s...", get_wifi_name());
    Mgr_Wifi_Connect(get_wifi_name(), get_wifi_password());

    // 11. 启动连网后的异步任务 (雷达 & TTS)
    xTaskCreatePinnedToCore(on_wifi_connected_task, "Post_WiFi", 8192, NULL, 4, NULL, 0);

    // 12. 主循环空转 (周期性打印系统状态)
    int loop_count = 0;
    while(1) {
        if (loop_count % 5 == 0) {
            DataCenter_PrintStatus();
        }
        loop_count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
