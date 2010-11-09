#include <string.h>
#include <avr/pgmspace.h>
#include <avr/delay.h>
#include "Screens.hh"
#include "SDCard.hh"
#include "Steppers.hh"
#include "Tool.hh"
#include "Command.hh"
#include "EepromMap.hh"
#include "Command.hh"
#include "Commands.hh"
#include "LCDConfigs.hh"
#include "Temps.hh"
#include "Main.hh"


#if (HAS_MODTRONIX_LCD)

#define COMMAND_PUSH8(x)  command::push(x)
#define COMMAND_PUSH16(x) command::push16(x)
#define COMMAND_PUSH32(x) command::push32(x)
#define COMMAND_SPACE_FREE() command::getRemainingCapacity()


bool strcasesuffix_P(const char* str, PGM_P patstr)
{
  size_t patlen = strlen_P(patstr);
  size_t len = strlen(str);
  if (len < patlen) {
    return false;
  }
  return (!strcasecmp_P(str+(len-patlen), patstr));
}





extern class DefaultScreen     default_screen;
extern class MainMenuScreen    main_menu_screen;
extern class FileSelectScreen  file_select_screen;
extern class TempMenuScreen    temp_menu_screen;
extern class ZeroMenuScreen    zero_menu_screen;
extern class LcdContrastScreen lcd_contrast_screen;

Screen*  Screen::current_screen = 0;
ScreenRedraw_t Screen::needs_update = DRAW;
static Timeout delay_timeout;

static const uint8_t currentToolIndex = 0;
const static uint8_t jog_length_xy[] = {
  (uint8_t)( 0.1*lcdcfg::STEPS_PER_MM_XY+0.5),  //  ~0.1 mm
  (uint8_t)( 1.0*lcdcfg::STEPS_PER_MM_XY+0.5),  //  ~1 mm
  (uint8_t)( 5.0*lcdcfg::STEPS_PER_MM_XY+0.5),  //  ~5 mm
  (uint8_t)(10.0*lcdcfg::STEPS_PER_MM_XY+0.5),  // ~10 mm
  (uint8_t)(20.0*lcdcfg::STEPS_PER_MM_XY+0.5)   // ~20 mm
  };
const static uint16_t jog_length_z[] = {
  (uint16_t)( 0.1*lcdcfg::STEPS_PER_MM_Z+0.5),   //  ~0.1 mm
  (uint16_t)( 1.0*lcdcfg::STEPS_PER_MM_Z+0.5),   //  ~1 mm
  (uint16_t)( 5.0*lcdcfg::STEPS_PER_MM_Z+0.5),   //  ~5 mm
  (uint16_t)(10.0*lcdcfg::STEPS_PER_MM_Z+0.5),   // ~10 mm
  (uint16_t)(20.0*lcdcfg::STEPS_PER_MM_Z+0.5)    // ~20 mm
  };
const static char jogtxt_step[] PROGMEM = "Step";
const static char jogtxt_1mm[]  PROGMEM = " 1mm";
const static char jogtxt_5mm[]  PROGMEM = " 5mm";
const static char jogtxt_10mm[] PROGMEM = "10mm";
const static char jogtxt_20mm[] PROGMEM = "20mm";
static PGM_P jog_length_txt[] = {jogtxt_step, jogtxt_1mm, jogtxt_5mm, jogtxt_10mm, jogtxt_20mm };


static void setpos(long x, long y, long z) {
    COMMAND_PUSH8(HOST_CMD_SET_POSITION);
    COMMAND_PUSH32(x);
    COMMAND_PUSH32(y);
    COMMAND_PUSH32(z);
}

static void moveto(long x, long y, long z, uint32_t stepdelay) {
    COMMAND_PUSH8(HOST_CMD_QUEUE_POINT_ABS);
    COMMAND_PUSH32(x);
    COMMAND_PUSH32(y);
    COMMAND_PUSH32(z);
    COMMAND_PUSH32(stepdelay);

    COMMAND_PUSH8(HOST_CMD_ENABLE_AXES);
    COMMAND_PUSH8(0x7);
}


static void send_tool_simple_command_with_byte(uint8_t tool, uint8_t cmd, uint8_t val) {
    // This needs to be sent to the tool immediately, as the
    // command buffer may have no space when you are trying to
    // tweak the temperature during a build, and you don't want
    // to delay the tweaks.

    // Keep trying until the tool lock is free.
    for (;;) {
        if (tool::getLock()) {
            OutPacket& out = tool::getOutPacket();
            InPacket& in = tool::getInPacket();
            out.reset();
            out.append8(tool);
            out.append8(cmd);
            out.append8(val);

            // we don't care about the response, so we can release
            // the lock after we initiate the transfer
            tool::startTransaction();
            while (!tool::isTransactionDone()) {
              tool::runToolSlice();
            }
            tool::releaseLock();
            break;
        }
    }
}



static void send_tool_simple_command_with_word(uint8_t tool, uint8_t cmd, uint16_t val) {
    // This needs to be sent to the tool immediately, as the
    // command buffer may have no space when you are trying to
    // tweak the temperature during a build, and you don't want
    // to delay the tweaks.

    // Keep trying until the tool lock is free.
    for (;;) {
        if (tool::getLock()) {
            OutPacket& out = tool::getOutPacket();
            InPacket& in = tool::getInPacket();
            out.reset();
            out.append8(tool);
            out.append8(cmd);
            out.append16(val);

            tool::startTransaction();
            while (!tool::isTransactionDone()) {
              tool::runToolSlice();
            }
            tool::releaseLock();
            break;
        }
    }
}



