#pragma once
#include <stdbool.h>

/**
 * @brief 请求 LampMind Server 进行对话处理 (作为独立任务运行)
 * 
 * @param pvParameters 传入 ASR 识别出的文本字符串指针 (char*)
 * @note 任务执行完毕后会自动释放传入的字符串内存，并发送 EVT_LLM_RESULT 事件。
 */
void Agent_LampMind_Chat_Task(void *pvParameters);
