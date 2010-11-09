#ifndef SCREENS_H
#define SCREENS_H

#include <avr/pgmspace.h>
#include "Timeout.hh"
#include "LCD.hh"
#include "Configuration.hh"


#if (HAS_MODTRONIX_LCD)

typedef enum {
  NONE,DRAW,REDRAW} 
ScreenRedraw_t;

class Screen {
private:
  static Screen* current_screen;
  static ScreenRedraw_t needs_update;

public:
  virtual void init();                 // Called before screen draw.
  virtual void draw();                 // To draw initial screen.
  virtual void redraw();               // To update an already drawn screen.
  virtual uint8_t handleKey(char c);   // To handle keypad entries.
  virtual uint8_t isUpdateNeeded();    // Lets subclass force early redraw.

  static bool hasElapsed();
  static void startTimer(micros_t timeout);

  static void change(Screen* scrn);
  static void update();
  static void processKey(char c);
  static void setNeedsDraw();
  static void setNeedsRedraw();
};


class IntegerEntryScreen: 
public Screen {
private:
  int16_t value;

public:
  virtual void init();
  virtual uint16_t getValue();
  virtual void setValue(int16_t val);
  virtual void draw();
  virtual void redraw();
  virtual uint8_t handleKey(char c);
  virtual void commit();
  virtual void cancel();
};


#endif // HAS_MODTRONIX_LCD


#endif // SCREENS_H

// vim: set sw=2 autoindent nowrap expandtab: settings


