#include "dev_csi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "Dev_CSI";

// ============================================================
// --- 算法灵敏度配置 ---
// ============================================================
#define CSI_MOTION_THRESHOLD    250   // 【待标定】截尾滤波后的阈值 (建议设在静止和微动之间)
#define CSI_LEAVE_TIMEOUT_MS    15000 // 离开判定超时 (15秒)

static csi_presence_cb_t s_app_cb = NULL;

static volatile uint32_t s_last_active_tick = 0;
static bool s_is_present = false;                   

// [新增] 记录每个包的独立得分，用于排序滤波
#define SCORE_HISTORY_MAX 32
static volatile uint32_t s_score_history[SCORE_HISTORY_MAX];
static volatile int s_score_count = 0;

#define MAX_SUBCARRIERS 128
static uint16_t s_baseline_shape[MAX_SUBCARRIERS] = {0};
static bool s_baseline_init = false;

// ============================================================
// 1. Wi-Fi 底层 CSI 数据接收回调
// ============================================================
static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf || info->len < 2) return;

    int subcarrier_count = info->len / 2;
    if (subcarrier_count > MAX_SUBCARRIERS) subcarrier_count = MAX_SUBCARRIERS;
    if (subcarrier_count == 0) return;

    int8_t *data = (int8_t *)info->buf;
    uint32_t total_amp = 0;
    uint16_t amps[MAX_SUBCARRIERS];
    
    for (int i = 0; i < subcarrier_count; i++) {
        amps[i] = abs(data[i * 2]) + abs(data[i * 2 + 1]);
        total_amp += amps[i];
    }

    uint32_t mean_amp = total_amp / subcarrier_count;
    if (mean_amp == 0) return; 

    uint32_t total_shape_diff = 0;
    for (int i = 0; i < subcarrier_count; i++) {
        uint16_t normalized_amp = (amps[i] * 100) / mean_amp;

        if (!s_baseline_init) {
            s_baseline_shape[i] = normalized_amp;
        } else {
            int diff = abs((int)normalized_amp - (int)s_baseline_shape[i]);
            total_shape_diff += (diff * diff); // 平方放大
            s_baseline_shape[i] = (s_baseline_shape[i] * 15 + normalized_amp) / 16;
        }
    }

    if (!s_baseline_init) {
        s_baseline_init = true;
        return;
    }

    uint32_t packet_score = total_shape_diff / subcarrier_count;

    // 将每个包的得分独立存入数组
    if (s_score_count < SCORE_HISTORY_MAX) {
        s_score_history[s_score_count++] = packet_score;
    }
}

// ============================================================
// 2. CSI 状态监控独立任务 (截尾滤波 + 连续确认)
// ============================================================
static void csi_monitor_task(void *arg) {
    ESP_LOGI(TAG, "CSI Monitor Task Started. [Trimmed Mean Filter Mode]");
    
    int consecutive_motion_count = 0; // 连续动作计数器

    while (1) {
        uint32_t now = xTaskGetTickCount();

        // 1. 提取并清空缓冲区
        uint32_t local_scores[SCORE_HISTORY_MAX];
        int count = s_score_count;
        for (int i = 0; i < count; i++) {
            local_scores[i] = s_score_history[i];
        }
        s_score_count = 0;

        uint32_t final_score = 0;

        if (count > 0) {
            // 2. 冒泡排序 (从小到大)
            for (int i = 0; i < count - 1; i++) {
                for (int j = i + 1; j < count; j++) {
                    if (local_scores[i] > local_scores[j]) {
                        uint32_t temp = local_scores[i];
                        local_scores[i] = local_scores[j];
                        local_scores[j] = temp;
                    }
                }
            }

            // 3. 截尾均值 (掐头去尾)
            int start_idx = 0;
            int end_idx = count;
            
            // 如果包数大于等于 6 个，去掉最高 20% 和最低 20%
            if (count >= 6) {
                int trim = count / 5; // 比如 10个包，trim=2
                start_idx = trim;
                end_idx = count - trim;
            }

            uint32_t sum = 0;
            for (int i = start_idx; i < end_idx; i++) {
                sum += local_scores[i];
            }
            final_score = sum / (end_idx - start_idx);
        }

        ESP_LOGI(TAG, "[雷达] 采样包数: %2d | 滤波后得分: %4lu | 阈值: %d", count, final_score, CSI_MOTION_THRESHOLD);

        // 4. 动作判定 (加入连续确认防抖)
        if (count > 5 && final_score > CSI_MOTION_THRESHOLD) {
            consecutive_motion_count++;
            // 必须连续 2 个窗口 (1秒) 都超过阈值，才判定为有人
            if (consecutive_motion_count >= 2) {
                s_last_active_tick = now; 
                if (!s_is_present) {
                    s_is_present = true;
                    if (s_app_cb) s_app_cb(true); 
                }
            }
        } else {
            consecutive_motion_count = 0; // 一旦低于阈值，计数器清零
        }

        // 5. 超时判定 (离开)
        if (s_is_present) {
            uint32_t elapsed_ticks = (now >= s_last_active_tick) ? (now - s_last_active_tick) : 0;
            if (elapsed_ticks * portTICK_PERIOD_MS > CSI_LEAVE_TIMEOUT_MS) {
                s_is_present = false;
                if (s_app_cb) s_app_cb(false); 
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

// ============================================================
// 3. 启动官方 Ping 雷达
// ============================================================
static void start_ping_emitter(void) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.gw.addr == 0) return;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr.type = ESP_IPADDR_TYPE_V4;
    ping_config.target_addr.u_addr.ip4.addr = ip_info.gw.addr; 
    ping_config.count = ESP_PING_COUNT_INFINITE;               
    ping_config.interval_ms = 50;                              
    ping_config.data_size = 32;                                

    esp_ping_handle_t ping;
    if (esp_ping_new_session(&ping_config, NULL, &ping) == ESP_OK) {
        esp_ping_start(ping);
    }
}

// ============================================================
// 4. 初始化函数
// ============================================================
void Dev_CSI_Init(csi_presence_cb_t cb) {
    s_app_cb = cb;
    s_is_present = false;
    s_last_active_tick = xTaskGetTickCount();
    s_score_count = 0;
    s_baseline_init = false;

    xTaskCreate(csi_monitor_task, "csi_monitor", 4096, NULL, 5, NULL);

    wifi_csi_config_t csi_config = {0}; 
    csi_config.lltf_en           = true;
    csi_config.htltf_en          = true;
    csi_config.stbc_htltf2_en    = true;
    csi_config.ltf_merge_en      = true;
    csi_config.channel_filter_en = true;
    csi_config.manu_scale        = false;
    csi_config.shift             = false;
    
    esp_wifi_set_csi_config(&csi_config);
    esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL);
    esp_wifi_set_csi(true);
    
    ESP_LOGI(TAG, "Wi-Fi CSI Presence Detection Initialized Successfully.");
    start_ping_emitter();
}
