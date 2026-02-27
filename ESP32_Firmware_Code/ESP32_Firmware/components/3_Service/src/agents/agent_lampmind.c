#include "agents/agent_lampmind.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "event_bus.h"
#include "data_center.h"
#include "app_config.h"
#include <string.h>

static const char *TAG = "LampMind";

// 缓冲区定义
static char *s_resp_buf = NULL;
static int s_resp_len = 0;

// HTTP 响应回调
static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (s_resp_buf == NULL) {
            s_resp_buf = malloc(evt->data_len + 1);
            if (s_resp_buf) {
                memcpy(s_resp_buf, evt->data, evt->data_len);
                s_resp_buf[evt->data_len] = 0;
                s_resp_len = evt->data_len;
            }
        } else {
            char *new_buf = realloc(s_resp_buf, s_resp_len + evt->data_len + 1);
            if (new_buf) {
                s_resp_buf = new_buf;
                memcpy(s_resp_buf + s_resp_len, evt->data, evt->data_len);
                s_resp_len += evt->data_len;
                s_resp_buf[s_resp_len] = 0;
            }
        }
    }
    return ESP_OK;
}

void Agent_LampMind_Chat_Task(void *pvParameters) {
    char *text = (char *)pvParameters;
    if (!text) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Sending to LampMind: %s", text);

    // 1. 构建 JSON 请求体
    cJSON *req_json = cJSON_CreateObject();
    cJSON_AddStringToObject(req_json, "device_id", LAMPMIND_DEVICE_ID);
    cJSON_AddStringToObject(req_json, "text", text);
    char *post_data = cJSON_PrintUnformatted(req_json);
    cJSON_Delete(req_json);

    if (s_resp_buf) { free(s_resp_buf); s_resp_buf = NULL; s_resp_len = 0; }

    // 2. 配置 HTTP 客户端
    esp_http_client_config_t config = {
        .url = LAMPMIND_SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000, // LLM 处理和工具调用可能较慢，设置 15 秒超时
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // 3. 发起请求
    esp_err_t err = esp_http_client_perform(client);
    char *reply_text = NULL;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status: %d", status);
        
        if (status == 200 && s_resp_buf) {
            ESP_LOGI(TAG, "Response: %s", s_resp_buf);
            cJSON *json = cJSON_Parse(s_resp_buf);
            if (json) {
                // 解析 reply_text (用于后续 TTS 播放)
                cJSON *reply_item = cJSON_GetObjectItem(json, "reply_text");
                if (reply_item && reply_item->valuestring) {
                    reply_text = strdup(reply_item->valuestring);
                }

                // 解析 action (设备控制指令)
                cJSON *action_item = cJSON_GetObjectItem(json, "action");
                if (action_item && action_item->type == cJSON_Object) {
                    cJSON *cmd_item = cJSON_GetObjectItem(action_item, "cmd");
                    if (cmd_item && cmd_item->valuestring) {
                        
                        // 处理灯光控制指令
                        if (strcmp(cmd_item->valuestring, "light") == 0) {
                            DC_LightingData_t light_data;
                            DataCenter_Get_Lighting(&light_data); // 获取当前状态
                            
                            cJSON *bri_item = cJSON_GetObjectItem(action_item, "brightness");
                            if (bri_item) light_data.brightness = bri_item->valueint;
                            
                            cJSON *cct_item = cJSON_GetObjectItem(action_item, "color_temp");
                            if (cct_item) light_data.color_temp = cct_item->valueint;
                            
                            light_data.power = true; // 收到调光指令默认开灯
                            
                            DataCenter_Set_Lighting(&light_data); // 更新数据中心，自动触发事件
                            ESP_LOGI(TAG, "Action executed: Light updated");
                        } 
                        // 处理定时器指令
                        else if (strcmp(cmd_item->valuestring, "timer") == 0) {
                            ESP_LOGI(TAG, "Action executed: Timer set (To be implemented)");
                        }
                    }
                }
                cJSON_Delete(json);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(err));
    }

    // 4. 清理资源
    esp_http_client_cleanup(client);
    free(post_data);
    free(text); // 释放传入的 ASR 文本内存
    if (s_resp_buf) { free(s_resp_buf); s_resp_buf = NULL; s_resp_len = 0; }

    // 5. 发送 LLM 结果事件 (即使失败也发送 NULL，防止状态机卡死)
    EventBus_Send(EVT_LLM_RESULT, reply_text, reply_text ? strlen(reply_text) : 0);

    vTaskDelete(NULL);
}
