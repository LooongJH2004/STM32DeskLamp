#include "LDR.h"

void LDR_Init(void)
{
    // 1. 开启 GPIOA 和 ADC1 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    
    // 2. 配置 ADC 时钟分频 (72MHz / 6 = 12MHz, 不超过 14MHz)
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    // 3. 配置 PA0 为模拟输入
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. ADC 初始化
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 5. 使能 ADC 并校准
    ADC_Cmd(ADC1, ENABLE);
    
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

uint16_t LDR_GetRawValue(void)
{
    // 设置序列、通道和采样时间
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    
    // 软件触发转换
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    
    // 等待转换完成
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    
    return ADC_GetConversionValue(ADC1);
}

uint16_t LDR_GetLuxPercentage(void)
{
    uint32_t sum = 0;
    // 连续采样 8 次取平均值，滤除高频噪声
    for(int i = 0; i < 8; i++) {
        sum += LDR_GetRawValue();
    }
    uint16_t avg = sum / 8;
    
    // 将 0-4095 映射到 0-1000
    // 注意：如果你的电路是光强越大电压越高，直接映射即可
    return (uint16_t)((avg * 1000) / 4095);
}
