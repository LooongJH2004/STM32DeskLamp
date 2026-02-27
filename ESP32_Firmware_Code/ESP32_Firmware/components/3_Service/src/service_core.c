#include "service_core.h"
#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "agents/agent_baidu_asr.h" // 引入 ASR 代理
#include "agents/agent_lampmind.h" // [新增] 引入 LampMind 代理

static const char *TAG = "Svc_Core";
static SystemState_t s_current_state = SYS_STATE_IDLE;

// --- 状态处理函数 ---

static void _handle_state_idle(SystemEvent_t *evt) {
    switch (evt->type) {
        case EVT_KEY_CLICK:
            ESP_LOGI(TAG, "[IDLE] Key Click -> Switch to LISTENING");
            s_current_state = SYS_STATE_LISTENING;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_LISTENING, 0);
            
            // 启动 ASR 录音 (非阻塞调用，或者在独立任务中运行)
            // 注意：Agent_ASR_Run_Session 是阻塞的，我们这里暂时用 Task 包装一下
            // 或者直接在这里调用（会阻塞状态机，不推荐，但为了测试先这样）
            // 更好的做法是发送一个信号给 ASR 任务
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
                
                // 识别成功 -> 进入处理状态
                s_current_state = SYS_STATE_PROCESSING;
                EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_PROCESSING, 0);
                
                // [修改] 启动 LampMind 请求任务
                // 注意：evt->data 的内存将由 Agent_LampMind_Chat_Task 负责释放，这里不再 free
                xTaskCreate(Agent_LampMind_Chat_Task, "LampMind_Task", 8192, evt->data, 5, NULL);
                
            } else {
                // 识别为空 -> 回到 IDLE
                ESP_LOGW(TAG, "[LISTENING] ASR Empty -> Back to IDLE");
                s_current_state = SYS_STATE_IDLE;
                EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_IDLE, 0);
            }
            break;
            
        case EVT_KEY_CLICK:
            ESP_LOGI(TAG, "[LISTENING] Key Click -> Cancel -> Switch to IDLE");
            Agent_ASR_Stop(); // 强制停止录音
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
            
            // [新增] 打印 LLM 回复的文本，并释放内存
            if (evt->data) {
                ESP_LOGI(TAG, ">>> LampMind Reply: %s", (char*)evt->data);
                free(evt->data); // 释放由 Agent_LampMind 分配的 reply_text 内存
            } else {
                ESP_LOGW(TAG, ">>> LampMind Reply: (Empty/Error)");
            }

            s_current_state = SYS_STATE_SPEAKING;
            EventBus_Send(EVT_SYS_STATE_CHANGE, (void*)SYS_STATE_SPEAKING, 0);
            
            // 模拟 TTS 播放完成 (后续替换为真实 TTS)
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

// --- 主循环 ---

void Service_Core_Task(void *pvParameters) {
    ESP_LOGI(TAG, "Service Core Started");
    
    EventBus_Init();
    
    SystemEvent_t evt;
    while (1) {
        // 阻塞等待事件
        if (EventBus_Receive(&evt, portMAX_DELAY) == ESP_OK) {
            
            // 全局事件处理 (优先级最高)
            if (evt.type == EVT_SYS_ERROR) {
                ESP_LOGE(TAG, "System Error: %d", (int)evt.data);
                s_current_state = SYS_STATE_ERROR;
            }
            
            // 状态机分发
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
        }
    }
}

void Service_Core_Init(void) {
    xTaskCreatePinnedToCore(Service_Core_Task, "Svc_Core", 4096, NULL, 5, NULL, 0);
}

