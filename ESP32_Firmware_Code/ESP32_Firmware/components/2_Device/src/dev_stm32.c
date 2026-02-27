#include "dev_stm32.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"
#include "data_center.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "Dev_STM32";

#define UART_NUM        UART_NUM_1
#define TX_PIN          17
#define RX_PIN          18
#define BUF_SIZE        1024

// [新增] 串口发送互斥锁，保证多任务并发发送时 JSON 帧不被截断
static SemaphoreHandle_t s_tx_mutex = NULL;

// 发送原始 JSON 字符串 (自动追加 \r\n)
static void _send_raw(const char *json_str) {
    if (!s_tx_mutex) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s\r\n", json_str);
    
    // 获取互斥锁，保证这一帧完整发完
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, buf, strlen(buf));
    xSemaphoreGive(s_tx_mutex);
    
    ESP_LOGI(TAG, "[STM32_TX] %s", json_str);
}

void Dev_STM32_Set_Light(uint16_t warm, uint16_t cold) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"light\",\"warm\":%d,\"cold\":%d}", warm, cold);
    _send_raw(buf);
}

void Dev_STM32_Set_Mode(uint8_t mode) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"mode\",\"val\":%d}", mode);
    _send_raw(buf);
}

// 串口接收任务
static void stm32_rx_task(void *arg) {
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    char line_buf[512];
    int line_len = 0;

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(20));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = (char)data[i];
                if (c == '\n' || c == '\r') {
                    if (line_len > 0) {
                        line_buf[line_len] = '\0';
                        ESP_LOGI(TAG, "[STM32_RX] %s", line_buf);

                        cJSON *json = cJSON_Parse(line_buf);
                        if (json) {
                            cJSON *ev = cJSON_GetObjectItem(json, "ev");
                            if (ev && ev->valuestring) {
                                // 处理环境数据上报
                                if (strcmp(ev->valuestring, "env") == 0) {
                                    DC_EnvData_t env;
                                    DataCenter_Get_Env(&env);
                                    
                                    cJSON *t = cJSON_GetObjectItem(json, "t");
                                    cJSON *h = cJSON_GetObjectItem(json, "h");
                                    if (t) env.indoor_temp = t->valueint;
                                    if (h) env.indoor_hum = h->valueint;
                                    
                                    DataCenter_Set_Env(&env);
                                }
                                // TODO: 后续在这里处理 "enc" (编码器) 和 "gest" (手势) 事件
                            }
                            cJSON_Delete(json);
                        }
                        line_len = 0;
                    }
                } else {
                    if (line_len < sizeof(line_buf) - 1) {
                        line_buf[line_len++] = c;
                    }
                }
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

void Dev_STM32_Init(void) {
    // [新增] 创建发送互斥锁
    if (s_tx_mutex == NULL) {
        s_tx_mutex = xSemaphoreCreateMutex();
    }

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(stm32_rx_task, "stm32_rx", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "STM32 UART Initialized.");
    
    // 启动时强制让 STM32 进入 Remote UI 模式
    Dev_STM32_Set_Mode(1); 
}
