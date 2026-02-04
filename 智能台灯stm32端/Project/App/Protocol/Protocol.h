/* App/Protocol/Protocol.h */
/**
  ******************************************************************************
  * @file    Protocol.h
  * @brief   通信协议层接口定义
  * @note    负责 JSON 组包、解析与 QoS 流量控制
  ******************************************************************************
  */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>

/** @brief QoS 水位线阈值 (百分比) */
#define PROTOCOL_QOS_THRESHOLD  70

/* --- 回调函数类型定义 --- */
typedef void (*Proto_ModeCallback_t)(uint8_t mode);
typedef void (*Proto_LightCallback_t)(uint16_t warm, uint16_t cold);

/* --- 基础接口 --- */
void Protocol_Init(void);
void Protocol_Process(void);
void Protocol_OnRxData(uint8_t data);

/* --- 回调注册 --- */
void Protocol_SetModeCallback(Proto_ModeCallback_t cb);
void Protocol_SetLightCallback(Proto_LightCallback_t cb);

/* --- 发送接口 (高优先级) --- */
void Protocol_Report_Encoder(int16_t diff);

/**
 * @brief 上报按键事件
 * @param name   按键名称/ID (如 "ModeSW")
 * @param action 动作类型 (如 "click", "hold")
 */
void Protocol_Report_Key(const char* name, const char* action);

void Protocol_Report_Gesture(uint8_t gesture);

/* --- 发送接口 (低优先级 - QoS) --- */
void Protocol_Report_State(uint16_t warm, uint16_t cold);
void Protocol_Report_Env(int8_t temp, uint8_t humi, uint16_t lux);
void Protocol_Report_Heartbeat(uint32_t uptime);

#endif
