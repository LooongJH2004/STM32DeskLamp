#pragma once
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    SemaphoreHandle_t mutex; // 互斥锁保证线程安全
} RingBuffer_t;

// 初始化缓冲区
RingBuffer_t* RingBuffer_Create(size_t size);
// 写入数据
size_t RingBuffer_Write(RingBuffer_t *rb, const uint8_t *data, size_t len);
// 读取数据
size_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data, size_t len);
// 获取当前缓冲区中存在的，但未被读取的数据量
size_t RingBuffer_GetCount(RingBuffer_t *rb);
