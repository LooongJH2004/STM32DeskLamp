#include "service_core.h"
#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "agents/agent_baidu_asr.h" 
#include "agents/agent_lampmind.h" 
#include "svc_lighting.h" // [新增] 引入灯光服务
#include "agents/agent_mqtt.h" // <--- [新增] 引入头文件

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
            // [新增] Wi-Fi 连上后，启动 MQTT
            Agent_MQTT_Init();
            break;            
        case EVT_NET_DISCONNECTED:
            // [新增] Wi-Fi 断开，停止 MQTT (可选，MQTT 库自带重连，但显式停止更安全)
            // Agent_MQTT_Stop(); 
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
    
    // 1. 初始化事件总线
    EventBus_Init();
    
    SystemEvent_t evt;
    
    while (1) {
        // 2. 阻塞等待事件 (核心循环)
        if (EventBus_Receive(&evt, portMAX_DELAY) == ESP_OK) {
            
            // ---------------------------------------------------------
            // A. 全局事件处理 (Global Event Handlers)
            // 这些事件与当前系统状态无关，任何时候发生都需要处理
            // ---------------------------------------------------------

            // [场景1] 数据中心发生变化 (来源: MQTT下发 / STM32旋钮 / 语音控制)
            if (evt.type == EVT_DATA_LIGHT_CHANGED) {
                ESP_LOGI(TAG, "Global: Light Data Changed -> Sync Hardware & Cloud");
                
                // 1. 下发给 STM32 执行硬件动作 (PWM 调光)
                Svc_Lighting_Apply(); 
                
                // 2. 上报给 MQTT，同步 Python GUI 状态 (保持滑块位置一致)
                Agent_MQTT_Publish_Status();
            }
            
            // [场景2] 环境数据发生变化 (来源: STM32传感器上报)
            else if (evt.type == EVT_DATA_ENV_CHANGED) {
                // 仅上报给 MQTT，无需驱动硬件
                Agent_MQTT_Publish_Status();
            }

            // [场景3] 网络连接成功
            else if (evt.type == EVT_NET_CONNECTED) {
                ESP_LOGI(TAG, "Global: Network Connected -> Start MQTT");
                // 启动 MQTT 客户端
                Agent_MQTT_Init();
            }

            // [场景4] 系统错误
            else if (evt.type == EVT_SYS_ERROR) {
                ESP_LOGE(TAG, "Global: System Error: %d", (int)evt.data);
                s_current_state = SYS_STATE_ERROR;
                // 可选: Agent_MQTT_Stop();
            }
            
            // ---------------------------------------------------------
            // B. 状态机事件处理 (State Machine Handlers)
            // 根据当前状态，决定如何响应特定事件 (如按键、语音结果)
            // ---------------------------------------------------------
            switch (s_current_state) {
                case SYS_STATE_IDLE: 
                    _handle_state_idle(&evt); 
                    break;
                case SYS_STATE_LISTENING: 
                    _handle_state_listening(&evt); 
                    break;
                case SYS_STATE_PROCESSING: 
                    _handle_state_processing(&evt); 
                    break;
                case SYS_STATE_SPEAKING: 
                    _handle_state_speaking(&evt); 
                    break;
                default: 
                    break;
            }
            
            // ---------------------------------------------------------
            // C. 资源清理
            // ---------------------------------------------------------
            // 如果事件携带了动态分配的数据 (如 ASR 结果字符串)，需在此释放
            // 注意：目前 ASR/LLM 结果在各自 handle 函数里处理，
            // 若有通用数据需释放，请根据具体协议在此添加 free(evt.data);
        }
    }
}
void Service_Core_Init(void) {
    xTaskCreatePinnedToCore(Service_Core_Task, "Svc_Core", 4096, NULL, 5, NULL, 0);
}
