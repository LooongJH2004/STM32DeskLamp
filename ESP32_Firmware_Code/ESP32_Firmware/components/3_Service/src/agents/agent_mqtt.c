#include "agents/agent_mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "data_center.h"
#include "app_config.h"

static const char *TAG = "Agent_MQTT";
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_is_connected = false;

// ============================================================
// 内部逻辑：处理收到的控制指令
// ============================================================

/**
 * @brief 解析来自 Python 的 JSON 指令
 * 格式示例: {"power":1, "brightness":80, "color_temp":20}
 */
static void _handle_ctrl_msg(const char *data, int len) {
    // 1. 解析 JSON
    cJSON *json = cJSON_ParseWithLength(data, len);
    if (!json) {
        ESP_LOGE(TAG, "JSON Parse Failed");
        return;
    }

    // 2. 获取当前数据中心的状态作为基础 (避免覆盖未修改的字段)
    DC_LightingData_t light_data;
    DataCenter_Get_Lighting(&light_data);

    // 3. 提取字段并更新
    cJSON *pwr = cJSON_GetObjectItem(json, "power");
    if (pwr) {
        light_data.power = (pwr->valueint != 0);
    }

    cJSON *bri = cJSON_GetObjectItem(json, "brightness");
    if (bri) {
        // 限制范围 0-100
        int val = bri->valueint;
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        light_data.brightness = (uint8_t)val;
    }

    cJSON *cct = cJSON_GetObjectItem(json, "color_temp");
    if (cct) {
        int val = cct->valueint;
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        light_data.color_temp = (uint8_t)val;
    }

    // 4. 写回数据中心
    // 注意：DataCenter_Set_Lighting 内部会自动比对数据，
    // 如果数据真的变了，它会发出 EVT_DATA_LIGHT_CHANGED 事件。
    DataCenter_Set_Lighting(&light_data);
    
    ESP_LOGI(TAG, "Applied Control: Pwr:%d, Bri:%d, CCT:%d", 
             light_data.power, light_data.brightness, light_data.color_temp);

    cJSON_Delete(json);
}

// ============================================================
// MQTT 事件回调
// ============================================================

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            s_is_connected = true;
            // 连接成功后，订阅控制主题
            esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_CTRL, 1);
            // 上线时主动上报一次当前状态
            Agent_MQTT_Publish_Status();
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected");
            s_is_connected = false;
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT Data: Topic=%.*s", event->topic_len, event->topic);
            // 判断是否是控制主题
            if (strncmp(event->topic, MQTT_TOPIC_CTRL, event->topic_len) == 0) {
                _handle_ctrl_msg(event->data, event->data_len);
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error");
            break;
            
        default:
            break;
    }
}

// ============================================================
// 公开接口
// ============================================================

void Agent_MQTT_Init(void) {
    if (s_client) return; // 防止重复初始化

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.keepalive = 60,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

void Agent_MQTT_Stop(void) {
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        s_is_connected = false;
    }
}

void Agent_MQTT_Publish_Status(void) {
    if (!s_client || !s_is_connected) return;

    // 1. 获取最新数据
    DC_LightingData_t light;
    DC_EnvData_t env;
    DataCenter_Get_Lighting(&light);
    DataCenter_Get_Env(&env);

    // 2. 构建 JSON
    cJSON *root = cJSON_CreateObject();
    
    // 灯光数据
    cJSON_AddNumberToObject(root, "power", light.power ? 1 : 0);
    cJSON_AddNumberToObject(root, "brightness", light.brightness);
    cJSON_AddNumberToObject(root, "color_temp", light.color_temp);
    
    // 环境数据 (如果有传感器)
    cJSON_AddNumberToObject(root, "temp", env.indoor_temp);
    cJSON_AddNumberToObject(root, "hum", env.indoor_hum);

    // 3. 发送
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATUS, json_str, 0, 1, 0);
        free(json_str);
        // ESP_LOGI(TAG, "Status Published"); // 调试时可打开
    }
    
    cJSON_Delete(root);
}
