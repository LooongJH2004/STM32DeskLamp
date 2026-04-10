#include "ui_port.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI_PORT_TOUCH";

#define TOUCH_CLK   1
#define TOUCH_MOSI  2
#define TOUCH_MISO  47
#define TOUCH_CS    38
#define TOUCH_IRQ   46

#define TOUCH_X_MIN 110
#define TOUCH_X_MAX 1820
#define TOUCH_Y_MIN 140
#define TOUCH_Y_MAX 1875

static spi_device_handle_t spi_handle;
extern volatile bool g_lcd_is_flushing;

static uint16_t xpt2046_read_adc(uint8_t cmd) {
    uint8_t tx_data[3] = {cmd, 0x00, 0x00};
    uint8_t rx_data[3] = {0};
    spi_transaction_t t = { .length = 24, .tx_buffer = tx_data, .rx_buffer = rx_data };
    spi_device_polling_transmit(spi_handle, &t);
    return (rx_data[1] << 4) | (rx_data[2] >> 4);
}

static uint16_t xpt2046_read_filtered(uint8_t cmd) {
    uint16_t buf[5];
    for(int i = 0; i < 5; i++) buf[i] = xpt2046_read_adc(cmd);
    for(int i = 0; i < 4; i++) {
        for(int j = i + 1; j < 5; j++) {
            if(buf[i] > buf[j]) {
                uint16_t temp = buf[i]; buf[i] = buf[j]; buf[j] = temp;
            }
        }
    }
    return buf[2];
}

static void touch_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data) {
    // 【智能隔离】如果 LCD 正在刷屏，直接 return。
    // 注意：这里不修改 data 的内容，LVGL 会自动沿用上一次的坐标和按下状态，
    // 这样既避免了总线打架，又保证了拖动滑块时不会“断触”。
    if (g_lcd_is_flushing) {
        return; 
    }

    if (gpio_get_level(TOUCH_IRQ) == 1) {
        data->state = LV_INDEV_STATE_REL; 
        return;
    }

    uint16_t raw_x = xpt2046_read_filtered(0xD0);
    uint16_t raw_y = xpt2046_read_filtered(0x90);

    int32_t x = (raw_y - TOUCH_X_MIN) * 320 / (TOUCH_X_MAX - TOUCH_X_MIN);
    int32_t y = (TOUCH_Y_MAX - raw_x) * 240 / (TOUCH_Y_MAX - TOUCH_Y_MIN);

    if (x < 0) x = 0; 
    if (x > 319) x = 319;
    if (y < 0) y = 0; 
    if (y > 239) y = 239;

    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PR; 
}

void UI_Port_Touch_Init(void) {
    gpio_reset_pin(TOUCH_IRQ);
    gpio_set_direction(TOUCH_IRQ, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TOUCH_IRQ, GPIO_PULLUP_ONLY);

    spi_bus_config_t buscfg = {
        .miso_io_num = TOUCH_MISO, .mosi_io_num = TOUCH_MOSI, .sclk_io_num = TOUCH_CLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 32
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2 * 1000 * 1000, // 触摸 SPI 保持 2MHz
        .mode = 0, .spics_io_num = TOUCH_CS, .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}
