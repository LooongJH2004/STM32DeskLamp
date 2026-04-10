#include "ring_buffer.h"
#include "esp_heap_caps.h"
#include <string.h>

RingBuffer_t* RingBuffer_Create(size_t size) {
    RingBuffer_t *rb = (RingBuffer_t *)heap_caps_malloc(sizeof(RingBuffer_t), MALLOC_CAP_INTERNAL);
    if (!rb) return NULL;

    // 音频缓冲区通常较大，建议放在 SPIRAM (如果开启) 或内部 RAM
    rb->buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->mutex = xSemaphoreCreateMutex();
    return rb;
}

size_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, size_t len) {
    xSemaphoreTake(rb->mutex, portMAX_DELAY);
    
    size_t free_space = rb->size - rb->count;
    if (len > free_space) len = free_space; // 丢弃溢出部分，或者返回错误

    size_t first_chunk = rb->size - rb->head;
    if (len <= first_chunk) {
        memcpy(&rb->buffer[rb->head], data, len);
        rb->head += len;
        if (rb->head >= rb->size) rb->head = 0;
    } else {
        memcpy(&rb->buffer[rb->head], data, first_chunk);
        memcpy(&rb->buffer[0], data + first_chunk, len - first_chunk);
        rb->head = len - first_chunk;
    }

    rb->count += len;
    xSemaphoreGive(rb->mutex);
    return len;
}

size_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, size_t len) {
    xSemaphoreTake(rb->mutex, portMAX_DELAY);

    if (len > rb->count) len = rb->count;

    size_t first_chunk = rb->size - rb->tail;
    if (len <= first_chunk) {
        memcpy(data, &rb->buffer[rb->tail], len);
        rb->tail += len;
        if (rb->tail >= rb->size) rb->tail = 0;
    } else {
        memcpy(data, &rb->buffer[rb->tail], first_chunk);
        memcpy(data + first_chunk, &rb->buffer[0], len - first_chunk);
        rb->tail = len - first_chunk;
    }

    rb->count -= len;
    xSemaphoreGive(rb->mutex);
    return len;
}

size_t RingBuffer_GetCount(const RingBuffer_t *rb) {
    return rb ? rb->count : 0;
}
