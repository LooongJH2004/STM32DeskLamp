#include "manager/mgr_wifi.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "event_bus.h" // [NEW] 引入事件总线

static const char *TAG = "Mgr_Wifi";

// 状态记录
static Wifi_Status_t s_wifi_status = WIFI_STATUS_IDLE;
static int s_retry_num = 0;
#define MAX_RETRY_COUNT 5

// SNTP 时间同步状态
static bool s_time_synced = false;

// --- SNTP 回调 ---
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time Synced Notification!");
    s_time_synced = true;
    
    // 发送事件通知系统
    EventBus_Send(EVT_TIME_SYNCED, NULL, 0);
    
    // 打印当前时间
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current Time: %s", strftime_buf);
}

static void _init_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_stop(); // 确保干净重启
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // 使用国内源，确保成功率
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "ntp.tencent.com");
    esp_sntp_setservername(2, "pool.ntp.org");
    
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

// --- Wi-Fi 事件处理 ---
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    
    // 1. Wi-Fi 启动 -> 开始连接
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_wifi_status = WIFI_STATUS_IDLE;
    } 
    // 2. 连接断开 -> 重连
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_status = WIFI_STATUS_DISCONNECTED;
        s_time_synced = false; 
        EventBus_Send(EVT_NET_DISCONNECTED, NULL, 0); // 发送断开事件
        
        if (s_retry_num < MAX_RETRY_COUNT) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, MAX_RETRY_COUNT);
        } else {
            ESP_LOGE(TAG, "Connect to the AP failed. Please check SSID/PWD.");
            s_wifi_status = WIFI_STATUS_ERROR;
            EventBus_Send(EVT_SYS_ERROR, (void*)WIFI_STATUS_ERROR, 0); // 发送错误事件
        }
    } 
    // 3. 获取 IP -> 连接成功
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_status = WIFI_STATUS_CONNECTED;
        
        // 发送连接成功事件
        EventBus_Send(EVT_NET_CONNECTED, NULL, 0);
        
        // 关键：拿到 IP 后，立即启动 SNTP
        _init_sntp();
    }
}

// --- 接口实现 ---

void Mgr_Wifi_Init(void) {
    // 0. NVS 初始化 (Wi-Fi 驱动需要)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. 网络接口初始化
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. Wi-Fi 驱动初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    
    ESP_LOGI(TAG, "Mgr_Wifi Initialized.");
}

void Mgr_Wifi_Connect(const char *ssid, const char *password) {
    wifi_config_t wifi_config = {0};
    
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG, "Connecting to SSID: %s ...", ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 如果已经在运行，调用 start 后需要手动 connect
    esp_wifi_connect();
}

Wifi_Status_t Mgr_Wifi_GetStatus(void) {
    return s_wifi_status;
}

bool Mgr_Wifi_IsTimeSynced(void) {
    return s_time_synced;
}
