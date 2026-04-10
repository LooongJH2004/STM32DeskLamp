#include "ui_port.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI_PORT_TOUCH";

// --- 触摸引脚定义 (来自 pin_config.txt) ---
#define TOUCH_CLK   1
#define TOUCH_MOSI  2
#define TOUCH_MISO  47
#define TOUCH_CS    38
#define TOUCH_IRQ   46

static spi_device_handle_t spi_handle;

// ============================================================
// 1. XPT2046 底层 SPI 读取
// ============================================================
static uint16_t xpt2046_read_adc(uint8_t cmd) {
    uint8_t tx_data[3] = {cmd, 0x00, 0x00};
    uint8_t rx_data[3] = {0};
    
    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };
    
    // 轮询方式发送和接收 SPI 数据
    spi_device_polling_transmit(spi_handle, &t);
    
    // XPT2046 返回的是 12 位 ADC 数据，分布在 rx_data 的后两个字节
    return (rx_data[1] << 4) | (rx_data[2] >> 4);
}

// ============================================================
// 2. LVGL 触摸读取回调函数 (已修复横屏坐标映射)
// ============================================================
static void touch_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data) {
    // 1. 检查 IRQ 引脚。高电平表示没有触摸
    if (gpio_get_level(TOUCH_IRQ) == 1) {
        data->state = LV_INDEV_STATE_REL; // 释放状态
        return;
    }

    // 2. 读取原始 ADC 值
    uint16_t raw_x = xpt2046_read_adc(0xD0);
    uint16_t raw_y = xpt2046_read_adc(0x90);

    // 3. 坐标映射与 X/Y 轴对调 (适配横屏)
    int16_t x = (raw_y - 300) * 320 / (3800 - 300);
    int16_t y = (raw_x - 300) * 240 / (3800 - 300);

    // 【新增这一行】：反转 X 轴方向
    x = 319 - x; 

    // 4. 边界限制保护
    if (x < 0) x = 0;
    if (x > 319) x = 319;
    if (y < 0) y = 0;
    if (y > 239) y = 239;

    // 5. 传给 LVGL
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PR; // 按下状态
}

// ============================================================
// 3. 触摸硬件与 LVGL 注册初始化
// ============================================================
void UI_Port_Touch_Init(void) {
    ESP_LOGI(TAG, "Initialize SPI bus for Touch");

    // 1. 初始化 IRQ 引脚 (输入，上拉)
    gpio_reset_pin(TOUCH_IRQ);
    gpio_set_direction(TOUCH_IRQ, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TOUCH_IRQ, GPIO_PULLUP_ONLY);

    // 2. 配置 SPI 总线 (使用 SPI2_HOST)
    spi_bus_config_t buscfg = {
        .miso_io_num = TOUCH_MISO,
        .mosi_io_num = TOUCH_MOSI,
        .sclk_io_num = TOUCH_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. 配置 SPI 设备 (XPT2046)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000, // 2MHz 时钟
        .mode = 0,                         // SPI mode 0
        .spics_io_num = TOUCH_CS,          // CS 引脚
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));

    // 4. 注册输入设备到 LVGL
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER; // 触摸屏属于指针设备
    indev_drv.read_cb = touch_read_cb;      // 绑定读取回调
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "UI Touch Port Initialized.");
}
