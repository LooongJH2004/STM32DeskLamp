#include "ui_port.h"
#include "esp_lcd_panel_io.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UI_PORT_DISP";

// --- 引脚定义 ---
#define LCD_D0      4
#define LCD_D1      5
#define LCD_D2      6
#define LCD_D3      7
#define LCD_D4      8
#define LCD_D5      9
#define LCD_D6      10
#define LCD_D7      11
#define LCD_WR      12
#define LCD_CS      13
#define LCD_DC      14
#define LCD_RST     15
#define LCD_BL      16

// 2.8寸 ILI9341 标准分辨率
#define LCD_H_RES   320
#define LCD_V_RES   240
#define DISP_BUF_SIZE (LCD_H_RES * LCD_V_RES / 5)

static esp_lcd_panel_io_handle_t io_handle = NULL; 

static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
static lv_disp_drv_t disp_drv;

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t data_bytes; 
} lcd_cmd_t;

static const lcd_cmd_t ili9341_init_cmds[] = {
    {0xCF, {0x00, 0xC1, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x00, 0x78}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x23}, 1},       
    {0xC1, {0x10}, 1},       
    {0xC5, {0x3E, 0x28}, 2}, 
    {0xC7, {0x86}, 1},       
    {0x36, {0x28}, 1},       // 横屏方向
    {0x3A, {0x55}, 1},       // 16-bit RGB565
    {0xB1, {0x00, 0x18}, 2}, 
    {0xB6, {0x08, 0x82, 0x27}, 3}, 
    {0xF2, {0x00}, 1},       
    {0x26, {0x01}, 1},       
    {0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15}, 
    {0xE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15}, 
    {0x11, {0}, 0xFF},       
    {0x29, {0}, 0}           
};

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false; 
}

// ============================================================
// 【核心修复】纯净版 LVGL 刷新函数
// ============================================================
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // 【修复1】必须使用 static，防止函数退出后局部变量被销毁，导致 DMA 读取到乱码坐标！
    static uint8_t caset_data[4];
    static uint8_t paset_data[4];

    caset_data[0] = (offsetx1 >> 8) & 0xFF;
    caset_data[1] = offsetx1 & 0xFF;
    caset_data[2] = (offsetx2 >> 8) & 0xFF;
    caset_data[3] = offsetx2 & 0xFF;
    esp_lcd_panel_io_tx_param(io_handle, 0x2A, caset_data, 4);

    paset_data[0] = (offsety1 >> 8) & 0xFF;
    paset_data[1] = offsety1 & 0xFF;
    paset_data[2] = (offsety2 >> 8) & 0xFF;
    paset_data[3] = offsety2 & 0xFF;
    esp_lcd_panel_io_tx_param(io_handle, 0x2B, paset_data, 4);

    size_t len = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1) * 2;
    esp_lcd_panel_io_tx_color(io_handle, 0x2C, color_map, len);
}

void UI_Port_Disp_Init(void) {
    ESP_LOGI(TAG, "Initialize Intel 8080 bus");

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_DC,
        .wr_gpio_num = LCD_WR,
        .data_gpio_nums = {
            LCD_D0, LCD_D1, LCD_D2, LCD_D3,
            LCD_D4, LCD_D5, LCD_D6, LCD_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = DISP_BUF_SIZE * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_CS,
        .pclk_hz = 10 * 1000 * 1000, // 恢复到 10MHz，保证流畅度
        
        // 【核心修复】：将队列深度从 10 增加到 50！
        // 防止 LVGL 高频局部刷新时，坐标指令(0x2A/0x2B)因队列溢出被丢弃
        .trans_queue_depth = 50, 
        
        .dc_levels = {
            .dc_idle_level = 0, .dc_cmd_level = 0,
            .dc_dummy_level = 0, .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = 1, 
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, &disp_drv));

    ESP_LOGI(TAG, "Manual Hardware Reset...");
    gpio_reset_pin(LCD_RST);
    gpio_set_direction(LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_LOGI(TAG, "Send ILI9341 Init Sequence...");
    for (int i = 0; i < sizeof(ili9341_init_cmds)/sizeof(lcd_cmd_t); i++) {
        if (ili9341_init_cmds[i].data_bytes == 0xFF) {
            esp_lcd_panel_io_tx_param(io_handle, ili9341_init_cmds[i].cmd, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(120)); 
        } else {
            esp_lcd_panel_io_tx_param(io_handle, ili9341_init_cmds[i].cmd, ili9341_init_cmds[i].data, ili9341_init_cmds[i].data_bytes);
        }
    }

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_direction(LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL, 1);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1 && buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    
    // 【修复3】关闭全屏刷新！恢复 LVGL 的局部脏更新机制
    disp_drv.full_refresh = 0; 
    
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb; 
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "UI Display Port Initialized.");
}
