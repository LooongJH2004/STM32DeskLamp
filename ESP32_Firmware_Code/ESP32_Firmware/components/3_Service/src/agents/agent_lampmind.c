#include "agents/agent_lampmind.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "event_bus.h"
#include "data_center.h"
#include "app_config.h"
#include <string.h>

static const char *TAG = "LampMind";

static char *s_resp_buf = NULL;
static int s_resp_len = 0;

static void _strip_markdown(char *str) {
    if (!str) return;
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '*' && *src != '#') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

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

    // --- 1. 获取当前设备状态 ---
    DC_LightingData_t light;
    DC_EnvData_t env;
    DataCenter_Get_Lighting(&light);
    DataCenter_Get_Env(&env);

    // --- 2. 构建 JSON 请求体 ---
    cJSON *req_json = cJSON_CreateObject();
    cJSON_AddStringToObject(req_json, "device_id", LAMPMIND_DEVICE_ID);
    cJSON_AddStringToObject(req_json, "text", text);
    
    // 构建 state 对象
    cJSON *state_obj = cJSON_CreateObject();
    
    cJSON *light_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(light_obj, "brightness", light.brightness);
    cJSON_AddNumberToObject(light_obj, "color_temp", light.color_temp);
    cJSON_AddBoolToObject(light_obj, "power", light.power);
    cJSON_AddItemToObject(state_obj, "light", light_obj);

    cJSON *env_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(env_obj, "temp", env.indoor_temp);
    cJSON_AddNumberToObject(env_obj, "humi", env.indoor_hum);
    cJSON_AddItemToObject(state_obj, "environment", env_obj);

    cJSON_AddItemToObject(req_json, "state", state_obj);

    char *post_data = cJSON_PrintUnformatted(req_json);
    cJSON_Delete(req_json);

    if (s_resp_buf) { free(s_resp_buf); s_resp_buf = NULL; s_resp_len = 0; }

    // --- 3. 发起 HTTP 请求 ---
    esp_http_client_config_t config = {
        .url = LAMPMIND_SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 45000, 
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    char *reply_text = NULL;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status: %d", status);
        
        if (status == 200 && s_resp_buf) {
            ESP_LOGI(TAG, "Response: %s", s_resp_buf);
            cJSON *json = cJSON_Parse(s_resp_buf);
            if (json) {
                cJSON *reply_item = cJSON_GetObjectItem(json, "reply_text");
                if (reply_item && reply_item->valuestring) {
                    reply_text = strdup(reply_item->valuestring);
                    _strip_markdown(reply_text); 
                }

                cJSON *action_item = cJSON_GetObjectItem(json, "action");
                if (action_item && action_item->type == cJSON_Object) {
                    cJSON *cmd_item = cJSON_GetObjectItem(action_item, "cmd");
                    if (cmd_item && cmd_item->valuestring) {
                        if (strcmp(cmd_item->valuestring, "light") == 0) {
                            DC_LightingData_t light_data;
                            DataCenter_Get_Lighting(&light_data);
                            
                            cJSON *bri_item = cJSON_GetObjectItem(action_item, "brightness");
                            if (bri_item) light_data.brightness = bri_item->valueint;
                            
                            cJSON *cct_item = cJSON_GetObjectItem(action_item, "color_temp");
                            if (cct_item) light_data.color_temp = cct_item->valueint;
                            
                            light_data.power = true;
                            DataCenter_Set_Lighting(&light_data);
                            ESP_LOGI(TAG, "Action executed: Light updated");
                        } 
                    }
                }
                cJSON_Delete(json);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(post_data);
    free(text); 
    if (s_resp_buf) { free(s_resp_buf); s_resp_buf = NULL; s_resp_len = 0; }

    EventBus_Send(EVT_LLM_RESULT, reply_text, reply_text ? strlen(reply_text) : 0);
    vTaskDelete(NULL);
}
