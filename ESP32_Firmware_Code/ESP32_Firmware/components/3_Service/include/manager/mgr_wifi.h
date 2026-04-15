#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Wi-Fi 连接状态枚举
typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_ERROR
} Wifi_Status_t;

/**
 * @brief Wi-Fi 配置信息结构体
 */
typedef struct {
    char ssid[32];      /*!< Wi-Fi 名称 */
    char password[64];  /*!< Wi-Fi 密码 */
    bool is_ready;      /*!< 标记 SSID 和密码是否均已配置 */
} wifi_information;

/**
 * @brief 初始化 Wi-Fi 信息结构体 (清空缓存)
 */
void wifi_information_init(void);

/**
 * @brief 获取当前配置的 Wi-Fi 名称
 * @return const char* 指向 SSID 字符串的指针
 */
const char* get_wifi_name(void);

/**
 * @brief 获取当前配置的 Wi-Fi 密码
 * @return const char* 指向 Password 字符串的指针
 */
const char* get_wifi_password(void);

/**
 * @brief 设置 Wi-Fi 名称 (供串口解析调用)
 * @param ssid Wi-Fi 名称字符串
 */
void set_wifi_name(const char *ssid);

/**
 * @brief 设置 Wi-Fi 密码 (供串口解析调用)
 * @param password Wi-Fi 密码字符串
 */
void set_wifi_password(const char *password);

/**
 * @brief 检查 Wi-Fi 信息是否已准备就绪
 * @return true 已准备好, false 未准备好
 */
bool is_wifi_info_ready(void);

/**
 * @brief 初始化 Wi-Fi 和 SNTP 服务
 */
void Mgr_Wifi_Init(void);

/**
 * @brief 连接指定热点
 * @param ssid Wi-Fi 名称
 * @param password Wi-Fi 密码
 */
void Mgr_Wifi_Connect(const char *ssid, const char *password);

/**
 * @brief 获取当前连接状态
 */
Wifi_Status_t Mgr_Wifi_GetStatus(void);

/**
 * @brief 检查时间是否已同步 (HTTPS 请求前必须检查)
 */
bool Mgr_Wifi_IsTimeSynced(void);
