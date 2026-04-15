#include "dev_stm32.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"
#include "data_center.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "crc16.h" 
#include <string.h>
#include <stdlib.h> 

static const char *TAG = "Dev_STM32";

#define UART_NUM        UART_NUM_1
#define TX_PIN          17
#define RX_PIN          18
#define BUF_SIZE        1024

static SemaphoreHandle_t s_tx_mutex = NULL;

// ============================================================
// 发送底层：策略模式 (函数指针)
// ============================================================
typedef void (*crc_send_strategy_t)(const char *json_str);

// 策略1：正确的 CRC 计算
static void _send_crc_right(const char *json_str) {
    if (!s_tx_mutex) return;
    
    // 1. 计算正确的 CRC
    uint16_t calc_crc = CRC16_Calculate((const uint8_t *)json_str, strlen(json_str));
    uint16_t send_crc = calc_crc; // 正常发送正确的 CRC
    
    // 2. 本地自检 (模拟接收端校验，余数必然为 0)
    uint16_t remainder = calc_crc ^ send_crc; 
    
    // 3. 组装物理帧并发送
    char buf[256];
    snprintf(buf, sizeof(buf), "%s|%04X\r\n", json_str, send_crc);
    
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, buf, strlen(buf));
    xSemaphoreGive(s_tx_mutex);
    
    // 4. 打印格式：json|发送的CRC|本地校验余数
    ESP_LOGI(TAG, "[TX_RIGHT] %s|%04X|%04X", json_str, send_crc, remainder); 
}

// 策略2：错误的 CRC 计算 (主动反转，用于测试 STM32 的拦截率)
static void _send_crc_error(const char *json_str) {
    if (!s_tx_mutex) return;
    
    // 1. 计算正确的 CRC
    uint16_t calc_crc = CRC16_Calculate((const uint8_t *)json_str, strlen(json_str));
    
    // 2. 制造错误：按位取反
    uint16_t send_crc = ~calc_crc; 
    
    // 3. 本地自检 (模拟接收端校验，余数必然不为 0)
    uint16_t remainder = calc_crc ^ send_crc; 
    
    // 4. 组装物理帧并发送
    char buf[256];
    snprintf(buf, sizeof(buf), "%s|%04X\r\n", json_str, send_crc);
    
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, buf, strlen(buf));
    xSemaphoreGive(s_tx_mutex);
    
    // 5. 打印格式：json|发送的错误CRC|本地校验余数 (非0)
    ESP_LOGW(TAG, "[TX_ERROR] %s|%04X|%04X", json_str, send_crc, remainder); 
}

// 当前使用的发送策略 (默认正确)
static crc_send_strategy_t s_current_send_strategy = _send_crc_right;

void Dev_STM32_Set_CRC_Mode(uint8_t mode) {
    if (mode == 0) {
        s_current_send_strategy = _send_crc_right;
        ESP_LOGW(TAG, ">>> Switched to CRC Mode: RIGHT (Normal) <<<");
    } else {
        s_current_send_strategy = _send_crc_error;
        ESP_LOGW(TAG, ">>> Switched to CRC Mode: ERROR (Inverted) <<<");
    }
}

static void _send_raw(const char *json_str) {
    if (s_current_send_strategy) {
        s_current_send_strategy(json_str);
    }
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

// ============================================================
// 接收底层：自动校验并展示余数
// ============================================================
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
                        
                        char *sep = strrchr(line_buf, '|'); 
                        if (sep != NULL) {
                            size_t json_len = sep - line_buf;
                            uint16_t calc_crc = CRC16_Calculate((const uint8_t *)line_buf, json_len);
                            uint16_t recv_crc = (uint16_t)strtol(sep + 1, NULL, 16);
                            
                            // 【关键】在模2除法中，相同数据的异或差值即为余数。余数为0代表校验通过。
                            uint16_t remainder = calc_crc ^ recv_crc;
                            
                            *sep = '\0'; // 截断字符串，只保留纯 JSON
                            
                            if (remainder == 0) {
                                // 校验通过，打印格式：json|crc原始值|crc验证的余数值
                                ESP_LOGI(TAG, "[RX] %s|%04X|%04X", line_buf, recv_crc, remainder);
                                
                                cJSON *json = cJSON_Parse(line_buf);
                                if (json) {
                                    cJSON *ev = cJSON_GetObjectItem(json, "ev");
                                    if (ev && ev->valuestring) {
                                        if (strcmp(ev->valuestring, "env") == 0) {
                                            DC_EnvData_t env;
                                            DataCenter_Get_Env(&env);
                                            cJSON *t = cJSON_GetObjectItem(json, "t");
                                            cJSON *h = cJSON_GetObjectItem(json, "h");
                                            cJSON *l = cJSON_GetObjectItem(json, "l");
                                            if (t) env.indoor_temp = t->valueint;
                                            if (h) env.indoor_hum = h->valueint;
                                            if (l) env.indoor_lux = l->valueint;
                                            DataCenter_Set_Env(&env);
                                        }
                                        else if (strcmp(ev->valuestring, "state") == 0) {
                                            cJSON *warm_item = cJSON_GetObjectItem(json, "warm");
                                            cJSON *cold_item = cJSON_GetObjectItem(json, "cold");
                                            if (warm_item && cold_item) {
                                                int warm = warm_item->valueint;
                                                int cold = cold_item->valueint;
                                                int total_pwm = warm + cold;
                                                DC_LightingData_t light;
                                                DataCenter_Get_Lighting(&light);
                                                light.brightness = total_pwm / 10;
                                                light.color_temp = total_pwm > 0 ? (cold * 100) / total_pwm : light.color_temp;
                                                light.power = (total_pwm > 0);
                                                DataCenter_Set_Lighting(&light);
                                            }
                                        }
                                    }
                                    cJSON_Delete(json);
                                }
                            } else {
                                // 校验失败，打印非零余数并丢弃
                                ESP_LOGE(TAG, "[RX_DROP] %s|%04X|%04X", line_buf, recv_crc, remainder);
                            }
                        }
                        line_len = 0; 
                    }
                } else {
                    if (line_len < sizeof(line_buf) - 1) line_buf[line_len++] = c;
                }
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

void Dev_STM32_Init(void) {
    if (s_tx_mutex == NULL) s_tx_mutex = xSemaphoreCreateMutex();

    uart_config_t uart_config = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(stm32_rx_task, "stm32_rx", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "STM32 UART Initialized with Dynamic CRC Strategy.");
    
    Dev_STM32_Set_Mode(1); 
}
