#include "ui_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UI_PORT_DISP";

// --- 引脚定义 (来自 pin_config.txt) ---
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

// --- 屏幕参数 ---
#define LCD_H_RES   480
#define LCD_V_RES   320
// LVGL 渲染缓冲区大小 (建议为屏幕总像素的 1/10 到 1/5)
#define DISP_BUF_SIZE (LCD_H_RES * LCD_V_RES / 10)

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL; // 双缓冲，配合 DMA 极致流畅

// ============================================================
// 1. LVGL 刷新回调函数 (将渲染好的图像推送到屏幕)
// ============================================================
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // 调用 ESP-IDF 的 LCD 驱动进行局部刷新 (底层会自动使用 DMA)
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    
    // 告诉 LVGL 刷新已完成 (非常重要，否则 LVGL 会卡死)
    lv_disp_flush_ready(drv);
}

// ============================================================
// 2. 屏幕硬件初始化 (8080 并口)
// ============================================================
void UI_Port_Disp_Init(void) {
    ESP_LOGI(TAG, "Initialize Intel 8080 bus");

    // 1. 配置 8080 总线
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

    // 2. 配置 LCD 面板 IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_CS,
        .pclk_hz = 20 * 1000 * 1000, // 20MHz 像素时钟 (可根据屏幕体质调高到 40MHz)
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = 1, // 修复颜色反转 (红蓝对调)
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    // 3. 初始化 ILI9486 面板驱动 (ESP-IDF 默认可能没有 9486，通常用 st7789 或 nt35510 兼容，这里我们先用 st7789 尝试，如果花屏再换)
    ESP_LOGI(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    // 注意：ILI9486 的初始化序列与 ST7789 类似，如果显示异常，我们需要手写初始化命令
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // 设置屏幕方向 (横屏)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    
    // 开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 4. 点亮背光
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_direction(LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL, 1);

    // ============================================================
    // 3. LVGL 核心初始化
    // ============================================================
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    // 分配双缓冲区 (优先使用内部 RAM 保证 DMA 速度，如果不够再用 PSRAM)
    buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1 && buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    // 注册显示驱动到 LVGL
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "UI Display Port Initialized.");
}
