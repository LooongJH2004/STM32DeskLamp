#include "agents/agent_baidu_asr.h"
#include "dev_audio.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_mac.h"
#include "esp_heap_caps.h" 
#include "event_bus.h" // [NEW] 引入事件总线
#include "esp_crt_bundle.h"
#include "app_config.h" // 引入配置
#include <math.h>       // 用于 abs
#include <string.h>     // 用于 memcpy

static const char *TAG = "BaiduASR";

static char *s_access_token = NULL;
static volatile bool s_is_recording = false;

// --- 缓冲区定义 ---
static char *s_token_resp_buf = NULL;
static int s_token_resp_len = 0;

static char *s_asr_resp_buf = NULL;
static int s_asr_resp_len = 0;

// ============================================================================
// 1. HTTP 事件回调 (关键修复点：这两个函数必须在被调用前定义)
// ============================================================================

// 处理 Token 请求的响应
esp_err_t _token_http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (s_token_resp_buf == NULL) {
            s_token_resp_buf = malloc(evt->data_len + 1);
            memcpy(s_token_resp_buf, evt->data, evt->data_len);
            s_token_resp_buf[evt->data_len] = 0;
            s_token_resp_len = evt->data_len;
        } else {
            s_token_resp_buf = realloc(s_token_resp_buf, s_token_resp_len + evt->data_len + 1);
            memcpy(s_token_resp_buf + s_token_resp_len, evt->data, evt->data_len);
            s_token_resp_len += evt->data_len;
            s_token_resp_buf[s_token_resp_len] = 0;
        }
    }
    return ESP_OK;
}

// 处理 ASR 识别结果的响应 (你之前缺少的函数)
esp_err_t _asr_http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (s_asr_resp_buf == NULL) {
            s_asr_resp_buf = malloc(evt->data_len + 1);
            memcpy(s_asr_resp_buf, evt->data, evt->data_len);
            s_asr_resp_buf[evt->data_len] = 0;
            s_asr_resp_len = evt->data_len;
        } else {
            s_asr_resp_buf = realloc(s_asr_resp_buf, s_asr_resp_len + evt->data_len + 1);
            memcpy(s_asr_resp_buf + s_asr_resp_len, evt->data, evt->data_len);
            s_asr_resp_len += evt->data_len;
            s_asr_resp_buf[s_asr_resp_len] = 0;
        }
    }
    return ESP_OK;
}

// ============================================================================
// 2. Token 获取逻辑
// ============================================================================

static void _get_token(void) {
    if (s_access_token) return;

    ESP_LOGI(TAG, "Getting Access Token...");
    
    if (s_token_resp_buf) { free(s_token_resp_buf); s_token_resp_buf = NULL; s_token_resp_len = 0; }

    char url[512];
    snprintf(url, sizeof(url), "%s?grant_type=client_credentials&client_id=%s&client_secret=%s",
             BAIDU_TOKEN_URL, BAIDU_API_KEY, BAIDU_SECRET_KEY);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _token_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200 && s_token_resp_buf) {
            cJSON *json = cJSON_Parse(s_token_resp_buf);
            if (json) {
                cJSON *token_item = cJSON_GetObjectItem(json, "access_token");
                if (token_item && token_item->valuestring) {
                    if (s_access_token) free(s_access_token);
                    s_access_token = strdup(token_item->valuestring);
                    ESP_LOGI(TAG, "Token Got: %s...", s_access_token);
                }
                cJSON_Delete(json);
            }
        } else {
            ESP_LOGE(TAG, "Token HTTP Error: %d", status);
        }
    } else {
        ESP_LOGE(TAG, "Token Request Failed: %s", esp_err_to_name(err));
    }
    
    if (s_token_resp_buf) { free(s_token_resp_buf); s_token_resp_buf = NULL; }
    esp_http_client_cleanup(client);
}

// ============================================================================
// 3. VAD 辅助函数
// ============================================================================
static uint32_t _calculate_volume(int16_t *data, int samples) {
    uint64_t sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += abs(data[i]); // 计算绝对值之和
    }
    return (uint32_t)(sum / samples); // 返回平均振幅
}

// ============================================================================
// 4. ASR 核心任务
// ============================================================================

void Agent_ASR_Init(void) {
    _get_token();
}

void Agent_ASR_Stop(void) {
    s_is_recording = false;
    ESP_LOGI(TAG, "ASR Stop Signal Received.");
}

