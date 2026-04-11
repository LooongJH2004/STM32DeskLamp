/**
 * @file    data_center.h
 * @brief   全局数据中心 (Single Source of Truth)
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// 1. 领域数据结构定义
// ============================================================

/** @brief 灯光状态域 (需要掉电保存) */
typedef struct {
    bool    power;          /*!< 开关状态 (true=开, false=关) */
    uint8_t brightness;     /*!< 亮度百分比 (0-100) */
    uint8_t color_temp;     /*!< 色温百分比 (0=暖, 100=冷) */
} DC_LightingData_t;

/** @brief 系统设置域 (需要掉电保存) [新增] */
typedef struct {
    uint8_t volume;             /*!< 语音播报音量 (0-100) */
    uint8_t screen_brightness;  /*!< 屏幕背光亮度 (0-100) */
} DC_SystemData_t;

/** @brief 环境状态域 (无需保存) */
typedef struct {
    int8_t  indoor_temp;    
    uint8_t indoor_hum;     
    uint16_t indoor_lux;    
    char    outdoor_weather[32]; 
    int8_t  outdoor_temp;   
} DC_EnvData_t;

/** @brief 定时器状态域 (无需保存) */
typedef enum { TIMER_IDLE = 0, TIMER_RUNNING, TIMER_PAUSED } DC_TimerState_t;

typedef struct {
    DC_TimerState_t state;  
    uint32_t remain_sec;    
    uint32_t total_sec;     
} DC_TimerData_t;

// ============================================================
// 2. 核心 API
// ============================================================
void DataCenter_Init(void);
void DataCenter_PrintStatus(void);

void DataCenter_Get_Lighting(DC_LightingData_t *out_data);
void DataCenter_Set_Lighting(const DC_LightingData_t *in_data);

void DataCenter_Get_System(DC_SystemData_t *out_data);
void DataCenter_Set_System(const DC_SystemData_t *in_data);

void DataCenter_Get_Env(DC_EnvData_t *out_data);
void DataCenter_Set_Env(const DC_EnvData_t *in_data);

void DataCenter_Get_Timer(DC_TimerData_t *out_data);
void DataCenter_Set_Timer(const DC_TimerData_t *in_data);
