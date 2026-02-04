/**
  ******************************************************************************
  * @file    DHT11.c
  * @brief   DHT11 温湿度传感器驱动实现
  ******************************************************************************
  */
#include "DHT11.h"
#include "SystemSupport.h" // 需要 Delay_us

#define DHT11_IO_PORT    GPIOA
#define DHT11_IO_PIN     GPIO_Pin_1
#define DHT11_RCC        RCC_APB2Periph_GPIOA

/* 内部宏：控制 GPIO 输入输出 */
#define DHT11_IO_OUT()   {GPIOA->CRL &= 0xFFFFFF0F; GPIOA->CRL |= 0x00000030;} // PA1 推挽输出
#define DHT11_IO_IN()    {GPIOA->CRL &= 0xFFFFFF0F; GPIOA->CRL |= 0x00000080;} // PA1 上拉输入

/* 内部宏：读写电平 */
#define DHT11_DQ_OUT(x)  GPIO_WriteBit(DHT11_IO_PORT, DHT11_IO_PIN, (BitAction)(x))
#define DHT11_DQ_IN      GPIO_ReadInputDataBit(DHT11_IO_PORT, DHT11_IO_PIN)

/**
  * @brief  复位 DHT11
  */
static void DHT11_Rst(void)
{
    DHT11_IO_OUT();     // 设置为输出
    DHT11_DQ_OUT(0);    // 拉低数据线
    Delay_ms(20);       // 拉低至少 18ms
    DHT11_DQ_OUT(1);    // 拉高数据线
    Delay_us(30);       // 主机拉高 20~40us
}

/**
  * @brief  检查 DHT11 是否响应
  * @retval 0: 存在, 1: 不存在
  */
static uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;
    DHT11_IO_IN();      // 设置为输入
    
    // 等待 DHT11 拉低 (响应信号)
    while (DHT11_DQ_IN && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    if (retry >= 100) return 1;
    
    retry = 0;
    // 等待 DHT11 拉高 (响应结束)
    while (!DHT11_DQ_IN && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    if (retry >= 100) return 1;
    
    return 0;
}

/**
  * @brief  读取一位数据
  */
static uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0;
    
    // 等待变为低电平 (上一位数据结束，开始下一位)
    while (DHT11_DQ_IN && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    
    retry = 0;
    // 等待变为高电平 (数据开始传输)
    while (!DHT11_DQ_IN && retry < 100)
    {
        retry++;
        Delay_us(1);
    }
    
    // 延时 40us 后检测电平
    // '0': 高电平持续 26-28us
    // '1': 高电平持续 70us
    Delay_us(40);
    
    if (DHT11_DQ_IN) return 1;
    else return 0;
}

/**
  * @brief  读取一个字节
  */
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, dat = 0;
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        dat |= DHT11_Read_Bit();
    }
    return dat;
}

/**
  * @brief  DHT11 初始化
  */
uint8_t DHT11_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = DHT11_IO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 初始推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_IO_PORT, &GPIO_InitStructure);
    
    DHT11_Rst();
    return DHT11_Check();
}

/**
  * @brief  读取温湿度数据
  */
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5];
    uint8_t i;
    
    DHT11_Rst();
    if (DHT11_Check() == 0)
    {
        for (i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();
        }
        
        // 校验和检查: buf[4] == buf[0] + buf[1] + buf[2] + buf[3]
        if (buf[4] == (buf[0] + buf[1] + buf[2] + buf[3]))
        {
            *humi = buf[0]; // 湿度整数部分
            *temp = buf[2]; // 温度整数部分
            return 0;
        }
    }
    return 1;
}