static void concat_decimal(char* outbuf, uint8_t buflen, int16_t val, int8_t wid, uint8_t decdigits, uint8_t prependzero)
{
  char buf[10];
  char* ptr = buf;
  char* ptr2;
  bool force = false;
  if (val < 0) {
    val = -val;
    *ptr++ = '-';
  }
  if (decdigits == 5) {
      *ptr++ = '0';
      *ptr++ = '.';
      force = true;
  }
  if (force || val >= 10000) {
    *ptr++ = '0'+(val/10000);
    val = val % 10000;
    force = true;
  }
  if (decdigits == 4) {
      if (ptr == buf) {
	  *ptr++ = '0';
      }
      *ptr++ = '.';
      force = true;
  }
  if (force || val >= 1000) {
    *ptr++ = '0'+(val/1000);
    val = val % 1000;
    force = true;
  }
  if (decdigits == 3) {
      if (ptr == buf) {
	  *ptr++ = '0';
      }
      *ptr++ = '.';
      force = true;
  }
  if (force || val >= 100) {
    *ptr++ = '0'+(val/100);
    val = val % 100;
    force = true;
  }
  if (decdigits == 2) {
      if (ptr == buf) {
	  *ptr++ = '0';
      }
      *ptr++ = '.';
      force = true;
  }
  if (force || val >= 10) {
    *ptr++ = '0'+(val/10);
    val = val % 10;
  }
  if (decdigits == 1) {
      if (ptr == buf) {
	  *ptr++ = '0';
      }
      *ptr++ = '.';
  }
  *ptr++ = '0'+val;
  *ptr = '\0';
  if (ptr-buf < wid) {
    ptr2 = buf+wid;
    while (ptr >= buf) {
      *ptr2-- = *ptr--;
    }
    if (prependzero) {
      while (ptr2 >= buf) {
	*ptr2-- = '0';
      } 
    } else {
      while (ptr2 >= buf) {
	*ptr2-- = ' ';
      } 
    } 
  }
  ptr = buf;
  ptr2 = outbuf;
  while (*ptr2 && ptr2 - outbuf < buflen)
    *ptr2++;
  while (*ptr && ptr2 - outbuf < buflen)
    *ptr2++ = *ptr++;
  if (ptr2 - outbuf < buflen) {
    *ptr2 = '\0';
  }
}



bool Screen::hasElapsed() {
  return delay_timeout.hasElapsed();
}
void Screen::startTimer(micros_t timeout) {
  delay_timeout.start(timeout);
}
uint8_t Screen::isUpdateNeeded() { 
  return 0; 
}
void Screen::init() { 
}
void Screen::draw() { 
}
void Screen::redraw() { 
}



uint8_t Screen::handleKey(char c)
{
  switch (c) {
  case lcdifc::KEY_STOP:
    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("\n    Resetting...    "));
    send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_TOGGLE_MOTOR_1, 0x0);
    reset(false);
    COMMAND_PUSH8(HOST_CMD_ABORT);
    Screen::change((Screen*)&default_screen);
    return 1;
  case lcdifc::KEY_PAUSE:
    command::pause(!command::isPaused());
    Screen::change((Screen*)&default_screen);
    return 1;
  case lcdifc::KEY_ZERO:
    Screen::change((Screen*)&zero_menu_screen);
    return 1;
  case lcdifc::KEY_TEMPS:
    Screen::change((Screen*)&temp_menu_screen);
    return 1;
  }
  return 0;
}



void Screen::change(Screen* scrn)
{
  current_screen = scrn;
  Screen::setNeedsDraw();
}




void Screen::update()
{
  if (!hasElapsed()) {
    // Don't try to update faster than the LCD can handle.
    return;
  }
  if (current_screen) {
    if (needs_update == NONE) {
      if (current_screen->isUpdateNeeded()) {
        needs_update = REDRAW;
      }
    }
    if (needs_update == REDRAW) {
      needs_update = NONE;
      current_screen->redraw();
      startTimer(250000L);
    } 
    else if (needs_update == DRAW) {
      needs_update = NONE;
      current_screen->init();
      current_screen->draw();
      startTimer(250000L);
    }
  }
}



void Screen::processKey(char c)
{
  if (current_screen) {
    if (!current_screen->handleKey(c)) {
      // Some keys may need to work in all, or most screens.
      current_screen->Screen::handleKey(c);
    }
  }
}



void Screen::setNeedsDraw()
{
  needs_update = DRAW;
}



void Screen::setNeedsRedraw()
{
  needs_update = REDRAW;
}




void IntegerEntryScreen::init()
{
  value = 0;
}



uint16_t IntegerEntryScreen::getValue()
{
  return value;
}



void IntegerEntryScreen::setValue(int16_t val)
{
  value = val;
}



void IntegerEntryScreen::draw()
{
  lcdifc::set_position(3,1);
  lcdifc::write_P(PSTR("<Enter> when done.\n"));
  lcdifc::write_P(PSTR("<Del> to backspace."));
  redraw();
}



