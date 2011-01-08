#ifndef GCODE_H
#define GCODE_H

#include <avr/pgmspace.h>

bool strcasesuffix_P(const char* str, PGM_P patstr);

namespace lcdcfg {

/****************************************************************************************
 * Specify tool# for heated build platform.
 ****************************************************************************************/
const uint8_t HEATED_PLATFORM_TOOL = 0;
const int DEFAULT_HEAD_TEMP = 210;
const int DEFAULT_PLATFORM_TEMP = 110;
const int MAX_FILENAME_SIZE = 32;

/****************************************************************************************
 * GCode Motion Configuration
 ****************************************************************************************/
#define BELFRY_FABBER_1 1
//#define CUPCAKE_CNC 1

const float NO_ENDSTOP = -999.0;

#ifdef BELFRY_FABBER_1
//const float STEPS_PER_MM_XY     =     7.874;    // For 10 tooth XL pulley.
//const float STEPS_PER_MM_XY     =    52.4936;   // For 15 tooth MXL pulley,  1/8 microsteppings.
const float STEPS_PER_MM_XY     =    13.1234;   // For 15 tooth MXL pulley, 1/2 microsteppings.
const float STEPS_PER_MM_Z      =   125.982;    // 3/8" - 8 ACME threaded rod direct drive
const int   MAX_MM_PER_MIN_XY   =  4500;
const int   MAX_MM_PER_MIN_Z    =   450;
const float ENDSTOP_X_MIN_POS   = NO_ENDSTOP;
const float ENDSTOP_X_MAX_POS   =    85.0;
const float ENDSTOP_Y_MIN_POS   = NO_ENDSTOP;
const float ENDSTOP_Y_MAX_POS   =    85.0;
const float ENDSTOP_Z_MIN_POS   =    0.0;
const float Z_AXIS_REST_POS     =    15.0;
#endif

#ifdef CUPCAKE_CNC
const float STEPS_PER_MM_XY     =    11.767463;
const float STEPS_PER_MM_Z      =   320.0;
const int   MAX_MM_PER_MIN_XY   =  5000;
const int   MAX_MM_PER_MIN_Z    =   150;
const float ENDSTOP_X_MIN_POS   =   -50.0;
const float ENDSTOP_X_MAX_POS   = NO_ENDSTOP;
const float ENDSTOP_Y_MIN_POS   = NO_ENDSTOP;
const float ENDSTOP_Y_MAX_POS   =    50.0;
const float ENDSTOP_Z_MIN_POS   =    0.0;
const float Z_AXIS_REST_POS     =    10.0;
#endif

const int   JOG_STEP_DELAY_XY   =  (int)(1.0e6/(STEPS_PER_MM_XY*0.7*MAX_MM_PER_MIN_XY/60.0));
const int   JOG_STEP_DELAY_Z    =  (int)(1.0e6/(STEPS_PER_MM_Z *0.7*MAX_MM_PER_MIN_Z /60.0));

} // namespace

#endif // GCODE_H
