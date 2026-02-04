/**
  ******************************************************************************
  * @file    SystemModel.c
  * @brief   系统数据模型实现
  ******************************************************************************
  */
#include "SystemModel.h"
#include <string.h>

// 全局模型实例
SystemModel_t g_SystemModel;

/**
  * @brief  初始化系统模型
  * @note   设置默认值
  */
	
	
	
/**
  * @brief  光强值 (Brightness)
  * @note   取值范围：0 (全灭) ~ 1000 (全亮)
  * @details 
  * 物理意义：代表灯具输出的总光能量。值越高，LED的总占空比越大，光线越强。
  * 控制策略：直接映射到PWM输出占空比，线性控制整体亮度。
  */
	
	
	
/**
	* @brief  色温/色暖值 (ColorTemp)
	* @note   取值范围：0 (最暖/黄) ~ 1000 (最冷/蓝白)
	* @details 
	* 物理意义：代表冷暖两路灯珠的功率分配比例。
	* - 值为0时：纯暖色（黄光）。暖色灯珠达到Brightness设定的亮度，冷色灯珠完全熄灭。
	* - 值为500时：中性光（自然光）。冷暖灯珠各占50%的能量。
	* - 值为1000时：纯冷色（蓝白光）。冷色灯珠达到Brightness设定的亮度，暖色灯珠完全熄灭。
	* 
	* 实现方式：通过两个独立的PWM通道分别控制暖色和冷色LED组，根据ColorTemp计算各自的占空比。
	*/
	
	
void SystemModel_Init(void)
{
    // 1. 清零所有数据
    memset(&g_SystemModel, 0, sizeof(SystemModel_t));

	
	
    // 2. 设置灯光默认值
    g_SystemModel.Light.Brightness = 500;
    g_SystemModel.Light.ColorTemp = 500;
    g_SystemModel.Light.Focus = FOCUS_BRIGHTNESS;
    g_SystemModel.Light.IsLongPressMode = 0;

    // 3. 设置传感器默认值
    g_SystemModel.Sensor.Temperature = 0.0f;
    g_SystemModel.Sensor.Humidity = 0.0f;
    g_SystemModel.Sensor.Lux = 0.0f;
}