void IntegerEntryScreen::redraw()
{
  char buf[10];
  buf[0] = '\0';
  concat_decimal(buf, sizeof(buf), value, 0, 0, 0);
  strcat_P(buf, PSTR("  \b\b"));
  lcdifc::set_position(2,1);
  lcdifc::write(buf);
  lcdifc::cursor_on();
}



uint8_t IntegerEntryScreen::handleKey(char c)
{
  if (c >= '0' && c <= '9') {
    value *= 10;
    value += c - '0';
    Screen::setNeedsRedraw();
    return 1;
  }
  if (c == lcdifc::KEY_DELETE) {
    value /= 10;
    Screen::setNeedsRedraw();
    return 1;
  }
  if (c == lcdifc::KEY_NEGATE) {
    value = -value;
    Screen::setNeedsRedraw();
    return 1;
  }
  if (c == lcdifc::KEY_MENU || c == lcdifc::KEY_TEMPS) {
    lcdifc::cursor_off();
    this->cancel();
    return 1;
  }
  if (c == lcdifc::KEY_ENTER) {
    lcdifc::cursor_off();
    this->commit();
    return 1;
  }
  return 0;
}



void IntegerEntryScreen::commit()
{
}



void IntegerEntryScreen::cancel()
{
}





class ZeroMenuScreen: 
public Screen {
  virtual void draw() {
    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("1)Zero X   5)Home X "));
    lcdifc::write_P(PSTR("2)Zero Y   6)Home Y "));
    lcdifc::write_P(PSTR("3)Zero Z   7)Home Z "));
    lcdifc::write_P(PSTR("4)Zero All 8)HomeAll"));
  }



  void seek_x_home() {
    Point currpos = steppers::getPosition();
    if (lcdcfg::ENDSTOP_X_MIN_POS != lcdcfg::NO_ENDSTOP) {
      COMMAND_PUSH8(HOST_CMD_FIND_AXES_MINIMUM);
      COMMAND_PUSH8(0x1); // X axis only
      COMMAND_PUSH32(lcdcfg::JOG_STEP_DELAY_XY*3/2); // Step delay speed
      COMMAND_PUSH16(10); // seconds timeout
      setpos((long)(lcdcfg::ENDSTOP_X_MIN_POS*lcdcfg::STEPS_PER_MM_XY), currpos[1], currpos[2]);
    } else if (lcdcfg::ENDSTOP_X_MAX_POS != lcdcfg::NO_ENDSTOP) {
      COMMAND_PUSH8(HOST_CMD_FIND_AXES_MAXIMUM);
      COMMAND_PUSH8(0x1); // X axis only
      COMMAND_PUSH32(lcdcfg::JOG_STEP_DELAY_XY*3/2); // Step delay speed
      COMMAND_PUSH16(10); // seconds timeout
      setpos((long)(lcdcfg::ENDSTOP_X_MAX_POS*lcdcfg::STEPS_PER_MM_XY), currpos[1], currpos[2]);
    }
  }



  void seek_y_home() {
    Point currpos = steppers::getPosition();
    if (lcdcfg::ENDSTOP_Y_MIN_POS != lcdcfg::NO_ENDSTOP) {
      COMMAND_PUSH8(HOST_CMD_FIND_AXES_MINIMUM);
      COMMAND_PUSH8(0x2); // Y axis only
      COMMAND_PUSH32(lcdcfg::JOG_STEP_DELAY_XY*3/2); // Step delay speed
      COMMAND_PUSH16(10); // seconds timeout
      setpos(currpos[0], (long)(lcdcfg::ENDSTOP_Y_MIN_POS*lcdcfg::STEPS_PER_MM_XY), currpos[2]);
    } else if (lcdcfg::ENDSTOP_Y_MAX_POS != lcdcfg::NO_ENDSTOP) {
      COMMAND_PUSH8(HOST_CMD_FIND_AXES_MAXIMUM);
      COMMAND_PUSH8(0x2); // Y axis only
      COMMAND_PUSH32(lcdcfg::JOG_STEP_DELAY_XY*3/2); // Step delay speed
      COMMAND_PUSH16(10); // seconds timeout
      setpos(currpos[0], (long)(lcdcfg::ENDSTOP_Y_MAX_POS*lcdcfg::STEPS_PER_MM_XY), currpos[2]);
    }
  }



  void seek_z_home() {
    if (lcdcfg::ENDSTOP_Z_MIN_POS != lcdcfg::NO_ENDSTOP) {
      Point currpos = steppers::getPosition();
      COMMAND_PUSH8(HOST_CMD_FIND_AXES_MINIMUM);
      COMMAND_PUSH8(0x4); // Z axis only
      COMMAND_PUSH32(lcdcfg::JOG_STEP_DELAY_Z); // Step delay speed
      COMMAND_PUSH16(65); // seconds timeout
      setpos(currpos[0], currpos[1], (long)(lcdcfg::ENDSTOP_Z_MIN_POS*lcdcfg::STEPS_PER_MM_Z));
      moveto(currpos[0], currpos[1], (long)(lcdcfg::Z_AXIS_REST_POS*lcdcfg::STEPS_PER_MM_Z), lcdcfg::JOG_STEP_DELAY_Z);
    }
  }


  virtual uint8_t handleKey(char c) {
    Point currpos = steppers::getPosition();
    if (c >= '1' && c <= '8') {
      if (sdcard::isPlaying() || steppers::isRunning() || !command::isEmpty()) {
        return 0;
      }
    }
    switch (c) {
    case lcdifc::KEY_ZERO:
      Screen::change((Screen*)&default_screen);
      return 1;

    case '1':
      setpos(0, currpos[1], currpos[2]);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '2':
      setpos(currpos[0], 0, currpos[2]);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '3':
      setpos(currpos[0], currpos[1], 0);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '4':
      setpos(0, 0, 0);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '5':
      lcdifc::clear();
      lcdifc::write_P(PSTR("\nSeeking X Axis Home "));
      seek_x_home();
      moveto(0, currpos[1], currpos[2], lcdcfg::JOG_STEP_DELAY_XY);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '6':
      lcdifc::clear();
      lcdifc::write_P(PSTR("\nSeeking Y Axis Home "));
      seek_y_home();
      moveto(currpos[0], 0, currpos[2], lcdcfg::JOG_STEP_DELAY_XY);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '7':
      lcdifc::clear();
      lcdifc::write_P(PSTR("\nSeeking Z Axis Home "));
      seek_z_home();
      Screen::change((Screen*)&default_screen);
      return 1;

    case '8':
      lcdifc::clear();
      lcdifc::write_P(PSTR("\n    Seeking Home    "));
      seek_z_home();
      seek_x_home();
      seek_y_home();
      moveto(0, 0, (long)(lcdcfg::Z_AXIS_REST_POS*lcdcfg::STEPS_PER_MM_Z), lcdcfg::JOG_STEP_DELAY_XY);
      Screen::change((Screen*)&default_screen);
      return 1;

    case '9':
      lcdifc::init_to_flash();
      Screen::change((Screen*)&default_screen);
      return 1;

    case lcdifc::KEY_MENU:
      Screen::change((Screen*)&default_screen);
      return 1;

    default:
      break;
    }
    return 0;
  }
} 
zero_menu_screen;




