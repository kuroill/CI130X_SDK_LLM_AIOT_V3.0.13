
#ifndef __CIAS_MESSAGE_HANDLE__
#define __CIAS_MESSAGE_HANDLE__
#include "stdint.h"

uint16_t crc16_ccitt(uint16_t pre_crc, const uint8_t * data, uint32_t length);
#endif