#pragma once

/**
 * @brief 初始化 NVS 存储模块 (创建防抖定时器)
 */
void Storage_NVS_Init(void);

/**
 * @brief 从 Flash 加载所有持久化数据到 DataCenter (仅在上电时调用一次)
 */
void Storage_NVS_Load_All(void);

/**
 * @brief 请求保存数据到 Flash (非阻塞，触发 3 秒防抖)
 */
void Storage_NVS_RequestSave(void);
