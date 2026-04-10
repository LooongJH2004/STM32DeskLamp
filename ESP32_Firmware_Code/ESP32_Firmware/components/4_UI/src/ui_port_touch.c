#include "ui_port.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI_PORT_TOUCH";

// --- 触摸引脚定义 ---
#define TOUCH_CLK   1
#define TOUCH_MOSI  2
#define TOUCH_MISO  47
#define TOUCH_CS    38
#define TOUCH_IRQ   46

// --- 触摸屏校准参数 (根据单元测试得出) ---
#define TOUCH_X_MIN 110
#define TOUCH_X_MAX 1820
#define TOUCH_Y_MIN 140
#define TOUCH_Y_MAX 1875

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
    spi_device_polling_transmit(spi_handle, &t);
    return (rx_data[1] << 4) | (rx_data[2] >> 4);
}

// ============================================================
// 2. 软件中值滤波 (消除按下/松手时的跳动噪点)
// ============================================================
static uint16_t xpt2046_read_filtered(uint8_t cmd) {
    uint16_t buf[5];
    // 连续采样 5 次
    for(int i = 0; i < 5; i++) {
        buf[i] = xpt2046_read_adc(cmd);
    }
    // 冒泡排序
    for(int i = 0; i < 4; i++) {
        for(int j = i + 1; j < 5; j++) {
            if(buf[i] > buf[j]) {
                uint16_t temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }
    // 返回中间值，完美剔除突变噪点
    return buf[2];
}

// ============================================================
// 3. LVGL 触摸读取回调函数
// ============================================================
static void touch_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data) {
    // 1. 检查 IRQ 引脚。高电平表示没有触摸
    if (gpio_get_level(TOUCH_IRQ) == 1) {
        data->state = LV_INDEV_STATE_REL; // 释放状态
        return;
    }

    // 2. 读取滤波后的 ADC 值
    uint16_t raw_x = xpt2046_read_filtered(0xD0);
    uint16_t raw_y = xpt2046_read_filtered(0x90);

    // 3. 精准坐标映射 (基于单元测试数据)
    // 物理 X 轴 (0-319) 对应 Raw Y (110 -> 1820)
    int32_t x = (raw_y - TOUCH_X_MIN) * 320 / (TOUCH_X_MAX - TOUCH_X_MIN);
    
    // 物理 Y 轴 (0-239) 对应 Raw X (1875 -> 140) -> 注意这里是反向减法！
    int32_t y = (TOUCH_Y_MAX - raw_x) * 240 / (TOUCH_Y_MAX - TOUCH_Y_MIN);

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
// 4. 触摸硬件与 LVGL 注册初始化
// ============================================================
void UI_Port_Touch_Init(void) {
    ESP_LOGI(TAG, "Initialize SPI bus for Touch");

    gpio_reset_pin(TOUCH_IRQ);
    gpio_set_direction(TOUCH_IRQ, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TOUCH_IRQ, GPIO_PULLUP_ONLY);

    spi_bus_config_t buscfg = {
        .miso_io_num = TOUCH_MISO,
        .mosi_io_num = TOUCH_MOSI,
        .sclk_io_num = TOUCH_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000, // 2MHz
        .mode = 0,
        .spics_io_num = TOUCH_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "UI Touch Port Initialized.");
}
