#include "Configuration.hh"


#ifndef LCD_H
#define LCD_H

#include "ModtronixLCD2S.hh"

namespace lcdifc {

#if (HAS_MODTRONIX_LCD)
const static uint8_t PAUSE_CHAR          =  1;
const static uint8_t PLAY_CHAR           =  2;
const static uint8_t EXT_FWD_CHAR        =  3;
const static uint8_t EXT_REV_CHAR        =  4;
const static uint8_t EXT_HEAT_CHAR       =  5;
const static uint8_t PLATFORM_HEAT_CHAR  =  6;
const static uint8_t UP_ARROW_CHAR       =  '\305';
const static uint8_t DOWN_ARROW_CHAR     =  '\306';
const static uint8_t RIGHT_ARROW_CHAR    =  '\307';
const static uint8_t LEFT_ARROW_CHAR     =  '\310';

void init();
void init_to_flash();
void defer_next_command(uint16_t bytes_just_sent);

void wait_for_free_space(uint8_t num);
void write(const char *str);
void write_P(PGM_P str);
void write_char(char c);
void set_position(uint8_t row, uint8_t col);
void clear();
void cursor_on();
void cursor_off();
void set_contrast(uint8_t val);
void set_brightness(uint8_t val);


#ifdef KEYPAD_ORIENT_ONEONTOP
  // We expect a telephone style one-on-top keypad layout
  //  1E-   2Z+   3Y+   Stop
  //  4X-   5E0   6X+   Pause
  //  7Y-   8Z-   9E+   Menu
  //  0Zero Temp  Units Enter

  const static uint8_t KEY_EMINUS     = '1';
  const static uint8_t KEY_ZPLUS      = '2';
  const static uint8_t KEY_YPLUS      = '3';

  const static uint8_t KEY_XMINUS     = '4';
  const static uint8_t KEY_ESTOP      = '5';
  const static uint8_t KEY_XPLUS      = '6';

  const static uint8_t KEY_YMINUS     = '7';
  const static uint8_t KEY_ZMINUS     = '8';
  const static uint8_t KEY_EPLUS      = '9';

#else
  // We expect a computer style one-on-bottom keypad layout
  //  7E-   8Z+   9Y+   Stop
  //  4X-   5E0   6X+   Pause
  //  1Y-   2Z-   3E+   Menu
  //  0Zero Temp  Units Enter

  const static uint8_t KEY_EMINUS     = '7';
  const static uint8_t KEY_ZPLUS      = '8';
  const static uint8_t KEY_YPLUS      = '9';

  const static uint8_t KEY_XMINUS     = '4';
  const static uint8_t KEY_ESTOP      = '5';
  const static uint8_t KEY_XPLUS      = '6';

  const static uint8_t KEY_YMINUS     = '1';
  const static uint8_t KEY_ZMINUS     = '2';
  const static uint8_t KEY_EPLUS      = '3';

#endif


const static uint8_t KEY_STOP      = 'S';
const static uint8_t KEY_PAUSE     = 'P';
const static uint8_t KEY_MENU      = 'M';
const static uint8_t KEY_ENTER     = 'E';
const static uint8_t KEY_TEMPS     = 'T';
const static uint8_t KEY_UNITS     = 'U';
const static uint8_t KEY_ZERO      = '0';
const static uint8_t KEY_NEGATE    = KEY_TEMPS;
const static uint8_t KEY_DELETE    = KEY_UNITS;


const static uint8_t MAX_CONTRAST   = 254;
const static uint8_t MAX_BRIGHTNESS = 254;
#endif // HAS_MODTRONIX_LCD

void runLcdSlice();

} // namespace
#endif // LCD_H


// vim: set sw=2 autoindent nowrap expandtab: settings

