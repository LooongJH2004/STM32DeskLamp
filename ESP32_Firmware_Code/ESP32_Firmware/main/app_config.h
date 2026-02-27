#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// --- Audio System (I2S0) ---
#define AUDIO_I2S_BCK_PIN       42
#define AUDIO_I2S_WS_PIN        41
#define AUDIO_I2S_DATA_IN_PIN   40
#define AUDIO_SAMPLE_RATE       16000
#define AUDIO_BIT_WIDTH         32

// --- VAD (Voice Activity Detection) Settings ---
// 静音阈值 (0-32767): 低于此值判定为静音
// INMP441 底噪通常在 200-500 左右，建议设为 800-1500
#define VAD_SILENCE_THRESHOLD   1000  

// 静音超时 (ms): 连续静音超过此时间，自动停止录音
#define VAD_SILENCE_DURATION_MS 2500  

// 最大录音时长 (ms): 百度限制 60秒
#define ASR_MAX_DURATION_MS     60000 

// --- Button System ---
// [修改] 使用外接按钮 GPIO 21
// 接线方式: GPIO 21 <--> 按钮 <--> GND
#define BOARD_BUTTON_PIN        21 

/**
 * @brief 业务层调试日志开关
 * @note  1: 开启打印; 0: 关闭打印 (节省 CPU 和 串口带宽)
 */
#define APP_DEBUG_PRINT         1

#if (APP_DEBUG_PRINT == 1)
    #include "esp_log.h"
    #define APP_LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
    #define APP_LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
    #define APP_LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#else
    #define APP_LOGI(tag, format, ...) do {} while(0)
    #define APP_LOGW(tag, format, ...) do {} while(0)
    #define APP_LOGE(tag, format, ...) do {} while(0)
#endif

#endif // APP_CONFIG_H