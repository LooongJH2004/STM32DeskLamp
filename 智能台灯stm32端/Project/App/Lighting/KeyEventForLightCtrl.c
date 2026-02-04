///**
//  * @file    KeyEventForLightCtrl.c
//  * @brief   按键事件映射
//  */
//#include "Key.h"
//#include "LightCtrl.h"

//void Key_Hook_OnSingleClick(void)
//{
//    // 单击切换焦点
//    LightCtrl_ToggleFocus();
//}

//void Key_Hook_OnDoubleClick(void)
//{
//    // 双击：一键回中 (50% 亮度, 50% 色温)
//    LightCtrl_SetBrightness(500, EV_SOURCE_KEY);
//    LightCtrl_SetColorTemp(500, EV_SOURCE_KEY);
//}

//void Key_Hook_OnLongPressStart(void)
//{
//    // 长按开始：锁定系统，进入临时调色温模式
//    LightCtrl_SetLongPressMode(1);
//}

//void Key_Hook_OnLongPressRelease(void)
//{
//    // 长按释放：解锁，并重置焦点为亮度
//    LightCtrl_SetLongPressMode(0);
//    LightCtrl_ResetFocusToBrightness();
//}
