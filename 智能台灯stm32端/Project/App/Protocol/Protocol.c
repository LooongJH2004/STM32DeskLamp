/* App/Protocol/Protocol.c */
/**
  ******************************************************************************
  * @file    Protocol.c
  * @brief   通信协议层实现
  ******************************************************************************
  */
#include "Protocol.h"
#include "USART_DMA.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

// --- 接收缓冲区 ---
#define RX_BUF_SIZE 256
static char s_RxBuf[RX_BUF_SIZE];
static volatile uint16_t s_RxIndex = 0;
static volatile uint8_t s_RxFrameReady = 0;

// --- 回调函数 ---
static Proto_ModeCallback_t s_ModeCb = NULL;
static Proto_LightCallback_t s_LightCb = NULL;

// --- 内部辅助：检查 QoS 水位线 ---
static int _CheckQoS(void)
{
    if (USART_DMA_GetUsage() > PROTOCOL_QOS_THRESHOLD) return 0;
    return 1;
}

void Protocol_Init(void)
{
    s_RxIndex = 0;
    s_RxFrameReady = 0;
}

void Protocol_OnRxData(uint8_t data)
{
    if (s_RxFrameReady) return;

    if (data == '\n' || data == '\r')
    {
        if (s_RxIndex > 0)
        {
            s_RxBuf[s_RxIndex] = '\0';
            s_RxFrameReady = 1;
        }
    }
    else
    {
        if (s_RxIndex < RX_BUF_SIZE - 1) s_RxBuf[s_RxIndex++] = data;
        else s_RxIndex = 0; // Overflow protection
    }
}

void Protocol_Process(void)
{
    if (!s_RxFrameReady) return;

    cJSON *root = cJSON_Parse(s_RxBuf);
    if (root)
    {
        cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
        if (cJSON_IsString(cmd))
        {
            // 1. 模式切换指令: {"cmd":"mode", "val":1}
            if (strcmp(cmd->valuestring, "mode") == 0)
            {
                cJSON *val = cJSON_GetObjectItem(root, "val");
                if (cJSON_IsNumber(val) && s_ModeCb)
                {
                    s_ModeCb((uint8_t)val->valueint);
                }
            }
            // 2. 灯光控制指令: {"cmd":"light", "warm":500, "cold":500}
            else if (strcmp(cmd->valuestring, "light") == 0)
            {
                cJSON *warm = cJSON_GetObjectItem(root, "warm");
                cJSON *cold = cJSON_GetObjectItem(root, "cold");
                
                if (cJSON_IsNumber(warm) && cJSON_IsNumber(cold) && s_LightCb)
                {
                    s_LightCb((uint16_t)warm->valueint, (uint16_t)cold->valueint);
                }
            }
        }
        cJSON_Delete(root);
    }
    else
    {
        USART_DMA_Printf("[Proto] JSON Parse Error: %s\r\n", s_RxBuf);
    }

    s_RxIndex = 0;
    s_RxFrameReady = 0;
}

void Protocol_SetModeCallback(Proto_ModeCallback_t cb) { s_ModeCb = cb; }
void Protocol_SetLightCallback(Proto_LightCallback_t cb) { s_LightCb = cb; }

/* --- 发送接口实现 --- */

void Protocol_Report_Encoder(int16_t diff)
{
    USART_DMA_Printf("{\"ev\":\"enc\",\"diff\":%d}\r\n", diff);
}

// [修改] 增加 id 字段上报
void Protocol_Report_Key(const char* name, const char* action)
{
    USART_DMA_Printf("{\"ev\":\"key\",\"id\":\"%s\",\"act\":\"%s\"}\r\n", name, action);
}

void Protocol_Report_Gesture(uint8_t gesture)
{
    USART_DMA_Printf("{\"ev\":\"gest\",\"val\":%d}\r\n", gesture);
}

void Protocol_Report_State(uint16_t warm, uint16_t cold)
{
    if (_CheckQoS())
    {
        USART_DMA_Printf("{\"ev\":\"state\",\"warm\":%d,\"cold\":%d}\r\n", warm, cold);
    }
}

void Protocol_Report_Env(int8_t temp, uint8_t humi, uint16_t lux)
{
    if (_CheckQoS())
    {
        USART_DMA_Printf("{\"ev\":\"env\",\"t\":%d,\"h\":%d,\"l\":%d}\r\n", temp, humi, lux);
    }
}

void Protocol_Report_Heartbeat(uint32_t uptime)
{
    if (_CheckQoS())
    {
        USART_DMA_Printf("{\"ev\":\"hb\",\"up\":%d}\r\n", uptime);
    }
}