class HeadTempScreen: 
public IntegerEntryScreen {
public:
  virtual void init() {
    uint16_t val = eeprom::getEeprom16(eeprom::HEAD_TEMP, lcdcfg::DEFAULT_HEAD_TEMP);
    this->setValue(val);
  }

  virtual void draw() {
    lcdifc::clear();
    lcdifc::write_P(PSTR("Extruder Target Temp"));
    IntegerEntryScreen::draw();
  }

  virtual void cancel() {
    Screen::change((Screen*)&temp_menu_screen);
  }

  virtual void commit() {
    eeprom::setEeprom16(eeprom::HEAD_TEMP, this->getValue());
    Screen::change((Screen*)&default_screen);
  }
} 
head_temp_screen;




class PlatformTempScreen: 
public IntegerEntryScreen {
public:
  virtual void init() {
    uint16_t val = eeprom::getEeprom16(eeprom::PLATFORM_TEMP, lcdcfg::DEFAULT_PLATFORM_TEMP);
    this->setValue(val);
  }

  virtual void draw() {
    lcdifc::clear();
    lcdifc::write_P(PSTR("Platform Target Temp"));
    IntegerEntryScreen::draw();
  }

  virtual void cancel() {
    Screen::change((Screen*)&temp_menu_screen);
  }

  virtual void commit() {
    eeprom::setEeprom16(eeprom::PLATFORM_TEMP, this->getValue());
    Screen::change((Screen*)&default_screen);
  }
} 
platform_temp_screen;




class TempMenuScreen: 
public Screen {
  virtual void draw() {
    uint16_t head_targ_temp = temps::getTargetHeadTemp();
    uint16_t plat_targ_temp = temps::getTargetPlatformTemp();
    uint16_t htemp = eeprom::getEeprom16(eeprom::HEAD_TEMP, lcdcfg::DEFAULT_HEAD_TEMP);
    uint16_t ptemp = eeprom::getEeprom16(eeprom::PLATFORM_TEMP, lcdcfg::DEFAULT_PLATFORM_TEMP);
    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("1) Extruder Heat "));
    if (head_targ_temp == htemp) {
      lcdifc::write_P(PSTR("Off"));
    } else {
      lcdifc::write_P(PSTR("On "));
    }
    lcdifc::write_P(PSTR("2) Platform Heat "));
    if (plat_targ_temp == ptemp) {
      lcdifc::write_P(PSTR("Off"));
    } else {
      lcdifc::write_P(PSTR("On "));
    }
    lcdifc::write_P(PSTR("3) Extruder Targ Tmp"));
    lcdifc::write_P(PSTR("4) Platform Targ Tmp"));
  }

  virtual uint8_t handleKey(char c) {
    uint16_t head_targ_temp = temps::getTargetHeadTemp();
    uint16_t plat_targ_temp = temps::getTargetPlatformTemp();
    int16_t temp;
    switch (c) {
    case '1':
      temp = eeprom::getEeprom16(eeprom::HEAD_TEMP, lcdcfg::DEFAULT_HEAD_TEMP);
      if (head_targ_temp != temp) {
        head_targ_temp = temp;
      } else {
        head_targ_temp = 0;
      }
      send_tool_simple_command_with_word(currentToolIndex, SLAVE_CMD_SET_TEMP, head_targ_temp);
      Screen::change((Screen*)&default_screen);
      return 1;
    case '2':
      temp = eeprom::getEeprom16(eeprom::PLATFORM_TEMP, lcdcfg::DEFAULT_PLATFORM_TEMP);
      if (plat_targ_temp != temp) {
        plat_targ_temp = temp;
      } else {
        plat_targ_temp = 0;
      }
      send_tool_simple_command_with_word(lcdcfg::HEATED_PLATFORM_TOOL, SLAVE_CMD_SET_PLATFORM_TEMP, plat_targ_temp);
      Screen::change((Screen*)&default_screen);
      return 1;
    case '3':
      Screen::change((Screen*)&head_temp_screen);
      return 1;
    case '4':
      Screen::change((Screen*)&platform_temp_screen);
      return 1;
    case lcdifc::KEY_MENU:
      Screen::change((Screen*)&default_screen);
      return 1;
    case lcdifc::KEY_TEMPS:
      Screen::change((Screen*)&default_screen);
      return 1;
    default:
      break;
    }
    return 0;
  }
} 
temp_menu_screen;




