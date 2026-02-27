/**
 * @file    data_center.h
 * @brief   全局数据中心 (Single Source of Truth)
 * @note    所有跨任务共享的数据必须通过此模块访问，内部已实现互斥锁保护。
 *          当数据被修改时，会自动向 EventBus 发送对应的变更事件。
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// 1. 领域数据结构定义
// ============================================================

/** @brief 灯光状态域 */
typedef struct {
    bool    power;          /*!< 开关状态 (true=开, false=关) */
    uint8_t brightness;     /*!< 亮度百分比 (0-100) */
    uint8_t color_temp;     /*!< 色温百分比 (0=暖, 100=冷) */
} DC_LightingData_t;

/** @brief 环境状态域 */
typedef struct {
    int8_t  indoor_temp;    /*!< 室内温度 (摄氏度) */
    uint8_t indoor_hum;     /*!< 室内湿度 (%) */
    char    outdoor_weather[32]; /*!< 室外天气描述 (如 "晴", "多云") */
    int8_t  outdoor_temp;   /*!< 室外温度 */
} DC_EnvData_t;

/** @brief 定时器状态域 */
typedef enum {
    TIMER_IDLE = 0,
    TIMER_RUNNING,
    TIMER_PAUSED
} DC_TimerState_t;

typedef struct {
    DC_TimerState_t state;  /*!< 定时器当前状态 */
    uint32_t remain_sec;    /*!< 剩余秒数 */
    uint32_t total_sec;     /*!< 设定的总秒数 */
} DC_TimerData_t;

// ============================================================
// 2. 核心 API
// ============================================================

/**
 * @brief 初始化数据中心 (创建互斥锁，赋予默认值)
 */
void DataCenter_Init(void);

/**
 * @brief 打印当前数据中心的所有状态 (受 APP_DEBUG_PRINT 宏控制)
 */
void DataCenter_PrintStatus(void);

// --- Lighting 接口 ---
void DataCenter_Get_Lighting(DC_LightingData_t *out_data);
void DataCenter_Set_Lighting(const DC_LightingData_t *in_data);

// --- Environment 接口 ---
void DataCenter_Get_Env(DC_EnvData_t *out_data);
void DataCenter_Set_Env(const DC_EnvData_t *in_data);

// --- Timer 接口 ---
void DataCenter_Get_Timer(DC_TimerData_t *out_data);
void DataCenter_Set_Timer(const DC_TimerData_t *in_data);
