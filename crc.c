
#include "config.h"
#include "crc.h"

/* crc-ccitt but with msb first */
u16 crc_ccitt_msb(u16 crc, unsigned char *buf, int count)
{
    int i, j;
    for (i=0; i<count; i++) 
    {
        crc ^= buf[i] << 8;
        for (j = 0; j < 8; j++)
            crc = (crc << 1) ^ ((crc & 0x8000) ? POLY : 0);
    }
    return crc;
}