class FileSelectScreen: 
public Screen {
private:
  int8_t selected;
  int8_t offset;

public:
  virtual void init() {
    selected = 0;
    offset = 0;
  }


  virtual void draw() {
    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("Select a File:   "));
    lcdifc::write_char(lcdifc::UP_ARROW_CHAR);
    lcdifc::write_char(lcdifc::DOWN_ARROW_CHAR);
    lcdifc::write_char('\n');
    lcdifc::defer_next_command(33);
    redraw();
  }


  uint8_t getNextFile(char* fnbuf, uint8_t len) {
    while(1) {
      uint8_t rspCode = sdcard::directoryNextEntry(fnbuf,len);
      if (rspCode != 0 || !fnbuf[0]) {
        fnbuf[0] = '\0';
        return 0;
      }
      fnbuf[len-1] = '\0';
      if (fnbuf[0] == '.') {
        continue;
      }
      if (strcasesuffix_P(fnbuf, PSTR(".s3g"))) {
        break;
      }
    }
    return 1;
  }


  virtual void redraw() {
    char fnbuf[lcdcfg::MAX_FILENAME_SIZE];
    uint8_t i, index;
    uint8_t rspCode = sdcard::directoryReset();
    lcdifc::set_position(2,1);
    if (rspCode != 0) {
      lcdifc::write_P(PSTR("Can't Read SD Card"));
      return;
    }
    if (selected < 0) {
      selected = 0;
    }
    if (selected > offset+2) {
      if (selected < 2) {
        offset = 0;
      } 
      else {
        offset = selected - 2;
      }
    }
    if (selected < offset) {
      offset = selected;
    }
    index = 0;
    for (i = 0; i < offset; i++) {
      if (!getNextFile(fnbuf, sizeof(fnbuf))) {
        if (selected > index || offset > index) {
          selected = index;
          offset = index;
          Screen::setNeedsRedraw();
        }
        return;
      }
      index++;
    }
    for (i = 1; i <= 3; i++) {
      if (!getNextFile(fnbuf, sizeof(fnbuf))) {
        if (index <= selected) {
          selected = index-1;
          lcdifc::set_position(i,1);
          lcdifc::write_char(lcdifc::RIGHT_ARROW_CHAR);
          lcdifc::set_position(i+1,1);
        }
        if (selected < offset) {
          offset = selected;
          Screen::setNeedsRedraw();
        }
        break;
      }
      if (index == selected) {
        lcdifc::write_char(lcdifc::RIGHT_ARROW_CHAR);
      } 
      else {
        lcdifc::write_char(' ');
      }

      fnbuf[19] = '\0';
      uint8_t len = strlen(fnbuf);
      char* ptr = fnbuf+len;
      for (; len < 19; len++) {
        *ptr++ = ' ';
      }
      *ptr = '\0';
      lcdifc::write(fnbuf);
      index++;
    }
    for (; i <= 3; i++) {
      lcdifc::write_P(PSTR("                    "));
    }
  }


  void selectFile() {
    char fnbuf[lcdcfg::MAX_FILENAME_SIZE];
    uint8_t i, index;
    uint8_t rspCode = sdcard::directoryReset();
    if (rspCode != 0) {
      return;
    }
    index = 0;
    for (i = 0; i <= selected; i++) {
      if (!getNextFile(fnbuf, sizeof(fnbuf))) {
        selected = index;
        offset = index;
        Screen::setNeedsRedraw();
        return;
      }
      index++;
    }
    if (index > 0) {
      sdcard::startPlayback(fnbuf);
    }
  }


  virtual uint8_t handleKey(char c) {
    switch (c) {
    case lcdifc::KEY_ZMINUS:
      selected++;
      Screen::setNeedsRedraw();
      return 1;
    case lcdifc::KEY_ZPLUS:
      selected--;
      Screen::setNeedsRedraw();
      return 1;
    case lcdifc::KEY_MENU:
      Screen::change((Screen*)&main_menu_screen);
      return 1;
    case lcdifc::KEY_ENTER:
      if (sdcard::isPlaying()) { 
        return 0;
      }
      selectFile();
      Screen::change((Screen*)&default_screen);
      return 1;
    default:
      break;
    }
    return 0;
  }
} 
file_select_screen;



