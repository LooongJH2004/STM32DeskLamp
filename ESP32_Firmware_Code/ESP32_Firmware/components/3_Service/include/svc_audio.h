#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * @brief 初始化音频播放服务 (创建 RingBuffer 和 播放任务)
 */
void Svc_Audio_Init(void);

/**
 * @brief 向播放缓冲区喂入 PCM 数据 (生产者调用)
 * @note 如果缓冲区已满，此函数会阻塞等待，直到数据全部写入 (产生背压)
 * @param data PCM 数据指针
 * @param len 数据长度 (字节)
 */
void Svc_Audio_Feed_Data(const uint8_t *data, size_t len);

/**
 * @brief 立即停止播放并清空缓冲区 (用于语音打断)
 */
void Svc_Audio_Stop(void);
