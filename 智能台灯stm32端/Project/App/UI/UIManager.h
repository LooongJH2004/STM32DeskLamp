/**
  ******************************************************************************
  * @file    UIManager.h
  * @brief   UI 管理器头文件
  * @note    负责 OLED 的初始化、页面调度和脏检测刷新。
  ******************************************************************************
  */
#ifndef __UI_MANAGER_H
#define __UI_MANAGER_H

/**
  * @brief  UI 系统初始化
  * @note   初始化 OLED 硬件，并绘制初始界面框架
  */
void UIManager_Init(void);

/**
  * @brief  UI 刷新任务
  * @note   建议在主循环中以较低频率调用 (如 100ms/次)
  *         函数内部会自动进行脏检测，仅刷新变化的数据。
  */
void UIManager_Task(void);

#endif
