#pragma once

/**
 * @brief 请求百度 TTS 并流式播放音频
 * @note 这是一个阻塞函数，会边下载边将数据喂给 Svc_Audio 的 RingBuffer。
 *       直到音频全部下载完毕才会返回。
 * @param text 要播报的 UTF-8 文本 (最长约 300 个汉字)
 */
void Agent_TTS_Play(const char *text);
