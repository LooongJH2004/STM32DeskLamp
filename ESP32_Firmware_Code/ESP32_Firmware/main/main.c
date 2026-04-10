#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "dev_csi.h"

static const char *TAG = "MAIN_TEST";

// 【请修改为你的 Wi-Fi】
#define MY_WIFI_SSID      "long"
#define MY_WIFI_PASS      "12345678"

// 事件组，用于通知 main 任务 Wi-Fi 已连接
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// ============================================================
// CSI 状态改变回调函数 (模拟台灯业务逻辑)
// ============================================================
static void my_csi_presence_callback(bool is_present) {
    if (is_present) {
        ESP_LOGW(TAG, "=====================================");
        ESP_LOGW(TAG, " 🚶‍♂️ 捕捉到动作！(人靠近或微动)");
        ESP_LOGW(TAG, " 💡 执行：【打开台灯】 (重置关灯倒计时)");
        ESP_LOGW(TAG, "=====================================");
    } else {
        ESP_LOGW(TAG, "=====================================");
        ESP_LOGW(TAG, " 👻 连续 15 秒没有任何微动！");
        ESP_LOGW(TAG, " 📴 执行：【关闭台灯】 (判定人已离开)");
        ESP_LOGW(TAG, "=====================================");
    }
}

// ============================================================
// Wi-Fi 事件处理 (支持断线无限重连)
// ============================================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi Started. Connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi Disconnected. Retrying in 2 seconds...");
        vTaskDelay(pdMS_TO_TICKS(2000)); 
        esp_wifi_connect(); 
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT); 
    }
}

static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = MY_WIFI_SSID,
            .password = MY_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = { .capable = true, .required = false },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 1. 启动 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // 2. 【关键修复】彻底关闭 Wi-Fi 睡眠模式，让天线 100% 保持全开监听！
    // 这将极大提高 CSI 采样的稳定性和数量。
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); 
}

void app_main(void) {
    ESP_LOGI(TAG, "--- CSI Presence Detection Test ---");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi Connected! Now starting CSI...");

    // 启动 CSI 监听
    Dev_CSI_Init(my_csi_presence_callback);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
