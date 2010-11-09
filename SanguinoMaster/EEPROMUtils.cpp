#include "EEPROMOffsets.h"

bool hasEEPROMSettings() {
  return (EEPROM.read(0) == EEPROM_CHECK_LOW &&
	  EEPROM.read(1) == EEPROM_CHECK_HIGH);
}


uint8_t getEEPROMbyte(int base, uint8_t def) {
  uint8_t tmp = def;
  if (hasEEPROMSettings()) {
    tmp = EEPROM.read(base);
  }
  if (tmp == 0) {
      // Uninitialized EEPROM memory is probably 0.  Det to default.
      tmp = def;
  }
  return tmp;
}


int16_t getEEPROMword(int base, int16_t def) {
  uint8_t tmplo = 0;
  int16_t tmp = def;
  if (hasEEPROMSettings()) {
    tmplo = EEPROM.read(base);
    tmp = EEPROM.read(base+1);
    tmp <<= 8;
    tmp |= tmplo;
  }
  if (tmp == 0) {
      // Uninitialized EEPROM memory is probably 0.  Det to default.
      tmp = def;
  }
  return tmp;
}


void setEEPROMword(int base, int16_t val) {
  EEPROM.write(base, val & 0xff);
  EEPROM.write(base+1, (val >> 8) & 0xff);
}


uint16_t getEEPROMHeadTemp() {
  return getEEPROMword(EEPROM_HEAD_TEMP_OFFSET, 220);
  }



uint16_t getEEPROMPlatformTemp() {
  return getEEPROMword(EEPROM_PLATFORM_TEMP_OFFSET, 110);
}



uint8_t getEEPROMLcdContrast() {
  return getEEPROMbyte(EEPROM_LCD_CONTRAST_OFFSET, 254);
  }



int8_t getEEPROMTempAdjust() {
  return getEEPROMbyte(EEPROM_TEMP_ADJUST_OFFSET, 0);
}



void setEEPROMHeadTemp(uint16_t val) {
  setEEPROMword(EEPROM_HEAD_TEMP_OFFSET, val);
}



void setEEPROMPlatformTemp(uint16_t val) {
  setEEPROMword(EEPROM_PLATFORM_TEMP_OFFSET, val);
}



void setEEPROMTempAdjust(int8_t val) {
  EEPROM.write(EEPROM_TEMP_ADJUST_OFFSET, val & 0xff);
}



