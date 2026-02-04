/**
  ******************************************************************************
  * @file    OLED.c
  * @brief   OLED 驱动 (V6.3 Init Fix)
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "OLED.h"
#include "OLED_Font.h"
#include "I2C_Driver.h"

#define OLED_I2C_ADDR   0x78
#define OLED_I2C        I2C1

/**
  * @brief  检测 OLED 是否连接正常
  */
uint8_t OLED_IsReady(void)
{
    if (I2C_Lib_IsDeviceReady(OLED_I2C, OLED_I2C_ADDR) == 0)
    {
        return 1;
    }
    return 0;
}

static void OLED_WriteCommand(uint8_t Command)
{
    uint8_t data[2];
    data[0] = 0x00; 
    data[1] = Command;
    I2C_Lib_WriteDirect(OLED_I2C, OLED_I2C_ADDR, data, 2);
}

static void OLED_WriteData(uint8_t Data)
{
    uint8_t data[2];
    data[0] = 0x40; 
    data[1] = Data;
    I2C_Lib_WriteDirect(OLED_I2C, OLED_I2C_ADDR, data, 2);
}

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i]);
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

static uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--) Result *= X;
    return Result;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number1 = -Number;
    }
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)
    {
        SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
        if (SingleNumber < 10)
        {
            OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        }
        else
        {
            OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
        }
    }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
    }
}

/**
  * @brief  OLED 初始化
  */
void OLED_Init(void)
{
    uint32_t i, j;

    // 上电延时
    for (i = 0; i < 1000; i++)
    {
        for (j = 0; j < 1000; j++);
    }

    I2C_Lib_Init(OLED_I2C);

    // 【关键修复】
    // 移除这里的 OLED_IsReady() 检查！
    // 很多 OLED 屏幕在未初始化前不会 ACK，导致这里直接返回，屏幕无法启动。
    // if (!OLED_IsReady()) return; 

    OLED_WriteCommand(0xAE); 
    OLED_WriteCommand(0xD5); 
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8); 
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3); 
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40); 
    OLED_WriteCommand(0xA1); 
    OLED_WriteCommand(0xC8); 
    OLED_WriteCommand(0xDA); 
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81); 
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9); 
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB); 
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4); 
    OLED_WriteCommand(0xA6); 
    OLED_WriteCommand(0x8D); 
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF); 

    OLED_Clear();
}
