/**
  ******************************************************************************
  * @file    SystemModel.h
  * @brief   系统数据模型定义 (Data Center) - 结构化重构版
  * @note    存储系统所有业务模块的运行时状态。
  ******************************************************************************
  */
#ifndef __SYSTEM_MODEL_H
#define __SYSTEM_MODEL_H

#include <stdint.h>

/**
  * @brief 灯光控制焦点枚举
  */
typedef enum {
    FOCUS_BRIGHTNESS = 0,   /*!< 焦点在亮度调节 */
    FOCUS_COLOR_TEMP        /*!< 焦点在色温调节 */
} LightFocus_t;

/**
  * @brief [域] 灯光业务数据模型
  */
typedef struct {
    int16_t Brightness;      /*!< 当前亮度值 (0-1000) */
    int16_t ColorTemp;       /*!< 当前色温值 (0-1000) */
    uint8_t IsLongPressMode; /*!< 是否处于长按临时模式 (0/1) */
    LightFocus_t Focus;      /*!< 当前编码器控制焦点 */
    // uint8_t AutoMode;     /*!< 自动调光模式 (预留) */
} Model_Light_t;

/**
  * @brief [域] 传感器业务数据模型
  */
typedef struct {
    float Temperature;       /*!< 环境温度 (摄氏度) */
    float Humidity;          /*!< 环境湿度 (%) */
    float Lux;               /*!< 环境光照强度 (Lux) - 预留 */
    // uint8_t HumanPresence;/*!< 人体存在状态 - 预留 */
} Model_Sensor_t;

/**
  * @brief 系统全局数据模型聚合
  */
typedef struct {
    Model_Light_t  Light;    /*!< 灯光子系统数据 */
    Model_Sensor_t Sensor;   /*!< 传感器子系统数据 */
} SystemModel_t;

/**
  * @brief 全局模型实例
  */
extern SystemModel_t g_SystemModel;

/**
  * @brief 模型初始化函数
  */
void SystemModel_Init(void);

#endif