void Agent_ASR_Run_Session(void *pvParameters) {
    // 1. 检查 Token
    if (!s_access_token) _get_token();
    if (!s_access_token) {
        ESP_LOGE(TAG, "No Token, Abort.");
        EventBus_Send(EVT_ASR_RESULT, NULL, 0);
        vTaskDelete(NULL);
        return;
    }

    // 2. 准备录音缓冲区 (PSRAM)
    size_t max_buffer_size = (16000 * 2 * ASR_MAX_DURATION_MS) / 1000;
    uint8_t *audio_buffer = (uint8_t *)heap_caps_malloc(max_buffer_size, MALLOC_CAP_SPIRAM);
    
    if (!audio_buffer) {
        ESP_LOGE(TAG, "PSRAM Malloc Failed!");
        EventBus_Send(EVT_ASR_RESULT, NULL, 0);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Start Recording (VAD Enabled)... Max: %d ms", ASR_MAX_DURATION_MS);
    s_is_recording = true;

    // 3. 录音循环变量
    size_t total_bytes_recorded = 0;
    size_t chunk_samples = 512; // 每次处理 512 个采样点 (约 32ms)
    int32_t *raw_i2s_buffer = malloc(chunk_samples * sizeof(int32_t));
    size_t bytes_read = 0;
    
    int silence_ms = 0;     // 当前连续静音时长
    int recording_ms = 0;   // 总录音时长

    // 4. 开始录音循环
    while (s_is_recording) {
        // 4.1 读取 I2S 数据
        Dev_Audio_Read(raw_i2s_buffer, chunk_samples * sizeof(int32_t), &bytes_read);
        
        int samples_read = bytes_read / sizeof(int32_t);
        int16_t *dest_ptr = (int16_t *)(audio_buffer + total_bytes_recorded);

        // 4.2 数据转换 (32bit -> 16bit) & 增益
        for (int i = 0; i < samples_read; i++) {
            int32_t val = raw_i2s_buffer[i];
            val = val >> 14; // 移位调整音量
            
            // 钳位防止溢出
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            dest_ptr[i] = (int16_t)val;
        }

        // 4.3 VAD 检测
        uint32_t current_vol = _calculate_volume(dest_ptr, samples_read);
        
        if (current_vol < VAD_SILENCE_THRESHOLD) {
            silence_ms += (samples_read * 1000) / 16000;
        } else {
            silence_ms = 0;
        }

        // 更新总长度
        size_t chunk_bytes = samples_read * sizeof(int16_t);
        total_bytes_recorded += chunk_bytes;
        recording_ms += (samples_read * 1000) / 16000;

        // 4.4 检查退出条件
        if (total_bytes_recorded >= max_buffer_size || recording_ms >= ASR_MAX_DURATION_MS) {
            ESP_LOGW(TAG, "Max duration reached (%d ms). Stopping.", recording_ms);
            break;
        }
        if (recording_ms > 500 && silence_ms > VAD_SILENCE_DURATION_MS) {
            ESP_LOGI(TAG, "Silence detected (%d ms). Stopping.", silence_ms);
            break;
        }

        vTaskDelay(1);
    }
    
    free(raw_i2s_buffer);
    s_is_recording = false;

    // 5. 上传处理
    if (total_bytes_recorded < 16000 * 2 * 0.5) { 
        ESP_LOGW(TAG, "Recording too short (%d bytes), ignore.", total_bytes_recorded);
        EventBus_Send(EVT_ASR_RESULT, NULL, 0);
    } else {
        ESP_LOGI(TAG, "Recording finished. Total: %d bytes (%d ms). Uploading...", 
                 total_bytes_recorded, recording_ms);
        
        char *result_text = NULL;
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char cuid[18];
        snprintf(cuid, sizeof(cuid), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        char url[512];
        snprintf(url, sizeof(url), "%s?cuid=%s&token=%s&dev_pid=1537", BAIDU_ASR_URL, cuid, s_access_token);

        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 10000,
            .event_handler = _asr_http_event_handler, // 现在这个函数已经定义了
            .buffer_size = 1024,
            .buffer_size_tx = 1024,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_http_client_set_header(client, "Content-Type", "audio/pcm;rate=16000");
        esp_http_client_set_post_field(client, (const char *)audio_buffer, total_bytes_recorded);

        esp_err_t err = esp_http_client_perform(client);
        
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "ASR HTTP Status: %d", status);
            if (status == 200 && s_asr_resp_buf) {
                ESP_LOGI(TAG, "ASR Response: %s", s_asr_resp_buf);
                cJSON *json = cJSON_Parse(s_asr_resp_buf);
                if (json) {
                    cJSON *err_no = cJSON_GetObjectItem(json, "err_no");
                    if (err_no && err_no->valueint == 0) {
                        cJSON *result = cJSON_GetObjectItem(json, "result");
                        if (result && cJSON_GetArraySize(result) > 0) {
                            cJSON *text = cJSON_GetArrayItem(result, 0);
                            if (text && text->valuestring) {
                                result_text = strdup(text->valuestring);
                            }
                        }
                    }
                    cJSON_Delete(json);
                }
            }
        } else {
            ESP_LOGE(TAG, "ASR Request Failed: %s", esp_err_to_name(err));
        }

        EventBus_Send(EVT_ASR_RESULT, result_text, result_text ? strlen(result_text) : 0);
        
        esp_http_client_cleanup(client);
        if (s_asr_resp_buf) { free(s_asr_resp_buf); s_asr_resp_buf = NULL; s_asr_resp_len = 0; }
    }

    // 6. 清理内存
    heap_caps_free(audio_buffer);
    vTaskDelete(NULL);
}
