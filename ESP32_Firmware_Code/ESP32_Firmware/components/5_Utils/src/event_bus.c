#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "EventBus";
static QueueHandle_t s_event_queue = NULL;

#define EVENT_QUEUE_SIZE 20 // 队列深度

esp_err_t EventBus_Init(void) {
    if (s_event_queue) return ESP_OK;

    s_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(SystemEvent_t));
    if (!s_event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t EventBus_Send(EventType_t type, void *data, int len) {
    if (!s_event_queue) return ESP_FAIL;

    SystemEvent_t evt;
    evt.type = type;
    evt.data = data;
    evt.len = len;
    evt.timestamp = xTaskGetTickCount();

    // 如果在中断中
    if (xPortInIsrContext()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xQueueSendFromISR(s_event_queue, &evt, &xHigherPriorityTaskWoken) != pdTRUE) {
            return ESP_FAIL; // 队列满
        }
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    } else {
        if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Queue full, event dropped: %d", type);
            // 注意：如果 data 是动态分配的，这里应该释放，防止内存泄漏
            // 但为了通用性，暂不处理，调用者需注意
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t EventBus_Receive(SystemEvent_t *evt, uint32_t timeout_ms) {
    if (!s_event_queue) return ESP_FAIL;
    
    if (xQueueReceive(s_event_queue, evt, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
