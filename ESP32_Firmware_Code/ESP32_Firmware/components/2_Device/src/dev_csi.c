#include "dev_csi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "ping/ping_sock.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "Dev_CSI";

#define MAX_SUBCARRIERS 128
#define CSI_LEAVE_TIMEOUT_MS 15000

// ============================================================
// 函数指针定义 (策略模式)
// ============================================================
typedef void (*csi_rx_algo_t)(wifi_csi_info_t *info);
typedef uint32_t (*csi_eval_algo_t)(void);

static csi_rx_algo_t s_current_rx_algo = NULL;
static csi_eval_algo_t s_current_eval_algo = NULL;

static uint8_t s_current_mode = 3;
static uint32_t s_current_threshold = 250;

static csi_presence_cb_t s_app_cb = NULL;
static volatile uint32_t s_last_active_tick = 0;
static bool s_is_present = false;

// ============================================================
// 算法 1：归一化轮廓绝对差值法 (V1)
// ============================================================
static uint32_t s_a1_diff_sum = 0;
static uint32_t s_a1_count = 0;
static uint16_t s_a1_baseline[MAX_SUBCARRIERS] = {0};
static bool s_a1_init = false;

static void algo1_rx(wifi_csi_info_t *info) {
    int sub_count = info->len / 2;
    if (sub_count > MAX_SUBCARRIERS) sub_count = MAX_SUBCARRIERS;
    int8_t *data = (int8_t *)info->buf;
    
    uint32_t total_amp = 0;
    uint16_t amps[MAX_SUBCARRIERS];
    for (int i = 0; i < sub_count; i++) {
        amps[i] = abs(data[i * 2]) + abs(data[i * 2 + 1]);
        total_amp += amps[i];
    }
    uint32_t mean_amp = total_amp / sub_count;
    if (mean_amp == 0) return;

    uint32_t total_diff = 0;
    for (int i = 0; i < sub_count; i++) {
        uint16_t norm = (amps[i] * 100) / mean_amp;
        if (!s_a1_init) {
            s_a1_baseline[i] = norm;
        } else {
            total_diff += abs((int)norm - (int)s_a1_baseline[i]);
            s_a1_baseline[i] = (s_a1_baseline[i] * 15 + norm) / 16;
        }
    }
    if (!s_a1_init) { s_a1_init = true; return; }
    
    s_a1_diff_sum += (total_diff / sub_count);
    s_a1_count++;
}

static uint32_t algo1_eval(void) {
    uint32_t score = 0;
    if (s_a1_count > 0) score = s_a1_diff_sum / s_a1_count;
    s_a1_diff_sum = 0;
    s_a1_count = 0;
    return score;
}

// ============================================================
// 算法 2：宏观总振幅极差法 (V2)
// ============================================================
static uint32_t s_a2_history[64];
static int s_a2_count = 0;

static void algo2_rx(wifi_csi_info_t *info) {
    int8_t *data = (int8_t *)info->buf;
    uint32_t total_amp = 0;
    for (int i = 0; i < info->len - 1; i += 2) {
        total_amp += abs(data[i]) + abs(data[i+1]);
    }
    if (s_a2_count < 64) s_a2_history[s_a2_count++] = total_amp;
}

static uint32_t algo2_eval(void) {
    uint32_t score = 0;
    if (s_a2_count > 2) {
        uint32_t max_a = 0, min_a = 0xFFFFFFFF;
        for (int i = 0; i < s_a2_count; i++) {
            if (s_a2_history[i] > max_a) max_a = s_a2_history[i];
            if (s_a2_history[i] < min_a) min_a = s_a2_history[i];
        }
        score = max_a - min_a;
    }
    s_a2_count = 0;
    return score;
}

// ============================================================
// 算法 3：轮廓均方误差与截尾滤波法 (V3)
// ============================================================
static uint32_t s_a3_history[32];
static int s_a3_count = 0;
static uint16_t s_a3_baseline[MAX_SUBCARRIERS] = {0};
static bool s_a3_init = false;

