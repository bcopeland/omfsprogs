#ifndef _CRC_H
#define _CRC_H

#include "config.h"

#define POLY 0x1021

u16 crc_ccitt_msb(u16 crc, unsigned char *buf, int count);

#endif 
