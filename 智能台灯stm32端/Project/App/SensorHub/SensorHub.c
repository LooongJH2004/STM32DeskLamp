/**
  * @file    SensorHub.c
  * @brief   传感器中心 (V6.5 Data Sync Fix)
  */
#include "SensorHub.h"
#include "DHT11.h"
#include "LDR.h"          // <--- 新增
#include "Protocol.h"
#include "USART_DMA.h"
#include "SystemModel.h"  // <--- 新增：用于更新本地模型

void SensorHub_Init(void)
{
    // 初始化 DHT11
    if (DHT11_Init() == 0)
    {
        USART_DMA_Printf("[Sensor] DHT11 Init Success.\r\n");
    }
    else
    {
        USART_DMA_Printf("[Sensor] DHT11 Init Failed!\r\n");
        // 设置错误标记
        g_SystemModel.Sensor.Temperature = -99.0f;
    }
    
    // 初始化 LDR
    LDR_Init(); 
}

void SensorHub_Task(void)
{
    uint8_t temp_int, humi_int;
    uint16_t lux_percent = 0;
    
    // 1. 读取光强
    lux_percent = LDR_GetLuxPercentage();
    
    // 2. 读取温湿度
    if (DHT11_Read_Data(&temp_int, &humi_int) == 0)
    {
        // 【关键修复】更新本地数据模型 (供 OLED 显示)
        g_SystemModel.Sensor.Temperature = (float)temp_int;
        g_SystemModel.Sensor.Humidity = (float)humi_int;
        g_SystemModel.Sensor.Lux = (float)lux_percent;

        // 上报给 ESP32
        Protocol_Report_Env((int8_t)temp_int, humi_int, lux_percent);
    }
    else
    {
        // 读取失败处理
        g_SystemModel.Sensor.Temperature = -99.0f; // 错误码
        USART_DMA_Printf("[Sensor] DHT11 Read Error\r\n");
    }
}
