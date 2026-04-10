#include "ui_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_ili9341.h"

static const char *TAG = "UI_PORT_DISP";

#define LCD_D0 4
#define LCD_D1 5
#define LCD_D2 6
#define LCD_D3 7
#define LCD_D4 8
#define LCD_D5 9
#define LCD_D6 10
#define LCD_D7 11
#define LCD_WR 12
#define LCD_CS 13
#define LCD_DC 14
#define LCD_RST 15
#define LCD_BL 16

#define LCD_H_RES 320
#define LCD_V_RES 240

// 单缓冲，1/4 屏幕大小
#define DISP_BUF_SIZE (LCD_H_RES * LCD_V_RES / 4)

static esp_lcd_panel_io_handle_t io_handle = NULL; 
static esp_lcd_panel_handle_t panel_handle = NULL;

static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf1 = NULL; 
static lv_disp_drv_t disp_drv;

// 全局隔离标志位
volatile bool g_lcd_is_flushing = false;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    g_lcd_is_flushing = false; // 释放总线
    lv_disp_flush_ready(disp_driver);
    return false; 
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t) drv->user_data;
    g_lcd_is_flushing = true;  // 锁定总线
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
}

void UI_Port_Disp_Init(void) {
    ESP_LOGI(TAG, "Initialize Intel 8080 bus");

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_DC,
        .wr_gpio_num = LCD_WR,
        .data_gpio_nums = {LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_D4, LCD_D5, LCD_D6, LCD_D7},
        .bus_width = 8,
        .max_transfer_bytes = DISP_BUF_SIZE * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_CS,
        .pclk_hz = 5 * 1000 * 1000, // 5MHz 安全频率
        .trans_queue_depth = 1,     // 队列深度 1，防止积压
        .dc_levels = {
            .dc_idle_level = 0, .dc_cmd_level = 0,
            .dc_dummy_level = 0, .dc_data_level = 1,
        },
        .flags = { .swap_color_bytes = 1 },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, &disp_drv));

    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR, 
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false)); 
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    gpio_set_direction(LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL, 1);

    lv_init();

    buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1);

    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, DISP_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.full_refresh = 0; 
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb; 
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle; 
    lv_disp_drv_register(&disp_drv);
}
