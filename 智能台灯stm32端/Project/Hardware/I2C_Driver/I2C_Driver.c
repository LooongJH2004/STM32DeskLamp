/**
  ******************************************************************************
  * @file    I2C_Driver.c
  * @brief   软件模拟 I2C 实现
  ******************************************************************************
  */
#include "I2C_Driver.h"
#include "SystemSupport.h" // 需要 Delay_us

// --- 引脚定义 ---
// Group 1: OLED (I2C1 Remap)
#define I2C1_PORT       GPIOB
#define I2C1_SCL_PIN    GPIO_Pin_8
#define I2C1_SDA_PIN    GPIO_Pin_9

// Group 2: Sensor (I2C2)
#define I2C2_PORT       GPIOB
#define I2C2_SCL_PIN    GPIO_Pin_10
#define I2C2_SDA_PIN    GPIO_Pin_11

// --- 延时控制 (调节 I2C 速度) ---
// 延时越长速度越慢，越稳定。PAJ7620 建议慢一点。
static void I2C_Delay(void)
{
    // 简单的空循环延时，大约 2-4us
    volatile int i = 10;
    while (i--);
}

// --- GPIO 操作宏 ---
#define SCL_H(port, pin)    GPIO_SetBits(port, pin)
#define SCL_L(port, pin)    GPIO_ResetBits(port, pin)
#define SDA_H(port, pin)    GPIO_SetBits(port, pin)
#define SDA_L(port, pin)    GPIO_ResetBits(port, pin)
#define SDA_READ(port, pin) GPIO_ReadInputDataBit(port, pin)

// --- 内部状态变量 ---
static GPIO_TypeDef* CUR_PORT;
static uint16_t      CUR_SCL;
static uint16_t      CUR_SDA;

// --- 切换当前操作的 I2C 组 ---
static void I2C_SelectPort(I2C_TypeDef* I2Cx)
{
    if (I2Cx == I2C1) {
        CUR_PORT = I2C1_PORT;
        CUR_SCL  = I2C1_SCL_PIN;
        CUR_SDA  = I2C1_SDA_PIN;
    } else {
        CUR_PORT = I2C2_PORT;
        CUR_SCL  = I2C2_SCL_PIN;
        CUR_SDA  = I2C2_SDA_PIN;
    }
}

// --- SDA 输入输出模式切换 ---
static void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = CUR_SDA;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(CUR_PORT, &GPIO_InitStructure);
}

static void SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = CUR_SDA;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入 (或浮空)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(CUR_PORT, &GPIO_InitStructure);
}

// --- I2C 底层信号 ---

static void I2C_Start(void)
{
    SDA_OUT();
    SDA_H(CUR_PORT, CUR_SDA);
    SCL_H(CUR_PORT, CUR_SCL);
    I2C_Delay();
    SDA_L(CUR_PORT, CUR_SDA);
    I2C_Delay();
    SCL_L(CUR_PORT, CUR_SCL);
}

static void I2C_Stop(void)
{
    SDA_OUT();
    SCL_L(CUR_PORT, CUR_SCL);
    SDA_L(CUR_PORT, CUR_SDA);
    I2C_Delay();
    SCL_H(CUR_PORT, CUR_SCL);
    I2C_Delay();
    SDA_H(CUR_PORT, CUR_SDA);
    I2C_Delay();
}

static uint8_t I2C_WaitAck(void)
{
    uint8_t ucErrTime = 0;
    SDA_IN();
    SDA_H(CUR_PORT, CUR_SDA); I2C_Delay();
    SCL_H(CUR_PORT, CUR_SCL); I2C_Delay();
    
    while (SDA_READ(CUR_PORT, CUR_SDA))
    {
        ucErrTime++;
        if (ucErrTime > 250)
        {
            I2C_Stop();
            return 1; // No ACK
        }
    }
    SCL_L(CUR_PORT, CUR_SCL);
    return 0; // ACK OK
}

static void I2C_Ack(void)
{
    SCL_L(CUR_PORT, CUR_SCL);
    SDA_OUT();
    SDA_L(CUR_PORT, CUR_SDA);
    I2C_Delay();
    SCL_H(CUR_PORT, CUR_SCL);
    I2C_Delay();
    SCL_L(CUR_PORT, CUR_SCL);
}

