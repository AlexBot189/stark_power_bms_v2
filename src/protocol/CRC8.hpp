#pragma once

void InitCrc8();

void CRC8(unsigned char* crc, unsigned char m);

void GetStrCrc8(unsigned char* crc, unsigned char* str, int length);