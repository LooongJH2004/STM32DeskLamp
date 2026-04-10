#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// 定义配置结构体
typedef struct {
    int bck_io_num;     // BCLK 引脚 (共享)
    int ws_io_num;      // LRCK 引脚 (共享)
    int data_in_num;    // DIN/SD 引脚 (麦克风)
    int data_out_num;   // DOUT/DIN 引脚 (功放) [新增]
    int sample_rate;    // 采样率 (e.g. 16000)
} Audio_Config_t;

// 初始化 I2S (传入配置参数)
esp_err_t Dev_Audio_Init(const Audio_Config_t *cfg);

// 读取原始音频数据 (麦克风)
esp_err_t Dev_Audio_Read(void *buffer, size_t len, size_t *bytes_read);

// [新增] 写入音频数据 (功放)
// buffer: 必须是 16bit signed PCM 数据
esp_err_t Dev_Audio_Write(const void *buffer, size_t len, size_t *bytes_written);

// [新增] 设置播放音量
// volume: 0 - 100
void Dev_Audio_Set_Volume(uint8_t volume);
