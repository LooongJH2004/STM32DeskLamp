/* App/Protocol/Protocol.c */
#include "Protocol.h"
#include "Protocol_CRC.h" // [新增] 引入 CRC 模块
#include "USART_DMA.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>       // [新增] 用于 strtol

// --- 应用层接收缓冲区 ---
#define APP_RX_BUF_SIZE 512
static char s_AppRxBuf[APP_RX_BUF_SIZE];
static uint16_t s_AppRxLen = 0;

// --- 回调函数 ---
static Proto_ModeCallback_t s_ModeCb = NULL;
static Proto_LightCallback_t s_LightCb = NULL;

// --- 内部辅助：检查 QoS 水位线 ---
static int _CheckQoS(void)
{
    if (USART_DMA_GetUsage() > PROTOCOL_QOS_THRESHOLD) return 0;
    return 1;
}

// ============================================================
// [新增] 内部辅助：带 CRC16 的底层发送函数
// ============================================================
static void _Send_With_CRC(const char* json_str)
{
    // 1. 计算纯 JSON 的 CRC16
    uint16_t crc = CRC16_Calculate((const uint8_t *)json_str, strlen(json_str));
    
    // 2. 拼接格式: [JSON]|XXXX\r\n
    char out_buf[256];
    sprintf(out_buf, "%s|%04X\r\n", json_str, crc);
    
    // 3. 调用 DMA 发送
    USART_DMA_Send((uint8_t*)out_buf, strlen(out_buf));
}

// --- 内部辅助：解析 JSON 指令 ---
static void _ParseJsonCmd(char* json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root)
    {
        cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
        if (cJSON_IsString(cmd))
        {
            // 1. 模式切换指令
            if (strcmp(cmd->valuestring, "mode") == 0)
            {
                cJSON *val = cJSON_GetObjectItem(root, "val");
                if (cJSON_IsNumber(val) && s_ModeCb)
                {
                    s_ModeCb((uint8_t)val->valueint);
                }
            }
            // 2. 灯光控制指令
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
        USART_DMA_Printf("[Proto] JSON Parse Error: %s\r\n", json_str);
    }
}

void Protocol_Init(void)
{
    s_AppRxLen = 0;
    memset(s_AppRxBuf, 0, APP_RX_BUF_SIZE);
}

void Protocol_Process(void)
{
    // 1. 从 DMA 驱动层拉取新数据
    uint8_t temp_buf[128];
    uint16_t len = USART_DMA_ReadRxBuffer(temp_buf, sizeof(temp_buf));

    if (len > 0)
    {
        if (s_AppRxLen + len < APP_RX_BUF_SIZE)
        {
            memcpy(&s_AppRxBuf[s_AppRxLen], temp_buf, len);
            s_AppRxLen += len;
            s_AppRxBuf[s_AppRxLen] = '\0'; 
        }
        else
        {
            s_AppRxLen = 0;
            USART_DMA_Printf("[Proto] Buffer Overflow! Reset.\r\n");
        }
    }

    // 2. 检查是否包含完整的数据帧 (以 \n 结尾)
    if (s_AppRxLen > 0)
    {
        char* newline_ptr = strchr(s_AppRxBuf, '\n');
        
        while (newline_ptr != NULL)
        {
            int frame_len = (newline_ptr - s_AppRxBuf) + 1;
            *newline_ptr = '\0';
            
            if (frame_len > 1 && s_AppRxBuf[frame_len - 2] == '\r')
            {
                s_AppRxBuf[frame_len - 2] = '\0';
            }

            // ============================================================
            // [新增] CRC 校验拦截逻辑
            // ============================================================
            if (strlen(s_AppRxBuf) > 0)
            {
                char *sep = strrchr(s_AppRxBuf, '|'); // 找分隔符
                
                if (sep != NULL)
                {
                    // 1. 计算本地 CRC
                    size_t json_len = sep - s_AppRxBuf;
                    uint16_t calc_crc = CRC16_Calculate((const uint8_t *)s_AppRxBuf, json_len);
                    
                    // 2. 提取接收到的 CRC
                    uint16_t recv_crc = (uint16_t)strtol(sep + 1, NULL, 16);
                    
                    // 3. 比对
                    if (calc_crc == recv_crc)
                    {
                        *sep = '\0'; // 校验通过，截断字符串，只保留纯 JSON
                        _ParseJsonCmd(s_AppRxBuf);
                    }
                    else
                    {
                        // 校验失败，静默丢弃 (仅打印 Log)
                        USART_DMA_Printf("[Proto] CRC Error! Calc:%04X Recv:%04X\r\n", calc_crc, recv_crc);
                    }
                }
                else
                {
                    // 找不到分隔符，格式错误，静默丢弃
                    USART_DMA_Printf("[Proto] Missing CRC separator. Drop.\r\n");
                }
            }

            // 3. 移除已处理的数据 (滑动窗口)
            int remaining = s_AppRxLen - frame_len;
            if (remaining > 0)
            {
                memmove(s_AppRxBuf, &s_AppRxBuf[frame_len], remaining);
                s_AppRxLen = remaining;
                s_AppRxBuf[s_AppRxLen] = '\0';
                newline_ptr = strchr(s_AppRxBuf, '\n');
            }
            else
            {
                s_AppRxLen = 0;
                newline_ptr = NULL;
            }
        }
    }
}

void Protocol_SetModeCallback(Proto_ModeCallback_t cb) { s_ModeCb = cb; }
void Protocol_SetLightCallback(Proto_LightCallback_t cb) { s_LightCb = cb; }

/* ============================================================
 * 发送接口实现 (重构：先组装 JSON，再调用 _Send_With_CRC)
 * ============================================================ */

void Protocol_Report_Encoder(int16_t diff)
{
    char buf[64];
    sprintf(buf, "{\"ev\":\"enc\",\"diff\":%d}", diff);
    _Send_With_CRC(buf);
}

void Protocol_Report_Key(const char* name, const char* action)
{
    char buf[64];
    sprintf(buf, "{\"ev\":\"key\",\"id\":\"%s\",\"act\":\"%s\"}", name, action);
    _Send_With_CRC(buf);
}

void Protocol_Report_Gesture(uint8_t gesture)
{
    char buf[64];
    sprintf(buf, "{\"ev\":\"gest\",\"val\":%d}", gesture);
    _Send_With_CRC(buf);
}

void Protocol_Report_State(uint16_t warm, uint16_t cold)
{
    if (_CheckQoS())
    {
        char buf[64];
        sprintf(buf, "{\"ev\":\"state\",\"warm\":%d,\"cold\":%d}", warm, cold);
        _Send_With_CRC(buf);
    }
}

void Protocol_Report_Env(int8_t temp, uint8_t humi, uint16_t lux)
{
    if (_CheckQoS())
    {
        char buf[64];
        sprintf(buf, "{\"ev\":\"env\",\"t\":%d,\"h\":%d,\"l\":%d}", temp, humi, lux);
        _Send_With_CRC(buf);
    }
}

void Protocol_Report_Heartbeat(uint32_t uptime)
{
    if (_CheckQoS())
    {
        char buf[64];
        sprintf(buf, "{\"ev\":\"hb\",\"up\":%d}", uptime);
        _Send_With_CRC(buf);
    }
}
