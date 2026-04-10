#include "ui_main.h"
#include "lvgl.h"
#include "data_center.h"
#include "esp_log.h"

static const char *TAG = "UI_MAIN";

static lv_obj_t * s_slider_bri;
static lv_obj_t * s_slider_cct;
static lv_obj_t * s_label_env;
static lv_obj_t * s_sw_power;
static lv_obj_t * s_btn_test; // 自动测试按钮

// ============================================================
// 1. UI 交互事件回调 (UI -> DataCenter)
// ============================================================

static uint32_t s_last_bri_tick = 0;
static void slider_bri_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int bri = lv_slider_get_value(slider);
    
    // 【跟手优化】软件限流：每 50ms 最多下发一次数据。
    // 这样滑块在屏幕上是 60FPS 实时滑动的，但底层通信被限制在 20Hz，杜绝事件风暴。
    if (lv_tick_elaps(s_last_bri_tick) > 50) {
        DC_LightingData_t light;
        DataCenter_Get_Lighting(&light);
        light.brightness = bri;
        if (bri > 0) light.power = true; 
        DataCenter_Set_Lighting(&light);
        s_last_bri_tick = lv_tick_get();
    }
}

static uint32_t s_last_cct_tick = 0;
static void slider_cct_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int cct = lv_slider_get_value(slider);
    
    if (lv_tick_elaps(s_last_cct_tick) > 50) {
        DC_LightingData_t light;
        DataCenter_Get_Lighting(&light);
        light.color_temp = cct;
        DataCenter_Set_Lighting(&light);
        s_last_cct_tick = lv_tick_get();
    }
}

static void sw_power_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);
    light.power = is_on;
    DataCenter_Set_Lighting(&light);
}

// ============================================================
// 自动测试动画 (用于隔离排查硬件撕裂)
// ============================================================
static void auto_anim_cb(void * var, int32_t v) {
    lv_slider_set_value((lv_obj_t *)var, v, LV_ANIM_ON);
}

static void btn_test_event_cb(lv_event_t * e) {
    static bool is_auto = false;
    is_auto = !is_auto;
    if (is_auto) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_slider_bri);
        lv_anim_set_values(&a, 0, 100);
        lv_anim_set_time(&a, 1500);
        lv_anim_set_playback_time(&a, 1500);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, auto_anim_cb);
        lv_anim_start(&a);
        lv_obj_set_style_bg_color(s_btn_test, lv_palette_main(LV_PALETTE_RED), 0);
        ESP_LOGI(TAG, "Auto Test Started");
    } else {
        lv_anim_del(s_slider_bri, auto_anim_cb);
        lv_obj_set_style_bg_color(s_btn_test, lv_palette_main(LV_PALETTE_BLUE), 0);
        ESP_LOGI(TAG, "Auto Test Stopped");
    }
}

// ============================================================
// 2. 数据同步定时器 (DataCenter -> UI)
// ============================================================
static void ui_sync_timer_cb(lv_timer_t * timer) {
    DC_EnvData_t env;
    DataCenter_Get_Env(&env);
    lv_label_set_text_fmt(s_label_env, "Temp: %d C   Hum: %d %%   Lux: %d", 
                          env.indoor_temp, env.indoor_hum, env.indoor_lux);

    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);
    
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
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1E1E1E), LV_PART_MAIN);

    lv_obj_t * label_title = lv_label_create(lv_scr_act());
    lv_label_set_text(label_title, "Smart Lamp Control");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 20, 20);

    // 【新增】自动测试按钮
    s_btn_test = lv_btn_create(lv_scr_act());
    lv_obj_align(s_btn_test, LV_ALIGN_TOP_RIGHT, -80, 10);
    lv_obj_set_style_bg_color(s_btn_test, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_event_cb(s_btn_test, btn_test_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * label_btn = lv_label_create(s_btn_test);
    lv_label_set_text(label_btn, "Auto Test");

    s_sw_power = lv_switch_create(lv_scr_act());
    lv_obj_align(s_sw_power, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(s_sw_power, sw_power_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * label_bri = lv_label_create(lv_scr_act());
    lv_label_set_text(label_bri, "Brightness");
    lv_obj_set_style_text_color(label_bri, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(label_bri, LV_ALIGN_TOP_LEFT, 20, 70);

    s_slider_bri = lv_slider_create(lv_scr_act());
    lv_obj_set_size(s_slider_bri, 280, 20);
    lv_obj_align(s_slider_bri, LV_ALIGN_TOP_MID, 0, 100);
    lv_slider_set_range(s_slider_bri, 0, 100);
    lv_obj_set_style_bg_color(s_slider_bri, lv_palette_main(LV_PALETTE_AMBER), LV_PART_INDICATOR);
    // 【恢复】改回 VALUE_CHANGED，配合上面的 50ms 限流器
    lv_obj_add_event_cb(s_slider_bri, slider_bri_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * label_cct = lv_label_create(lv_scr_act());
    lv_label_set_text(label_cct, "Color Temp (Warm -> Cold)");
    lv_obj_set_style_text_color(label_cct, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(label_cct, LV_ALIGN_TOP_LEFT, 20, 140);

    s_slider_cct = lv_slider_create(lv_scr_act());
    lv_obj_set_size(s_slider_cct, 280, 20);
    lv_obj_align(s_slider_cct, LV_ALIGN_TOP_MID, 0, 170);
    lv_slider_set_range(s_slider_cct, 0, 100);
    lv_obj_set_style_bg_color(s_slider_cct, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    // 【恢复】改回 VALUE_CHANGED
    lv_obj_add_event_cb(s_slider_cct, slider_cct_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_label_env = lv_label_create(lv_scr_act());
    lv_label_set_text(s_label_env, "Temp: -- C   Hum: -- %   Lux: --");
    lv_obj_set_style_text_color(s_label_env, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align(s_label_env, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_timer_create(ui_sync_timer_cb, 500, NULL);

    ESP_LOGI(TAG, "Main UI Initialized");
}
