#pragma once
#include "system_types.h"
#include "esp_err.h"

// 初始化事件总线
esp_err_t EventBus_Init(void);

// 发送事件 (线程安全，可在中断中调用)
// data: 如果是动态分配的内存，接收方负责释放
esp_err_t EventBus_Send(EventType_t type, void *data, int len);

// 接收事件 (阻塞等待)
// timeout_ms: 等待超时时间，portMAX_DELAY 表示无限等待
esp_err_t EventBus_Receive(SystemEvent_t *evt, uint32_t timeout_ms);