class LcdContrastScreen: 
public Screen {
private:
  uint8_t contrastval;
  uint8_t oldcontrastval;

public:
  LcdContrastScreen() {
  }

  virtual void init() {
    oldcontrastval = contrastval = eeprom::getEeprom8(eeprom::LCD_CONTRAST, lcdifc::MAX_CONTRAST);
    if (contrastval < 200) {
      contrastval = oldcontrastval = 255;
    }
    lcdifc::set_contrast(contrastval);
  }


  virtual void draw() {
    char buf[5];
    buf[0] = lcdifc::UP_ARROW_CHAR;
    buf[1] = lcdifc::DOWN_ARROW_CHAR;
    buf[2] = '\n';
    buf[3] = '\0';

    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("Adjust Contrast: "));
    lcdifc::write(buf);
    lcdifc::write_P(PSTR("\n<Enter> to save"));
    lcdifc::write_P(PSTR("\n<Menu> to cancel."));
  }


  virtual void redraw() {
  }


  virtual uint8_t handleKey(char c) {
    switch (c) {
    case lcdifc::KEY_ZMINUS:
      if (contrastval > 204) {
        contrastval -= 4;
      } else {
        contrastval = 200;
      }
      lcdifc::defer_next_command(6);
      lcdifc::set_contrast(contrastval);
      Screen::setNeedsRedraw();
      return 1;
    case lcdifc::KEY_ZPLUS:
      if (contrastval < 250) {
        contrastval += 4;
      } else {
        contrastval = 254;
      }
      lcdifc::defer_next_command(6);
      lcdifc::set_contrast(contrastval);
      Screen::setNeedsRedraw();
      return 1;
    case lcdifc::KEY_ENTER:
      eeprom::setEeprom8(eeprom::LCD_CONTRAST, contrastval);
      Screen::change((Screen*)&default_screen);
      return 1;
    case lcdifc::KEY_MENU:
      contrastval = oldcontrastval;
      lcdifc::set_contrast(contrastval);
      Screen::change((Screen*)&main_menu_screen);
      return 1;
    default:
      break;
    }
    return 0;
  }
} 
lcd_contrast_screen;



class MainMenuScreen: 
public Screen {
  virtual void draw() {
    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("1) Build File\n"));
    lcdifc::write_P(PSTR("2) Change Contrast\n"));
  }

  virtual uint8_t handleKey(char c) {
    switch (c) {
    case '1':
      if (sdcard::isPlaying()) {
        return 0;
      }
      Screen::change((Screen*)&file_select_screen);
      return 1;
    case '2':
      Screen::change((Screen*)&lcd_contrast_screen);
      return 1;
    case lcdifc::KEY_MENU:
      Screen::change((Screen*)&default_screen);
      return 1;
    default:
      break;
    }
    return 0;
  }
} 
main_menu_screen;



class HeadWarmupScreen: public Screen {
private:
  int8_t extruder_dir;

public:
  void setExtruderDir(int8_t val) {
      extruder_dir = val;
  }

  virtual void init() {
  }


  virtual uint8_t isUpdateNeeded() {
    if (hasElapsed()) {
      return 1;
    }
    return 0;
  }


  virtual void draw() {
    lcdifc::clear();
    lcdifc::cursor_off();
    lcdifc::write_P(PSTR("Warming Up Extruder."));
    lcdifc::write_P(PSTR("<Menu> to cancel.   \n"));

    uint16_t head_targ_temp = temps::getTargetHeadTemp();
    uint16_t eeprom_head_targ_temp = eeprom::getEeprom16(eeprom::HEAD_TEMP, lcdcfg::DEFAULT_HEAD_TEMP);
    if (head_targ_temp < eeprom_head_targ_temp) {
      send_tool_simple_command_with_word(currentToolIndex, SLAVE_CMD_SET_TEMP, head_targ_temp);
    }
  }

  virtual void redraw() {
    char buf[80];
    char chbuf[3];
    uint16_t head_temp = temps::getLastHeadTemp();
    uint16_t head_targ_temp = temps::getTargetHeadTemp();
    if (head_temp >= head_targ_temp - 3) {
      if (extruder_dir > 0) {
        send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_SET_MOTOR_1_PWM, 0xff);
        send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_TOGGLE_MOTOR_1, 0x3);
      } else if (extruder_dir < 0) {
        send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_SET_MOTOR_1_PWM, 0xff);
        send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_TOGGLE_MOTOR_1, 0x1);
      }
      Screen::change((Screen*)&default_screen);
      return;
    }

    lcdifc::set_position(4,1);
    buf[0] = '\0';
    strcat_P(buf,PSTR(" Temp:"));
    concat_decimal(buf, sizeof(buf), head_temp, 3, 0, 0);
    strcat_P(buf,PSTR("C "));
    chbuf[0] = lcdifc::RIGHT_ARROW_CHAR;
    chbuf[1] = ' ';
    chbuf[2] = '\0';
    strcat(buf,chbuf);
    concat_decimal(buf, sizeof(buf), head_targ_temp, 3, 0, 0);
    strcat_P(buf,PSTR("C "));
    lcdifc::write(buf);
  }

  virtual uint8_t handleKey(char c) {
    switch (c) {
    case lcdifc::KEY_TEMPS:
      return 1;
    case lcdifc::KEY_ZERO:
      return 1;
    case lcdifc::KEY_UNITS:
      return 1;
    case lcdifc::KEY_MENU:
      Screen::change((Screen*)&default_screen);
      return 1;
    default:
      break;
    }
    return 0;
  }
} 
head_warmup_screen;



