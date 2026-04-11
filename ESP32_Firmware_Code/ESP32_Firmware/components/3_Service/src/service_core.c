#include "service_core.h"
#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "agents/agent_baidu_asr.h" 
#include "agents/agent_lampmind.h" 
#include "agents/agent_baidu_tts.h" // [新增] 引入 TTS
#include "svc_lighting.h" 
#include "agents/agent_mqtt.h" 

static const char *TAG = "Svc_Core";
static SystemState_t s_current_state = SYS_STATE_IDLE;

// ============================================================
// [新增] TTS 独立播放任务
// 为什么要在独立任务中播放？因为 Agent_TTS_Play 是阻塞的（边下边播）。
// 如果直接在 Service_Core 中调用，会卡死事件总线，导致无法响应按键打断！
// ============================================================
static void tts_play_task(void *pvParameters) {
    char *text = (char *)pvParameters;
    if (text) {
        Agent_TTS_Play(text);
        free(text); // 播放完毕后，释放 LLM 传过来的字符串内存
    }
    // 播放结束，发送事件通知状态机回到 IDLE
    EventBus_Send(EVT_TTS_PLAY_FINISH, NULL, 0);
    vTaskDelete(NULL);
}

static void _handle_state_idle(SystemEvent_t *evt) {
    switch (evt->type) {
        case EVT_KEY_CLICK:
            ESP_LOGI(TAG, "[IDLE] Key Click -> Switch to LISTENING");
            s_current_state = SYS_STATE_LISTENING;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_LISTENING, 0);
            xTaskCreate((TaskFunction_t)Agent_ASR_Run_Session, "ASR_Task", 8192, (void*)5000, 5, NULL);
            break;
        case EVT_NET_CONNECTED:
            ESP_LOGI(TAG, "[IDLE] Network Connected");
            Agent_MQTT_Init();
            break;            
        default:
            break;
    }
}

static void _handle_state_listening(SystemEvent_t *evt) {
    switch (evt->type) {
        case EVT_ASR_RESULT:
            if (evt->data) {
                ESP_LOGI(TAG, "[LISTENING] ASR Result: %s", (char*)evt->data);
                s_current_state = SYS_STATE_PROCESSING;
                EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_PROCESSING, 0);
                xTaskCreate(Agent_LampMind_Chat_Task, "LampMind_Task", 8192, evt->data, 5, NULL);
            } else {
                ESP_LOGW(TAG, "[LISTENING] ASR Empty -> Back to IDLE");
                s_current_state = SYS_STATE_IDLE;
                EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_IDLE, 0);
            }
            break;
        case EVT_KEY_CLICK:
            ESP_LOGI(TAG, "[LISTENING] Key Click -> Cancel -> Switch to IDLE");
            Agent_ASR_Stop(); 
            s_current_state = SYS_STATE_IDLE;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_IDLE, 0);
            break;
        default:
            break;
    }
}

static void _handle_state_processing(SystemEvent_t *evt) {
    switch (evt->type) {
        case EVT_LLM_RESULT:
            ESP_LOGI(TAG, "[PROCESSING] LLM Result -> Switch to SPEAKING");
            s_current_state = SYS_STATE_SPEAKING;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_SPEAKING, 0);
            
            if (evt->data) {
                ESP_LOGI(TAG, ">>> LampMind Reply: %s", (char*)evt->data);
                // [修改] 启动独立任务播放 TTS
                xTaskCreate(tts_play_task, "TTS_Task", 8192, evt->data, 5, NULL);
            } else {
                ESP_LOGW(TAG, ">>> LampMind Reply: (Empty/Error)");
                EventBus_Send(EVT_TTS_PLAY_FINISH, NULL, 0);
            }
            break;
        default:
            break;
    }
}

static void _handle_state_speaking(SystemEvent_t *evt) {
    switch (evt->type) {
        case EVT_TTS_PLAY_FINISH:
            ESP_LOGI(TAG, "[SPEAKING] Play Finish -> Back to IDLE");
            s_current_state = SYS_STATE_IDLE;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_IDLE, 0);
            break;
        default:
            break;
    }
}

void Service_Core_Task(void *pvParameters) {
    ESP_LOGI(TAG, "Service Core Started");
    SystemEvent_t evt;
    
    while (1) {
        if (EventBus_Receive(&evt, portMAX_DELAY) == ESP_OK) {
            // --- 全局事件 ---
            if (evt.type == EVT_DATA_LIGHT_CHANGED) {
                Svc_Lighting_Apply(); 
                Agent_MQTT_Publish_Status();
            } else if (evt.type == EVT_DATA_ENV_CHANGED) {
                Agent_MQTT_Publish_Status();
            } else if (evt.type == EVT_NET_CONNECTED) {
                Agent_MQTT_Init();
            }
            
            // --- 状态机事件 ---
            switch (s_current_state) {
                case SYS_STATE_IDLE: _handle_state_idle(&evt); break;
                case SYS_STATE_LISTENING: _handle_state_listening(&evt); break;
                case SYS_STATE_PROCESSING: _handle_state_processing(&evt); break;
                case SYS_STATE_SPEAKING: _handle_state_speaking(&evt); break;
                default: break;
            }
        }
    }
}

void Service_Core_Init(void) {
    xTaskCreatePinnedToCore(Service_Core_Task, "Svc_Core", 4096, NULL, 5, NULL, 0);
}
