// Yep, this is actually -*- c++ -*-

#include <avr/delay.h>
#include "Temps.hh"
#include "LCD.hh"
#include "Tool.hh"
#include "Screens.hh"
#include "EepromMap.hh"
#include "Motherboard.hh"
#include "ModtronixLCD2S.hh"


namespace lcdifc {


#if (HAS_MODTRONIX_LCD)

#define LCD_MAXBUF 80
ModtronixLCD2S Lcd(LCD_I2C_ADDRESS);


const char keypadmap[] = {
  KEY_EMINUS,  KEY_ZPLUS,   KEY_YPLUS,   KEY_STOP,
  KEY_XMINUS,  KEY_ESTOP,   KEY_XPLUS,   KEY_PAUSE,
  KEY_YMINUS,  KEY_ZMINUS,  KEY_EPLUS,   KEY_MENU,
  KEY_ZERO,    KEY_TEMPS,   KEY_UNITS,   KEY_ENTER
};


static char    last_polled_key;
static uint8_t last_polled_lcdbuf_free;
static Timeout keypad_timeout;
static Timeout temps_timeout;
static Timeout lcd_buffer_timeout;
static bool    lcd_saving_to_flash = false;
static bool    lcd_is_connected = false;

static enum lcd_states {
  LCDSTATE_UNINITIALIZED = 0,
  LCDSTATE_INIT_START,
  LCDSTATE_INIT_BRIGHTNESS,
  LCDSTATE_INIT_CONTRAST,
  LCDSTATE_INIT_DISPLAY1,
  LCDSTATE_INIT_DISPLAY2,
  LCDSTATE_INIT_DISPLAY3,
  LCDSTATE_INIT_DISPLAY4,
  LCDSTATE_INIT_FLASH1,
  LCDSTATE_INIT_FLASH2,
  LCDSTATE_INIT_FLASH3,
  LCDSTATE_INIT_FLASH4,
  LCDSTATE_INIT_FLASH5,
  LCDSTATE_INIT_FLASH6,
  LCDSTATE_INIT_FLASH7,
  LCDSTATE_INIT_CHAR1,
  LCDSTATE_INIT_CHAR2,
  LCDSTATE_INIT_CHAR3,
  LCDSTATE_INIT_CHAR4,
  LCDSTATE_INIT_FINALIZE,
  LCDSTATE_HALT,
  LCDSTATE_READY
} lcd_state;



void defer_next_command(uint16_t bytes_just_sent) {
  lcd_buffer_timeout.start(2000L*bytes_just_sent);
}



void process_init_stage()
{
  char buf[25];
  PGM_P line1 = PSTR("                    ");
  PGM_P line2 = PSTR("Revar's LCD Firmware");
  PGM_P line3 = PSTR("       v2.0         ");

  switch (lcd_state) {
    case LCDSTATE_INIT_START:
      Lcd.clear();
      Lcd.display_on();
      Lcd.backlight_on();
      lcd_state = LCDSTATE_INIT_BRIGHTNESS;
      defer_next_command(25);
      break;

    case LCDSTATE_INIT_BRIGHTNESS:
      Lcd.backlight_brightness(lcdifc::MAX_BRIGHTNESS);
      lcd_state = LCDSTATE_INIT_CONTRAST;
      defer_next_command(8*4);  // Saves to flash.  Give it extra time.
      break;

    case LCDSTATE_INIT_CONTRAST: {
      uint8_t cont = eeprom::getEeprom8(eeprom::LCD_CONTRAST,MAX_CONTRAST);
      if (cont < 200) {
        cont = 255;
      }
      Lcd.set_contrast(cont);
      lcd_state = LCDSTATE_INIT_DISPLAY1;
      defer_next_command(8*4);  // Saves to flash.  Give it extra time.
      break;
    }


    case LCDSTATE_INIT_DISPLAY1:
      Lcd.write_string_P(line1);
      defer_next_command(22);
      lcd_state = LCDSTATE_INIT_DISPLAY2;
      break;

    case LCDSTATE_INIT_DISPLAY2:
      Lcd.write_string_P(line2);
      defer_next_command(22);
      lcd_state = LCDSTATE_INIT_DISPLAY3;
      break;

    case LCDSTATE_INIT_DISPLAY3:
      Lcd.write_string_P(line3);
      defer_next_command(22);
      lcd_state = LCDSTATE_INIT_DISPLAY4;
      break;

    case LCDSTATE_INIT_DISPLAY4:
      Lcd.write_string_P(line1);
      defer_next_command(22);
      if (lcd_saving_to_flash) {
        lcd_state = LCDSTATE_INIT_FLASH1;
      } else {
        lcd_state = LCDSTATE_INIT_CHAR1;
      }
      break;


    case LCDSTATE_INIT_FLASH1:
      Lcd.clear();
      Lcd.write_string_P(PSTR("\n    Initializing\n   LCD Firmware..."));
      defer_next_command(45);
      lcd_state = LCDSTATE_INIT_FLASH2;
      break;

    case LCDSTATE_INIT_FLASH2:
      // Initializes RevarLCD specific eeprom settings
      eeprom::setEeprom16(eeprom::HEAD_TEMP,220);
      eeprom::setEeprom16(eeprom::PLATFORM_TEMP,110);
      eeprom::setEeprom8(eeprom::LCD_CONTRAST,254);
      // Initialize screen line.
      strcpy_P(buf, line1);
      Lcd.set_startup_line(1, buf);
      defer_next_command(22*4);  // Saves to flash.  Give it extra time.
      lcd_state = LCDSTATE_INIT_FLASH3;
      break;

    case LCDSTATE_INIT_FLASH3:
      strcpy_P(buf, line2);
      Lcd.set_startup_line(2, buf);
      defer_next_command(22*4);  // Saves to flash.  Give it extra time.
      lcd_state = LCDSTATE_INIT_FLASH4;
      break;

    case LCDSTATE_INIT_FLASH4:
      strcpy_P(buf, line3);
      Lcd.set_startup_line(3, buf);
      defer_next_command(22*4);  // Saves to flash.  Give it extra time.
      lcd_state = LCDSTATE_INIT_FLASH5;
      break;

    case LCDSTATE_INIT_FLASH5:
      strcpy_P(buf, line1);
      Lcd.set_startup_line(4, buf);
      defer_next_command(22*4);  // Saves to flash.  Give it extra time.
      lcd_state = LCDSTATE_INIT_FLASH6;
      break;

    case LCDSTATE_INIT_FLASH6:
      Lcd.config_keypad_and_io(0);
      defer_next_command(10*4);  // Saves to flash.  Give it extra time.
      lcd_state = LCDSTATE_INIT_FLASH7;
      break;

    case LCDSTATE_INIT_FLASH7:
      Lcd.set_keypad_debounce_time(100/8);
      defer_next_command(10*4);  // Saves to flash.  Give it extra time.
      lcd_state = LCDSTATE_INIT_CHAR1;
      break;


    case LCDSTATE_INIT_CHAR1: {
      if (lcd_saving_to_flash) {
        Lcd.remember();
        defer_next_command(20*4);
      } else {
        defer_next_command(20);
      }
      const uint8_t pausechar[8]  = { 0x00,0x1b,0x1b,0x1b,0x1b,0x1b,0x1b,0x00 };
      Lcd.define_custom_char(PAUSE_CHAR, pausechar);
      lcd_state = LCDSTATE_INIT_CHAR2;
      break;
    }

    case LCDSTATE_INIT_CHAR2: {
      if (lcd_saving_to_flash) {
        Lcd.remember();
        defer_next_command(20*4);
      } else {
        defer_next_command(20);
      }
      const uint8_t playchar[8]   = { 0x00,0x18,0x1e,0x1f,0x1e,0x18,0x00,0x00 };
      Lcd.define_custom_char(PLAY_CHAR, playchar);
      lcd_state = LCDSTATE_INIT_CHAR3;
      break;
    }

    case LCDSTATE_INIT_CHAR3: {
      if (lcd_saving_to_flash) {
        Lcd.remember();
        defer_next_command(20*4);
      } else {
        defer_next_command(20);
      }
      const uint8_t extfwdchar[8] = { 0x1f,0x1f,0x1f,0x04,0x00,0x04,0x04,0x03 };
      Lcd.define_custom_char(EXT_FWD_CHAR, extfwdchar);
      lcd_state = LCDSTATE_INIT_CHAR4;
      break;
    }

    case LCDSTATE_INIT_CHAR4: {
      if (lcd_saving_to_flash) {
        Lcd.remember();
      }
      const uint8_t extrevchar[8] = { 0x00,0x04,0x0e,0x1f,0x00,0x1f,0x00,0x00 };
      Lcd.define_custom_char(EXT_REV_CHAR, extrevchar);
      defer_next_command(1000);
      lcd_state = LCDSTATE_INIT_FINALIZE;
      break;
    }

    case LCDSTATE_INIT_FINALIZE:
      Lcd.clear();
      defer_next_command(10);
      lcd_state = LCDSTATE_READY;
      break;
  }
}




void init_to_flash()
{
  lcd_saving_to_flash = true;
  lcd_state = LCDSTATE_INIT_START;
  lcd_buffer_timeout.start(50000L);
}



void init()
{
  asynctwi::init();
  lcd_saving_to_flash = false;
  lcd_state = LCDSTATE_INIT_START;

  last_polled_key = '\0';
  last_polled_lcdbuf_free = LCD_MAXBUF;
  lcd_is_connected = true;
  keypad_timeout.start(100000L);
  temps_timeout.start(1000000L);
  lcd_buffer_timeout.start(50000L);
}




static void reconnect()
{
  defer_next_command(25);
  init();
  Screen::setNeedsDraw();
}



static void disconnect()
{
  lcd_is_connected = false;
}



static void keypad_poll_cb(uint8_t cmd, uint8_t* data, uint8_t len)
{
  if (len == 0) {
    return;
  }
  char key = (char)data[0];
  if (!key) {
    return;
  }
  last_polled_key = keypadmap[key-'a'];
}



static void status_poll_cb(uint8_t cmd, uint8_t* data, uint8_t len)
{
  if (len == 0) {
    if (lcd_is_connected) {
      disconnect();
    }
    return;
  }
  if (!lcd_is_connected) {
    reconnect();
  }
  uint8_t status = data[0];
  if ((status & 0x80)) {
    // Keypress data to read.
    Lcd.read_keypad_data_async(keypad_poll_cb);
  }
  last_polled_lcdbuf_free = (status & 0x7f);
}




void runLcdSlice()
{
  // If we were asked to defer before sending our next lcd command, wait for it.
  if (!lcd_buffer_timeout.hasElapsed()) {
    return;
  }
  if (lcd_state != LCDSTATE_READY) {
    process_init_stage();
    return;
  }

  // Every tenth of a second or so, poll for keypad status.
  if (keypad_timeout.hasElapsed()) {
    Lcd.read_status_async(status_poll_cb);
    if (lcd_is_connected) {
      keypad_timeout.start(100000L);
    } else {
      keypad_timeout.start(2000000L);
    }
  }

  // Every second or so, get an update on the temperatures.
  if (temps_timeout.hasElapsed()) {
    temps::pollCurrentTemps();
    temps_timeout.start(100000L);
  }

  // If we get a character from the keypad, process it.
  if (last_polled_key) {
    Screen::processKey(last_polled_key);
    last_polled_key = '\0';
  }

  // Finish sending pending commands before we task the LCD with more.
  if (asynctwi::pending() > 0) {
    return;
  }

  // Don't spam LCD with data until it has a clear buffer.
  if (last_polled_lcdbuf_free < LCD_MAXBUF-5) {
      return;
  }

  // Display the current Screen if necessary.
  Screen::update();
}




/////////////////////////////////////////////////////////////////////////
// LCD control abstractions for the LCD control Screens
// This is set up to use the ModtronixLCD2S controller object.
// We're expecting a 4x20 LCD screen with 4x4 keypad support.
/////////////////////////////////////////////////////////////////////////


void write_char(char c)
{
  if (!lcd_is_connected) {
    return;
  }
  char buf[2];
  buf[0] = c;
  buf[1] = '\0';
  Lcd.write_string(buf);
}


void write(const char *str)
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.write_string(str);
}


void write_P(PGM_P str)
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.write_string_P(str);
}


void set_position(uint8_t row, uint8_t col)
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.set_position(row, col);
}


void clear()
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.clear();
}


void cursor_on()
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.cursor_block_on();
}


void cursor_off()
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.cursor_block_off();
}


void set_contrast(uint8_t val)
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.set_contrast(val);
}


void set_brightness(uint8_t val)
{
  if (!lcd_is_connected) {
    return;
  }
  Lcd.backlight_brightness(val);
}

#else  // !HAS_MODTRONIX_LCD

void runLcdSlice()
{
  return;
}

#endif // HAS_MODTRONIX_LCD

} // namespace


// vim: set sw=2 autoindent nowrap expandtab: settings


