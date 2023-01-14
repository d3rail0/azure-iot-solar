#ifndef UTILS_H
#define UTILS_H

#include <EEPROM.h>

#define EEPROM_END 0
#define EEPROM_START 1

// Give user an option to enter new credentials
// like SSID, PWD and Azure connection string
bool need_erase_EEPROM();

// reset the EEPROM
void clear_EEPROM();

// Write `size` amount of bytes from `data` buffer
// starting from address `addr`
void EEPROM_write(int addr, char *data, int size);

// read bytes starting from `addr` until '\0'
// return the length of total bytes read.
int EEPROM_read(int addr, char *buf);

#endif