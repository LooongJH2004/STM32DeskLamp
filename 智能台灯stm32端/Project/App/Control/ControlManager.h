/* App/Control/ControlManager.h */
#ifndef __CONTROL_MANAGER_H
#define __CONTROL_MANAGER_H

#include <stdint.h>

// --- 接口 ---
void Control_Init(void);
void Control_Task(void); // 周期性调用

// 事件入口
void Control_OnEncoder(int16_t diff);

/**
 * @brief 处理按键事件
 * @param key_name 按键标识符 (如 "ModeSW")
 * @param action   动作类型 ("click", "double", "hold", "release")
 */
void Control_OnKey(const char* key_name, const char* action);

void Control_OnGesture(uint8_t gesture);
void Control_OnProximity(uint8_t brightness);

// 状态查询
uint8_t Control_GetFocus(void); // 0:Bri, 1:CCT

#endif