static void algo3_rx(wifi_csi_info_t *info) {
    int sub_count = info->len / 2;
    if (sub_count > MAX_SUBCARRIERS) sub_count = MAX_SUBCARRIERS;
    int8_t *data = (int8_t *)info->buf;
    
    uint32_t total_amp = 0;
    uint16_t amps[MAX_SUBCARRIERS];
    for (int i = 0; i < sub_count; i++) {
        amps[i] = abs(data[i * 2]) + abs(data[i * 2 + 1]);
        total_amp += amps[i];
    }
    uint32_t mean_amp = total_amp / sub_count;
    if (mean_amp == 0) return;

    uint32_t total_sq_diff = 0;
    for (int i = 0; i < sub_count; i++) {
        uint16_t norm = (amps[i] * 100) / mean_amp;
        if (!s_a3_init) {
            s_a3_baseline[i] = norm;
        } else {
            int diff = abs((int)norm - (int)s_a3_baseline[i]);
            total_sq_diff += (diff * diff);
            s_a3_baseline[i] = (s_a3_baseline[i] * 15 + norm) / 16;
        }
    }
    if (!s_a3_init) { s_a3_init = true; return; }
    
    if (s_a3_count < 32) s_a3_history[s_a3_count++] = total_sq_diff / sub_count;
}

static uint32_t algo3_eval(void) {
    uint32_t score = 0;
    int count = s_a3_count;
    uint32_t local[32];
    for (int i = 0; i < count; i++) local[i] = s_a3_history[i];
    s_a3_count = 0;

    if (count > 0) {
        for (int i = 0; i < count - 1; i++) {
            for (int j = i + 1; j < count; j++) {
                if (local[i] > local[j]) {
                    uint32_t temp = local[i]; local[i] = local[j]; local[j] = temp;
                }
            }
        }
        int start = 0, end = count;
        if (count >= 6) {
            int trim = count / 5;
            start = trim; end = count - trim;
        }
        uint32_t sum = 0;
        for (int i = start; i < end; i++) sum += local[i];
        score = sum / (end - start);
    }
    return score;
}

// ============================================================
// 核心调度与任务
// ============================================================
static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    if (s_current_rx_algo) s_current_rx_algo(info);
}

void Dev_CSI_Set_Mode(uint8_t mode) {
    esp_wifi_set_csi(false); // 切换前先关闭 CSI，保证线程安全
    
    s_current_mode = mode;
    if (mode == 1) {
        s_current_rx_algo = algo1_rx;
        s_current_eval_algo = algo1_eval;
        s_current_threshold = 8;
        s_a1_init = false; s_a1_count = 0; s_a1_diff_sum = 0;
        ESP_LOGW(TAG, ">>> Switched to Mode 1: Normalized Abs Diff <<<");
    } else if (mode == 2) {
        s_current_rx_algo = algo2_rx;
        s_current_eval_algo = algo2_eval;
        s_current_threshold = 150;
        s_a2_count = 0;
        ESP_LOGW(TAG, ">>> Switched to Mode 2: Macro Amp Range <<<");
    } else if (mode == 3) {
        s_current_rx_algo = algo3_rx;
        s_current_eval_algo = algo3_eval;
        s_current_threshold = 250;
        s_a3_init = false; s_a3_count = 0;
        ESP_LOGW(TAG, ">>> Switched to Mode 3: Squared MSE + Trimmed Mean <<<");
    }
    
    s_is_present = false;
    esp_wifi_set_csi(true);
}

static void csi_monitor_task(void *arg) {
    int consecutive_motion_count = 0;
    while (1) {
        uint32_t now = xTaskGetTickCount();
        uint32_t score = 0;
        
        if (s_current_eval_algo) score = s_current_eval_algo();

        ESP_LOGI(TAG, "[雷达-V%d] 得分: %4lu | 阈值: %lu", s_current_mode, score, s_current_threshold);

        if (score > s_current_threshold) {
            consecutive_motion_count++;
            if (consecutive_motion_count >= 2) {
                s_last_active_tick = now; 
                if (!s_is_present) {
                    s_is_present = true;
                    if (s_app_cb) s_app_cb(true); 
                }
            }
        } else {
            consecutive_motion_count = 0;
        }

        if (s_is_present) {
            uint32_t elapsed = (now >= s_last_active_tick) ? (now - s_last_active_tick) : 0;
            if (elapsed * portTICK_PERIOD_MS > CSI_LEAVE_TIMEOUT_MS) {
                s_is_present = false;
                if (s_app_cb) s_app_cb(false); 
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}

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

void Dev_CSI_Init(csi_presence_cb_t cb) {
    s_app_cb = cb;
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
    
    // 默认启动第三版算法
    Dev_CSI_Set_Mode(3);
    
    start_ping_emitter();
}
