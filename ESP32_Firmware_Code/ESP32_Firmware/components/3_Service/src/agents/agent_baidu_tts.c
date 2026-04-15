/**
 * @file    agent_baidu_tts.c
 * @brief   百度语音合成 (TTS) 代理模块
 * @details 负责将纯文本发送至百度 TTS 接口，接收返回的 PCM 音频流，
 *          并利用 HTTP 分块传输机制 (Chunked) 将音频流实时喂入底层 RingBuffer 实现边下边播。
 */

#include "agents/agent_baidu_tts.h"
#include "agents/agent_baidu_asr.h" 
#include "svc_audio.h"              
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "BaiduTTS";

/** @brief 百度 TTS API 接口地址 */
#define BAIDU_TTS_URL "http://tsn.baidu.com/text2audio"

/** @brief 标记当前 HTTP 响应是否为合法的音频流 */
static bool s_is_audio_response = false;

/** @brief 标记是否为当前 TTS 会话的第一个音频数据块 (用于精确测算首包延迟) */
static bool s_is_first_chunk = true;

/**
 * @brief URL 编码函数
 * @details 将包含中文字符的纯文本转换为 HTTP GET/POST 请求支持的 URL 编码格式
 * @param str 原始 UTF-8 字符串
 * @return char* 编码后的字符串指针 (动态分配，调用者需负责 free)
 */
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

/**
 * @brief HTTP 客户端事件回调函数
 * @details 处理 HTTP 响应头与分块到达的音频数据
 * @param evt HTTP 事件结构体指针
 * @return esp_err_t 始终返回 ESP_OK
 */
static esp_err_t _tts_http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            // 严格检查 Content-Type，确保百度返回的是音频而不是错误 JSON
            if (strcasecmp(evt->header_key, "Content-Type") == 0) {
                if (strstr(evt->header_value, "audio") != NULL) {
                    s_is_audio_response = true;
                } else {
                    s_is_audio_response = false;
                    ESP_LOGE(TAG, "API Error! Content-Type is not audio: %s", evt->header_value);
                }
            }
            break;
            
        case HTTP_EVENT_ON_DATA: {
            int status = esp_http_client_get_status_code(evt->client);
            if (status == 200 && s_is_audio_response) {
                
                // 【关键逻辑】只在收到第一块音频数据时打印 T3 时间戳，防止日志刷屏
                if (s_is_first_chunk) {
                    ESP_LOGI(TAG, "[TIMING] T3: TTS First Audio Chunk Received");
                    s_is_first_chunk = false;
                }
                
                // 只有确认为 audio 格式，才将 PCM 数据喂给底层播放器的 RingBuffer
                Svc_Audio_Feed_Data((const uint8_t *)evt->data, evt->data_len);
            } else if (!s_is_audio_response) {
                // 打印出百度的报错信息，方便调试 (如 Token 过期、文本过长等)
                ESP_LOGE(TAG, "Baidu API Error Msg: %.*s", evt->data_len, (char*)evt->data);
            }
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief 请求百度 TTS 并流式播放音频
 * @note 这是一个阻塞函数，会边下载边将数据喂给 Svc_Audio 的 RingBuffer。
 *       直到音频全部下载完毕才会返回。
 * @param text 要播报的 UTF-8 文本 (最长约 300 个汉字)
 */
void Agent_TTS_Play(const char *text) {
    if (!text || strlen(text) == 0) return;
    
    // 【关键逻辑】每次发起新的 TTS 播放请求前，重置首包标志位
    s_is_first_chunk = true; 

    // 1. 获取鉴权 Token (复用 ASR 模块的 Token)
    const char *token = Agent_ASR_Get_Token();
    if (!token) {
        ESP_LOGE(TAG, "Failed to get Baidu Token");
        return;
    }

    // 2. 对文本进行 URL 编码
    char *encoded_text = url_encode(text);
    if (!encoded_text) return;

    // 3. 获取设备 MAC 地址作为唯一标识符 (CUID)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char cuid[18];
    snprintf(cuid, sizeof(cuid), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 4. 组装 POST 请求体
    // 参数说明: ctp=1(客户端类型), lan=zh(中文), spd=5(语速), pit=5(语调), vol=5(音量), per=0(度小美), aue=4(16K PCM)
    char *post_data = malloc(strlen(encoded_text) + 256);
    snprintf(post_data, strlen(encoded_text) + 256, 
             "tex=%s&tok=%s&cuid=%s&ctp=1&lan=zh&spd=5&pit=5&vol=5&per=0&aue=4", 
             encoded_text, token, cuid);

    ESP_LOGI(TAG, "Requesting TTS for text: %s", text);

    // 5. 配置 HTTP 客户端
    esp_http_client_config_t config = {
        .url = BAIDU_TTS_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000, // 增加超时时间，防止长文本合成超时
        .event_handler = _tts_http_event_handler,
        .buffer_size = 2048, // 接收缓冲区大小
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 6. 设置请求头并发送
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_header(client, "Accept", "*/*");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "TTS Playback Finished.");
    }

    // 7. 清理资源
    esp_http_client_cleanup(client);
    free(post_data);
    free(encoded_text);
}
