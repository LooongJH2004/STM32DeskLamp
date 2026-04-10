#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @brief  计算 CRC16-CCITT (XMODEM)
 *         Poly: 0x1021, Init: 0x0000
 * @param  data: 数据指针
 * @param  length: 数据长度
 * @return CRC16 校验码
 */
uint16_t CRC16_Calculate(const uint8_t *data, size_t length);