class DefaultScreen: 
public Screen {
private:
  static int8_t jog_units;
  static long pos_target_x;
  static long pos_target_y;
  static long pos_target_z;
  static bool tophalf;

public:
  DefaultScreen() {
    Screen::change((Screen*)this);

    // Start the timer for the first time.
    startTimer(250000L);
  }


  virtual void init() {
  }


  virtual uint8_t isUpdateNeeded() {
    if (hasElapsed()) {
      return 1;
    }
    return 0;
  }


  virtual void draw() {
    lcdifc::cursor_off();
  }


  virtual void redraw() {
    char buf[82];
    char chbuf[2];
    millis_t now = Motherboard::getBoard().getCurrentMillis();
    Point currpos = steppers::getPosition();

    int32_t xmm, ymm, zmm;
    xmm = ((((uint32_t)currpos[0])*((uint32_t)((1<<9)*10.0/lcdcfg::STEPS_PER_MM_XY))>>8) + 1) >> 1;
    ymm = ((((uint32_t)currpos[1])*((uint32_t)((1<<9)*10.0/lcdcfg::STEPS_PER_MM_XY))>>8) + 1) >> 1;
    zmm = ((((uint32_t)currpos[2])*((uint32_t)((1<<12)*10.0/lcdcfg::STEPS_PER_MM_Z))>>11) + 1) >> 1;
    uint16_t head_temp = temps::getLastHeadTemp();
    uint16_t plat_temp = temps::getLastPlatformTemp();
    uint16_t head_targ_temp = temps::getTargetHeadTemp();
    uint16_t plat_targ_temp = temps::getTargetPlatformTemp();
    int8_t extdir = temps::getExtruderDir();

    if (sdcard::isPlaying()) {
      if (!sdcard::playbackHasNext()) {
        sdcard::finishPlayback();
      }

      int8_t  pcnt = sdcard::percentagePlayed();
      int16_t secs = sdcard::secondsPlayed();
      int16_t mins = secs / 60;
      int16_t hrs = mins / 60;
      secs %= 60;
      mins %= 60;

      // calculate visible space for filename.
      int8_t trimback = 20;
      trimback -= 1; // play/pause char.
      if (hrs > 0) {
        trimback -= 3; // Hours
      }
      trimback -= 3; // Minutes
      trimback -= 3; // Seconds
      trimback -= 4; // Percent

      const char* filename = sdcard::getPlaybackFilename();
      int8_t fnlen = strlen(filename);

      // If filename is larger than the visible space, scroll it.
      if (fnlen > trimback) {

        // Advance about one char/sec
        int8_t scrolloff = ((now >> 10) % ((fnlen+1-trimback)+2)) - 2;

        if (scrolloff < 0) {
          // Pause for a couple beats at the start of the name.
          scrolloff = 0;
        }
        while (scrolloff-->0)
          filename++;
      }

      // Playback Filename.
      strncpy(buf,filename,trimback);
      strcat_P(buf,PSTR("          "));
      buf[trimback] = '\0';

      // Playback % complete.
      concat_decimal(buf, sizeof(buf), pcnt, 3, 0, 0);
      strcat_P(buf,PSTR("% "));

      // Playback time elapsed.
      if (hrs > 0) {
        concat_decimal(buf, sizeof(buf), hrs,  2, 0, 0);
        strcat_P(buf,PSTR(":"));
      }
      concat_decimal(buf, sizeof(buf), mins, 2, 0, (hrs>0));
      strcat_P(buf,PSTR(":"));
      concat_decimal(buf, sizeof(buf), secs, 2, 0, 1);
      strcat_P(buf,PSTR("  "));

      buf[19] = lcdifc::PLAY_CHAR;
    } 
    else {
      strcpy_P(buf,PSTR("Ready...            "));
    }
    if (command::isPaused()) {
      // Blink pause icon.
      if (((now >> 9) & 0x1) == 0) {
        buf[19] = lcdifc::PAUSE_CHAR;
      } else {
        buf[19] = ' ';
      }
    }
    if (extdir < 0) {
      buf[19] = lcdifc::EXT_REV_CHAR; 
    }
    buf[20] = '\0';

    strcpy_P(buf+20,PSTR("X:"));
    concat_decimal(buf+20, sizeof(buf)-20, xmm, 5, 1, 0);
    strcat_P(buf+20,PSTR("   "));
    buf[29] = '\0';
    strcat_P(buf+29,PSTR("T:"));
    concat_decimal(buf+29, sizeof(buf), head_temp, 3, 0, 0);
    chbuf[0] = lcdifc::RIGHT_ARROW_CHAR;
    chbuf[1] = '\0';
    strcat(buf+29,chbuf);
    concat_decimal(buf+29, sizeof(buf)-29, head_targ_temp, 3, 0, 0);
    strcat_P(buf+29,PSTR("C "));
    if (extdir > 0) {
      buf[39] = lcdifc::EXT_FWD_CHAR; 
    } else {
      buf[39] = ' '; 
    }
    buf[40] = '\0';

    strcpy_P(buf+40,PSTR("Y:"));
    concat_decimal(buf+40, sizeof(buf)-40, ymm, 5, 1, 0);
    strcat_P(buf+40,PSTR("   "));
    buf[49] = '\0';
    strcat_P(buf+49,PSTR("P:"));
    concat_decimal(buf+49, sizeof(buf)-49, plat_temp, 3, 0, 0);
    chbuf[0] = lcdifc::RIGHT_ARROW_CHAR;
    chbuf[1] = '\0';
    strcat(buf+49,chbuf);
    concat_decimal(buf+49, sizeof(buf)-49, plat_targ_temp, 3, 0, 0);
    strcat_P(buf+49,PSTR("C "));
    buf[60] = '\0';

    strcpy_P(buf+60,PSTR("Z:"));
    concat_decimal(buf+60, sizeof(buf)-60, zmm, 5, 1, 0);
    strcat_P(buf+60,PSTR("   "));
    buf[69] = '\0';
    strcat_P(buf+69,PSTR("Jog:"));
    strcat_P(buf+69,jog_length_txt[jog_units]);
    strcat_P(buf+69,PSTR("   "));
    buf[80] = '\0';

    lcdifc::set_position(1,1);
    lcdifc::write(buf);
    //Screen::setNeedsRedraw();
    lcdifc::defer_next_command(90);
  }


  virtual uint8_t handleKey(char c) {
    if (c >= '1' && c <= '9') {
      if (sdcard::isPlaying()) { 
        return 0;
      }
      if (c != '3' && c !='5' && c != '7') {
        if (!steppers::isRunning() && command::isEmpty()) {
          Point currpos = steppers::getPosition();
          pos_target_x = currpos[0];
          pos_target_y = currpos[1];
          pos_target_z = currpos[2];
        }
      }
    }
    switch (c) {
    case lcdifc::KEY_YMINUS:
      // Y-
      pos_target_y -= jog_length_xy[jog_units];
      moveto(pos_target_x, pos_target_y, pos_target_z, lcdcfg::JOG_STEP_DELAY_XY);
      return 1;
    case lcdifc::KEY_ZMINUS:
      // Z-
      pos_target_z -= jog_length_z[jog_units];
      moveto(pos_target_x, pos_target_y, pos_target_z, lcdcfg::JOG_STEP_DELAY_Z);
      return 1;
    case lcdifc::KEY_EPLUS:
      // Extruder Forwards
      head_warmup_screen.setExtruderDir(1);
      Screen::change((Screen*)&head_warmup_screen);
      return 1;
    case lcdifc::KEY_XMINUS:
      // X-
      pos_target_x -= jog_length_xy[jog_units];
      moveto(pos_target_x, pos_target_y, pos_target_z, lcdcfg::JOG_STEP_DELAY_XY);
      return 1;
    case lcdifc::KEY_ESTOP:
      // Extruder Stop
      //send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_SET_MOTOR_1_PWM, 0x0);
      send_tool_simple_command_with_byte(currentToolIndex, SLAVE_CMD_TOGGLE_MOTOR_1, 0x0);
      Screen::setNeedsRedraw();
      return 1;
    case lcdifc::KEY_XPLUS:
      // X+
      pos_target_x += jog_length_xy[jog_units];
      moveto(pos_target_x, pos_target_y, pos_target_z, lcdcfg::JOG_STEP_DELAY_XY);
      return 1;
    case lcdifc::KEY_EMINUS:
      // Extruder reverse
      head_warmup_screen.setExtruderDir(-1);
      Screen::change((Screen*)&head_warmup_screen);
      return 1;
    case lcdifc::KEY_ZPLUS:
      // Z+
      pos_target_z += jog_length_z[jog_units];
      moveto(pos_target_x, pos_target_y, pos_target_z, lcdcfg::JOG_STEP_DELAY_Z);
      return 1;
    case lcdifc::KEY_YPLUS:
      // Y+
      pos_target_y += jog_length_xy[jog_units];
      moveto(pos_target_x, pos_target_y, pos_target_z, lcdcfg::JOG_STEP_DELAY_XY);
      return 1;
    case lcdifc::KEY_ZERO:
      // Zero
      Screen::change((Screen*)&zero_menu_screen);
      return 1;
    case lcdifc::KEY_UNITS:
      // Units.  Rotate through jog sizes
      if (jog_units == 0) {
        jog_units = sizeof(jog_length_xy)-1;
      } 
      else {
        jog_units--;
      }
      Screen::setNeedsRedraw();
      return 1;
    case lcdifc::KEY_TEMPS:
      // Temp
      Screen::change((Screen*)&temp_menu_screen);
      return 1;
    case lcdifc::KEY_MENU:
      // Menu
      Screen::change((Screen*)&main_menu_screen);
      return 1;
    case lcdifc::KEY_ENTER:
      // Enter.  Do nothing.
      return 1;
    default:
      break;
    }
    return 0;
  }
} 
default_screen;

int8_t DefaultScreen::jog_units = 3;
int32_t DefaultScreen::pos_target_x = 0;
int32_t DefaultScreen::pos_target_y = 0;
int32_t DefaultScreen::pos_target_z = 0;
bool DefaultScreen::tophalf = true;


#endif // HAS_MODTRONIX_LCD

// vim: set sw=2 autoindent nowrap expandtab: settings


