#include "svc_audio.h"
#include "ring_buffer.h"
#include "dev_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "Svc_Audio";

#define AUDIO_RB_SIZE (16 * 1024) 
#define PLAY_CHUNK_SIZE 1024      

static RingBuffer_t *s_audio_rb = NULL;

// 准备一段全 0 的静音数据
static const uint8_t silence_buf[PLAY_CHUNK_SIZE] = {0};

static void audio_play_task(void *arg) {
    ESP_LOGI(TAG, "Audio Play Task Started on Core 1");
    
    uint8_t *chunk_buf = malloc(PLAY_CHUNK_SIZE);
    if (!chunk_buf) {
        ESP_LOGE(TAG, "Failed to allocate chunk buffer");
        vTaskDelete(NULL);
        return;
    }

    bool is_buffering = true; 

    while (1) {
        size_t available = RingBuffer_GetCount(s_audio_rb);
        
        if (is_buffering) {
            if (available >= 12288) { // 提高预缓冲阈值到 4KB
                is_buffering = false;
                ESP_LOGI(TAG, "Buffering done, start playing.");
            } else {
                // [修复] 缓冲期间，主动向 I2S 写入静音数据，防止 DMA 欠载发出杂音！
                size_t bw;
                Dev_Audio_Write(silence_buf, PLAY_CHUNK_SIZE, &bw);
                continue;
            }
        }

        if (available > 0) {
            size_t read_len = (available > PLAY_CHUNK_SIZE) ? PLAY_CHUNK_SIZE : available;
            read_len = read_len & ~1; // 强制偶数对齐
            
            if (read_len == 0) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            
            RingBuffer_Read(s_audio_rb, chunk_buf, read_len);
            
            size_t bytes_written = 0;
            Dev_Audio_Write(chunk_buf, read_len, &bytes_written);
        } else {
            is_buffering = true;
            ESP_LOGW(TAG, "Buffer underrun! Re-buffering...");
        }
    }
    
    free(chunk_buf);
    vTaskDelete(NULL);
}

void Svc_Audio_Init(void) {
    if (s_audio_rb == NULL) {
        s_audio_rb = RingBuffer_Create(AUDIO_RB_SIZE);
    }
    xTaskCreatePinnedToCore(audio_play_task, "Audio_Play", 4096, NULL, 6, NULL, 1);
    ESP_LOGI(TAG, "Audio Service Initialized");
}

void Svc_Audio_Feed_Data(const uint8_t *data, size_t len) {
    if (!s_audio_rb || !data || len == 0) return;

    size_t written = 0;
    while (written < len) {
        size_t free_space = s_audio_rb->size - RingBuffer_GetCount(s_audio_rb);
        if (free_space > 0) {
            size_t to_write = len - written;
            if (to_write > free_space) to_write = free_space;
            
            RingBuffer_Write(s_audio_rb, data + written, to_write);
            written += to_write;
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void Svc_Audio_Stop(void) {
    if (s_audio_rb) {
        xSemaphoreTake(s_audio_rb->mutex, portMAX_DELAY);
        s_audio_rb->head = 0;
        s_audio_rb->tail = 0;
        s_audio_rb->count = 0;
        xSemaphoreGive(s_audio_rb->mutex);
        ESP_LOGI(TAG, "Audio Playback Stopped & Buffer Cleared");
    }
}
