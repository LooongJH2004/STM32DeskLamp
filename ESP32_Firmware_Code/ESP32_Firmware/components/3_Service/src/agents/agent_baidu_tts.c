#include "agents/agent_baidu_tts.h"
#include "agents/agent_baidu_asr.h" 
#include "svc_audio.h"              
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "BaiduTTS";
#define BAIDU_TTS_URL "http://tsn.baidu.com/text2audio"

static char *url_encode(const char *str) {
    const char *hex = "0123456789ABCDEF";
    char *encoded = malloc(strlen(str) * 3 + 1);
    if (!encoded) return NULL;
    char *p = encoded;
    while (*str) {
        if (isalnum((unsigned char)*str) || *str == '-' || *str == '_' || *str == '.' || *str == '~') {
            *p++ = *str;
        } else if (*str == ' ') {
            *p++ = '+';
        } else {
            *p++ = '%';
            *p++ = hex[(*str >> 4) & 0x0F];
            *p++ = hex[*str & 0x0F];
        }
        str++;
    }
    *p = '\0';
    return encoded;
}

static esp_err_t _tts_http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            // [诊断] 打印返回的 Header，看是不是 audio
            ESP_LOGI(TAG, "Header: %s = %s", evt->header_key, evt->header_value);
            break;
            
        case HTTP_EVENT_ON_DATA: {
            int status = esp_http_client_get_status_code(evt->client);
            if (status == 200) {
                // [诊断] 检查前几个字节是不是 JSON 的 '{'
                if (evt->data_len > 0 && ((char*)evt->data)[0] == '{') {
                    ESP_LOGE(TAG, "API ERROR RETURNED: %.*s", evt->data_len, (char*)evt->data);
                    // 如果是错误信息，绝对不能塞进音频缓冲区！
                    return ESP_OK; 
                }
                // 正常音频数据，塞入缓冲区
                Svc_Audio_Feed_Data((const uint8_t *)evt->data, evt->data_len);
            } else {
                ESP_LOGE(TAG, "HTTP Error Status: %d", status);
            }
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

void Agent_TTS_Play(const char *text) {
    if (!text || strlen(text) == 0) return;

    const char *token = Agent_ASR_Get_Token();
    if (!token) {
        ESP_LOGE(TAG, "Failed to get Baidu Token");
        return;
    }

    char *encoded_text = url_encode(text);
    if (!encoded_text) return;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char cuid[18];
    snprintf(cuid, sizeof(cuid), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char *post_data = malloc(strlen(encoded_text) + 256);
    snprintf(post_data, strlen(encoded_text) + 256, 
             "tex=%s&tok=%s&cuid=%s&ctp=1&lan=zh&spd=5&pit=5&vol=5&per=0&aue=4", 
             encoded_text, token, cuid);

    ESP_LOGI(TAG, "Requesting TTS for text: %s", text);

    esp_http_client_config_t config = {
        .url = BAIDU_TTS_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000, // 增加超时时间
        .event_handler = _tts_http_event_handler,
        .buffer_size = 2048, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_header(client, "Accept", "*/*");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "TTS Playback Finished.");
    }

    esp_http_client_cleanup(client);
    free(post_data);
    free(encoded_text);
}
