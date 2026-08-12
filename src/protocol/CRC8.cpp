#include "CRC8.hpp"

#define GP 0x107 /* x^8 + x^2 + x + 1 */
#define DI 0x07

static unsigned char crc8_table[256]; /* 8-bit table */
static int made_table = 0;
void InitCrc8()
{
    int i, j;
    unsigned char crc;
    if (!made_table)
    {
        for (i = 0; i < 256; i++)
        {
            crc = i;
            for (j = 0; j < 8; j++)
                crc = (crc << 1) ^ ((crc & 0x80) ? DI : 0);
            crc8_table[i] = crc & 0xFF;
            /* printf("table[%d] = %d (0x%X)\n", i, crc, crc); */
        }
        made_table = 1;
    }
}

/*
* For a byte array whose accumulated crc value is stored in *crc, computes
* resultant crc obtained by appending m to the byte array
*/
void CRC8(unsigned char* crc, unsigned char m)
{
    if (!made_table)
        InitCrc8();
    *crc = crc8_table[(*crc) ^ m];
    *crc &= 0xFF;
}

void GetStrCrc8(unsigned char* crc, unsigned char* str, int length)
{
    for (int i = 0; i < length; i++)
    {
        CRC8(crc, *(str + i));
    }
}
