#include "ui_main.h"
#include "lvgl.h"
#include "data_center.h"
#include "esp_log.h"

static const char *TAG = "UI_MAIN";

// 控件全局指针，方便定时器更新它们
static lv_obj_t * s_slider_bri;
static lv_obj_t * s_slider_cct;
static lv_obj_t * s_label_env;
static lv_obj_t * s_sw_power;

// ============================================================
// 1. UI 交互事件回调 (UI -> DataCenter)
// ============================================================

// 亮度滑块拖动事件
static void slider_bri_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int bri = lv_slider_get_value(slider);
    
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);
    light.brightness = bri;
    if (bri > 0) light.power = true; // 拉动滑块自动开灯
    DataCenter_Set_Lighting(&light);
}

// 色温滑块拖动事件
static void slider_cct_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int cct = lv_slider_get_value(slider);
    
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);
    light.color_temp = cct;
    DataCenter_Set_Lighting(&light);
}

// 开关点击事件
static void sw_power_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);
    light.power = is_on;
    DataCenter_Set_Lighting(&light);
}

// ============================================================
// 2. 数据同步定时器 (DataCenter -> UI)
// ============================================================
static void ui_sync_timer_cb(lv_timer_t * timer) {
    // 1. 获取最新环境数据并更新文本
    DC_EnvData_t env;
    DataCenter_Get_Env(&env);
    lv_label_set_text_fmt(s_label_env, "Temp: %d C   Hum: %d %%   Lux: %d", 
                          env.indoor_temp, env.indoor_hum, env.indoor_lux);

    // 2. 获取最新灯光数据并更新控件 (防止与其他控制端冲突)
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);
    
    // 如果用户正在拖动滑块，就不更新滑块位置，防止手抖
    if (!lv_obj_has_state(s_slider_bri, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_slider_bri, light.brightness, LV_ANIM_ON);
    }
    if (!lv_obj_has_state(s_slider_cct, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_slider_cct, light.color_temp, LV_ANIM_ON);
    }
    
    if (light.power) {
        lv_obj_add_state(s_sw_power, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_sw_power, LV_STATE_CHECKED);
    }
}

// ============================================================
// 3. 界面构建
// ============================================================
void UI_Main_Init(void) {
    // 设置深色背景
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1E1E1E), LV_PART_MAIN);

    // --- 1. 顶部标题与开关 ---
    lv_obj_t * label_title = lv_label_create(lv_scr_act());
    lv_label_set_text(label_title, "Smart Lamp Control");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 20, 20);

    s_sw_power = lv_switch_create(lv_scr_act());
    lv_obj_align(s_sw_power, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(s_sw_power, sw_power_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- 2. 亮度控制 (滑块) ---
    lv_obj_t * label_bri = lv_label_create(lv_scr_act());
    lv_label_set_text(label_bri, "Brightness");
    lv_obj_set_style_text_color(label_bri, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(label_bri, LV_ALIGN_TOP_LEFT, 20, 70);

    s_slider_bri = lv_slider_create(lv_scr_act());
    lv_obj_set_size(s_slider_bri, 280, 20);
    lv_obj_align(s_slider_bri, LV_ALIGN_TOP_MID, 0, 100);
    lv_slider_set_range(s_slider_bri, 0, 100);
    lv_obj_set_style_bg_color(s_slider_bri, lv_palette_main(LV_PALETTE_AMBER), LV_PART_INDICATOR);
    lv_obj_add_event_cb(s_slider_bri, slider_bri_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- 3. 色温控制 (滑块) ---
    lv_obj_t * label_cct = lv_label_create(lv_scr_act());
    lv_label_set_text(label_cct, "Color Temp (Warm -> Cold)");
    lv_obj_set_style_text_color(label_cct, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(label_cct, LV_ALIGN_TOP_LEFT, 20, 140);

    s_slider_cct = lv_slider_create(lv_scr_act());
    lv_obj_set_size(s_slider_cct, 280, 20);
    lv_obj_align(s_slider_cct, LV_ALIGN_TOP_MID, 0, 170);
    lv_slider_set_range(s_slider_cct, 0, 100);
    lv_obj_set_style_bg_color(s_slider_cct, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_add_event_cb(s_slider_cct, slider_cct_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- 4. 底部环境数据展示 ---
    s_label_env = lv_label_create(lv_scr_act());
    lv_label_set_text(s_label_env, "Temp: -- C   Hum: -- %   Lux: --");
    lv_obj_set_style_text_color(s_label_env, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align(s_label_env, LV_ALIGN_BOTTOM_MID, 0, -20);

    // --- 5. 创建 LVGL 定时器，每 500ms 同步一次数据 ---
    lv_timer_create(ui_sync_timer_cb, 500, NULL);

    ESP_LOGI(TAG, "Main UI Initialized");
}
