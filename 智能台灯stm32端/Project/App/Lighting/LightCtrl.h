#ifndef __LIGHT_CTRL_H
#define __LIGHT_CTRL_H

#include <stdint.h>

// --- 接口 ---
void LightCtrl_Init(void);

// 本地控制 (相对调整)
// 用于编码器旋转，调整的是 Brightness/ColorTemp 模型
void LightCtrl_AdjustBrightness(int16_t delta);
void LightCtrl_AdjustColorTemp(int16_t delta);

// 远程控制 (绝对设置)
// 直接设置 PWM 占空比 (0-1000)
void LightCtrl_SetRawPWM(uint16_t warm, uint16_t cold);

// 获取当前状态 (用于上报)
uint16_t LightCtrl_GetBrightness(void);
uint16_t LightCtrl_GetColorTemp(void);

// 周期性任务 (处理节流上报)
void LightCtrl_Task(void);

#endif