static void I2C_NAck(void)
{
    SCL_L(CUR_PORT, CUR_SCL);
    SDA_OUT();
    SDA_H(CUR_PORT, CUR_SDA);
    I2C_Delay();
    SCL_H(CUR_PORT, CUR_SCL);
    I2C_Delay();
    SCL_L(CUR_PORT, CUR_SCL);
}

static void I2C_SendByte(uint8_t txd)
{
    uint8_t t;
    SDA_OUT();
    SCL_L(CUR_PORT, CUR_SCL);
    for (t = 0; t < 8; t++)
    {
        if ((txd & 0x80) >> 7)
            SDA_H(CUR_PORT, CUR_SDA);
        else
            SDA_L(CUR_PORT, CUR_SDA);
        txd <<= 1;
        I2C_Delay();
        SCL_H(CUR_PORT, CUR_SCL);
        I2C_Delay();
        SCL_L(CUR_PORT, CUR_SCL);
        I2C_Delay();
    }
}

static uint8_t I2C_ReadByte(unsigned char ack)
{
    unsigned char i, receive = 0;
    SDA_IN();
    for (i = 0; i < 8; i++)
    {
        SCL_L(CUR_PORT, CUR_SCL);
        I2C_Delay();
        SCL_H(CUR_PORT, CUR_SCL);
        receive <<= 1;
        if (SDA_READ(CUR_PORT, CUR_SDA)) receive++;
        I2C_Delay();
    }
    if (!ack)
        I2C_NAck();
    else
        I2C_Ack();
    return receive;
}

// --- 接口实现 ---

void I2C_Lib_Init(I2C_TypeDef* I2Cx)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 无论 I2Cx 是什么，我们都初始化对应的 GPIO
    // 软件 I2C 不需要开启 I2C 外设时钟，也不需要 AFIO 重映射
    
    if (I2Cx == I2C1) {
        // PB8, PB9
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    } else {
        // PB10, PB11
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    }
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 默认拉高
    if (I2Cx == I2C1) {
        GPIO_SetBits(GPIOB, GPIO_Pin_8 | GPIO_Pin_9);
    } else {
        GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
    }
}

uint8_t I2C_Lib_Write(I2C_TypeDef* I2Cx, uint8_t DevAddr, uint8_t RegAddr, uint8_t* pData, uint16_t Size)
{
    I2C_SelectPort(I2Cx);
    I2C_Start();
    
    I2C_SendByte(DevAddr); // 写地址
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    
    I2C_SendByte(RegAddr); // 寄存器地址
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    
    for (uint16_t i = 0; i < Size; i++) {
        I2C_SendByte(pData[i]);
        if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    }
    
    I2C_Stop();
    return 0;
}

uint8_t I2C_Lib_Read(I2C_TypeDef* I2Cx, uint8_t DevAddr, uint8_t RegAddr, uint8_t* pData, uint16_t Size)
{
    I2C_SelectPort(I2Cx);
    
    // 1. 写寄存器地址
    I2C_Start();
    I2C_SendByte(DevAddr);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    
    I2C_SendByte(RegAddr);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    
    // 2. 重启总线，读数据
    I2C_Start();
    I2C_SendByte(DevAddr | 0x01); // 读地址
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    
    for (uint16_t i = 0; i < Size; i++) {
        // 如果是最后一个字节，发送 NACK
        if (i == Size - 1) {
            pData[i] = I2C_ReadByte(0);
        } else {
            pData[i] = I2C_ReadByte(1);
        }
    }
    
    I2C_Stop();
    return 0;
}

uint8_t I2C_Lib_WriteDirect(I2C_TypeDef* I2Cx, uint8_t DevAddr, uint8_t* pData, uint16_t Size)
{
    I2C_SelectPort(I2Cx);
    I2C_Start();
    
    I2C_SendByte(DevAddr);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    
    for (uint16_t i = 0; i < Size; i++) {
        I2C_SendByte(pData[i]);
        if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    }
    
    I2C_Stop();
    return 0;
}

uint8_t I2C_Lib_IsDeviceReady(I2C_TypeDef* I2Cx, uint8_t DevAddr)
{
    I2C_SelectPort(I2Cx);
    I2C_Start();
    
    I2C_SendByte(DevAddr);
    if (I2C_WaitAck()) {
        I2C_Stop();
        return 1; // 失败
    }
    
    I2C_Stop();
    return 0; // 成功
}
