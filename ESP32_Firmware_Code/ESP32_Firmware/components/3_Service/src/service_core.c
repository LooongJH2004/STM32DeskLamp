#include "service_core.h"
#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "agents/agent_baidu_asr.h" 
#include "agents/agent_lampmind.h" 
#include "svc_lighting.h" // [新增] 引入灯光服务

static const char *TAG = "Svc_Core";
static SystemState_t s_current_state = SYS_STATE_IDLE;

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
            if (evt->data) {
                ESP_LOGI(TAG, ">>> LampMind Reply: %s", (char*)evt->data);
                free(evt->data); 
            } else {
                ESP_LOGW(TAG, ">>> LampMind Reply: (Empty/Error)");
            }
            s_current_state = SYS_STATE_SPEAKING;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_SPEAKING, 0);
            
            vTaskDelay(pdMS_TO_TICKS(2000));
            EventBus_Send(EVT_TTS_PLAY_FINISH, NULL, 0);
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
    EventBus_Init();
    SystemEvent_t evt;
    
    while (1) {
        if (EventBus_Receive(&evt, portMAX_DELAY) == ESP_OK) {
            
            // [新增] 全局事件：监听数据中心灯光变化
            if (evt.type == EVT_DATA_LIGHT_CHANGED) {
                Svc_Lighting_Apply(); // 触发 PWM 计算并下发 STM32
            }
            
            if (evt.type == EVT_SYS_ERROR) {
                ESP_LOGE(TAG, "System Error: %d", (int)evt.data);
                s_current_state = SYS_STATE_ERROR;
            }
            
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
