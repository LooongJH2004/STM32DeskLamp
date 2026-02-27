#pragma once

/**
 * @brief 执行灯光状态更新 (将 DataCenter 的数据转换为 PWM 并下发给 STM32)
 */
void Svc_Lighting_Apply(void);
