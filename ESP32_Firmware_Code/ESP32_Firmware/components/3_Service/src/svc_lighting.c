#include "svc_lighting.h"
#include "data_center.h"
#include "dev_stm32.h"
#include "esp_log.h"

static const char *TAG = "Svc_Light";

void Svc_Lighting_Apply(void) {
    DC_LightingData_t light;
    DataCenter_Get_Lighting(&light);

    uint16_t warm_pwm = 0;
    uint16_t cold_pwm = 0;

    if (light.power) {
        // 算法：总 PWM 范围 0-1000
        uint16_t total_pwm = light.brightness * 10; 
        
        // 色温 0=最暖(全warm), 100=最冷(全cold)
        cold_pwm = (total_pwm * light.color_temp) / 100;
        warm_pwm = total_pwm - cold_pwm;
    }

    ESP_LOGI(TAG, "Apply Light -> Power:%d, Bri:%d, CCT:%d => Warm:%d, Cold:%d", 
             light.power, light.brightness, light.color_temp, warm_pwm, cold_pwm);

    // 下发给 STM32
    Dev_STM32_Set_Light(warm_pwm, cold_pwm);
}
