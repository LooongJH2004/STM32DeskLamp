#include "dev_audio.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "Dev_Audio";
static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL; 

static uint8_t s_out_volume = 50; 

esp_err_t Dev_Audio_Init(const Audio_Config_t *cfg) {
    ESP_LOGI(TAG, "Initializing I2S for INMP441 (RX) & MAX98357A (TX)...");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    // 配置 RX (麦克风) - 32bit 单声道
    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->bck_io_num,
            .ws = cfg->ws_io_num,
            .dout = I2S_GPIO_UNUSED,
            .din = cfg->data_in_num,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    rx_std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_cfg));

    // 配置 TX (功放) - 16bit 单声道
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        // 这里保持 16BIT 和 MONO，ESP-IDF 会自动处理双槽复制，满足功放时钟要求
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->bck_io_num,
            .ws = cfg->ws_io_num,
            .dout = cfg->data_out_num, 
            .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    // 注意：这里已经删除了之前那个导致频率降低的 slot_bit_width = 32 错误配置！
    tx_std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
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

esp_err_t Dev_Audio_Write(const void *buffer, size_t len, size_t *bytes_written) {
    if (!tx_handle) return ESP_FAIL;
    
    int16_t temp_buf[128]; 
    size_t total_written = 0;
    const int16_t *src = (const int16_t *)buffer;
    size_t samples_to_write = len / sizeof(int16_t);

    while (samples_to_write > 0) {
        size_t chunk_samples = (samples_to_write > 128) ? 128 : samples_to_write;
        
        // 正常的音量缩放逻辑，不会改变频率
        for (size_t i = 0; i < chunk_samples; i++) {
            int32_t val = src[i];
            val = (val * s_out_volume) / 100; 
            temp_buf[i] = (int16_t)val;
        }

        size_t bw = 0;
        esp_err_t err = i2s_channel_write(tx_handle, temp_buf, chunk_samples * sizeof(int16_t), &bw, portMAX_DELAY);
        if (err != ESP_OK) return err;
        
        total_written += bw;
        src += chunk_samples;
        samples_to_write -= chunk_samples;
    }

    if (bytes_written) *bytes_written = total_written;
    return ESP_OK;
}
