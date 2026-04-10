#include "dev_stm32.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"
#include "data_center.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "crc16.h" // [新增] 引入 CRC16 校验库
#include <string.h>
#include <stdlib.h> // 用于 strtol

static const char *TAG = "Dev_STM32";

#define UART_NUM        UART_NUM_1
#define TX_PIN          17
#define RX_PIN          18
#define BUF_SIZE        1024

// 串口发送互斥锁，保证多任务并发发送时帧不被截断
static SemaphoreHandle_t s_tx_mutex = NULL;

// ============================================================
// 发送底层：自动计算并追加 CRC16
// ============================================================
static void _send_raw(const char *json_str) {
    if (!s_tx_mutex) return;

    // 1. 计算纯 JSON 字符串的 CRC16
    uint16_t crc = CRC16_Calculate((const uint8_t *)json_str, strlen(json_str));

    // 2. 拼接格式: [JSON]|XXXX\r\n
    char buf[256];
    snprintf(buf, sizeof(buf), "%s|%04X\r\n", json_str, crc);
    
    // 3. 获取互斥锁，保证这一帧完整发完
    xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, buf, strlen(buf));
    xSemaphoreGive(s_tx_mutex);
    
    // 打印实际发送的完整帧 (包含 CRC)
    ESP_LOGI(TAG, "[STM32_TX] %s", buf); 
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
// 接收底层：自动校验并剥离 CRC16
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
                
                // 遇到换行符，说明收到了一整帧
                if (c == '\n' || c == '\r') {
                    if (line_len > 0) {
                        line_buf[line_len] = '\0'; // 封闭字符串
                        
                        // --- [新增] CRC 校验逻辑开始 ---
                        char *sep = strrchr(line_buf, '|'); // 从后往前找分隔符
                        
                        if (sep != NULL) {
                            // 1. 计算本地 CRC (只计算 '|' 前面的 JSON 部分)
                            size_t json_len = sep - line_buf;
                            uint16_t calc_crc = CRC16_Calculate((const uint8_t *)line_buf, json_len);
                            
                            // 2. 提取接收到的 CRC (跳过 '|' 字符)
                            uint16_t recv_crc = (uint16_t)strtol(sep + 1, NULL, 16);
                            
                            // 3. 比对 CRC
                            if (calc_crc == recv_crc) {
                                // 校验通过！将 '|' 替换为 '\0'，截断字符串，只保留纯 JSON
                                *sep = '\0'; 
                                //ESP_LOGI(TAG, "[STM32_RX] Valid: %s", line_buf);//纯json
                                ESP_LOGI(TAG, "[STM32_RX] Valid: %s | CRC:%04X", line_buf, recv_crc);//带crc计算的json，便于调试
                                
                                // --- 原有的 JSON 解析逻辑 ---
                                cJSON *json = cJSON_Parse(line_buf);
                                if (json) {
                                    cJSON *ev = cJSON_GetObjectItem(json, "ev");
                                    if (ev && ev->valuestring) {
                                        // 1. 处理环境数据上报
                                        if (strcmp(ev->valuestring, "env") == 0) {
                                            DC_EnvData_t env;
                                            DataCenter_Get_Env(&env);
                                            
                                            cJSON *t = cJSON_GetObjectItem(json, "t");
                                            cJSON *h = cJSON_GetObjectItem(json, "h");
                                            cJSON *l = cJSON_GetObjectItem(json, "l");
                                            
                                            if (t && cJSON_IsNumber(t)) env.indoor_temp = t->valueint;
                                            if (h && cJSON_IsNumber(h)) env.indoor_hum = h->valueint;
                                            if (l && cJSON_IsNumber(l)) env.indoor_lux = l->valueint;
                                            
                                            DataCenter_Set_Env(&env);
                                        }
                                        // 2. 处理灯光状态同步 (反向同步)
                                        else if (strcmp(ev->valuestring, "state") == 0) {
                                            cJSON *warm_item = cJSON_GetObjectItem(json, "warm");
                                            cJSON *cold_item = cJSON_GetObjectItem(json, "cold");
                                            
                                            if (warm_item && cJSON_IsNumber(warm_item) && 
                                                cold_item && cJSON_IsNumber(cold_item)) {
                                                
                                                int warm = warm_item->valueint;
                                                int cold = cold_item->valueint;
                                                int total_pwm = warm + cold;
                                                
                                                DC_LightingData_t light;
                                                DataCenter_Get_Lighting(&light);
                                                
                                                int new_bri = total_pwm / 10;
                                                if (new_bri > 100) new_bri = 100;
                                                
                                                int new_cct = 0;
                                                if (total_pwm > 0) {
                                                    new_cct = (cold * 100) / total_pwm;
                                                } else {
                                                    new_cct = light.color_temp; 
                                                }
                                                if (new_cct > 100) new_cct = 100;

                                                light.brightness = new_bri;
                                                light.color_temp = new_cct;
                                                light.power = (total_pwm > 0);
                                                
                                                DataCenter_Set_Lighting(&light);
                                            }
                                        }
                                    }
                                    cJSON_Delete(json);
                                }
                            } else {
                                // 校验失败，静默丢弃 (仅打印警告)
                                ESP_LOGW(TAG, "[STM32_RX] CRC Error! Calc:%04X Recv:%04X. Drop: %s", calc_crc, recv_crc, line_buf);
                            }
                        } else {
                            // 找不到分隔符，说明格式错误，静默丢弃
                            ESP_LOGW(TAG, "[STM32_RX] Missing CRC separator. Drop: %s", line_buf);
                        }
                        // --- [新增] CRC 校验逻辑结束 ---
                        
                        line_len = 0; // 准备接收下一帧
                    }
                } else {
                    // 收集字符
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
    ESP_LOGI(TAG, "STM32 UART Initialized with CRC16 Support.");
    
    // 启动时强制让 STM32 进入 Remote UI 模式
    Dev_STM32_Set_Mode(1); 
}
