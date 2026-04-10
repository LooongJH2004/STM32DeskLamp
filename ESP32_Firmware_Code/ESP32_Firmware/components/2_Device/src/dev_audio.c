#include "dev_audio.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "Dev_Audio";
static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL; 

static uint8_t s_out_volume = 10; // 默认音量设低一点

esp_err_t Dev_Audio_Init(const Audio_Config_t *cfg) {
    ESP_LOGI(TAG, "Initializing I2S for INMP441 (RX) & MAX98357A (TX)...");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    // 1. 配置 RX (麦克风) - 内存 32bit，物理 32bit
    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = cfg->bck_io_num, .ws = cfg->ws_io_num,
            .dout = I2S_GPIO_UNUSED, .din = cfg->data_in_num,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    rx_std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    // 强制物理线宽为 32bit，确保 BCLK 频率与 TX 一致
    rx_std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT; 
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_cfg));

    // 2. 配置 TX (功放) - 【核心修复】内存 16bit，物理 32bit
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        // 告诉 DMA：内存数据是 16-bit 的！
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = cfg->bck_io_num, .ws = cfg->ws_io_num,
            .dout = cfg->data_out_num, .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    // 【魔法在这里】强制物理线宽为 32bit。DMA 会自动把 16bit 数据放在高位，低位补零！
    tx_std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT; 
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &tx_std_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    
    ESP_LOGI(TAG, "I2S TX & RX Initialized Successfully.");
    return ESP_OK;
}

esp_err_t Dev_Audio_Read(void *buffer, size_t len, size_t *bytes_read) {
    if (!rx_handle) return ESP_FAIL;
    return i2s_channel_read(rx_handle, buffer, len, bytes_read, portMAX_DELAY);
}

void Dev_Audio_Set_Volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    s_out_volume = volume;
    ESP_LOGI(TAG, "Volume set to %d%%", s_out_volume);
}

// ============================================================
// 【重构】绝对安全的音频写入与音量控制
// ============================================================
// ============================================================
// 替换 components/2_Device/src/dev_audio.c 中的 Dev_Audio_Write
// ============================================================

esp_err_t Dev_Audio_Write(const void *buffer, size_t len, size_t *bytes_written) {
    if (!tx_handle) return ESP_FAIL;
    
    size_t samples = len / sizeof(int16_t);
    int16_t *processed_buf = malloc(len);
    if (!processed_buf) return ESP_ERR_NO_MEM;

    const int16_t *src = (const int16_t *)buffer;
    
    // 静态变量，用于保存上一个采样点，实现 IIR 滤波器
    static int32_t s_prev_x = 0;
    static int32_t s_prev_y = 0;

    for (size_t i = 0; i < samples; i++) {
        int32_t x = src[i]; 
        
        // 1. 【新增】DC Blocker (去直流偏置滤波器)
        // 公式: y[n] = x[n] - x[n-1] + R * y[n-1], R 取 0.995
        // 使用定点数移位优化乘法: 0.995 * 32768 ≈ 32604
        int32_t y = x - s_prev_x + ((s_prev_y * 32604) >> 15);
        s_prev_x = x;
        s_prev_y = y;
        
        // 2. 【优化】合并音量与硬件衰减，减少量化截断误差
        // vol_factor 是 0-100。最大乘积是 10000。
        // 硬件需要衰减 4 倍 (防止 MAX98357A 削顶)，所以总除数是 40000。
        int32_t val = (y * s_out_volume * s_out_volume) / 40000; 
        
        // 3. 硬限幅保护
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        
        processed_buf[i] = (int16_t)val; 
    }

    esp_err_t err = i2s_channel_write(tx_handle, processed_buf, len, bytes_written, portMAX_DELAY);
    
    free(processed_buf);
    return err;
}

