#ifndef GCODE_H
#define GCODE_H

#include <avr/pgmspace.h>

bool strcasesuffix_P(const char* str, PGM_P patstr);

namespace lcdcfg {

/****************************************************************************************
 * Specify tool# for heated build platform.
 ****************************************************************************************/
const uint8_t HEATED_PLATFORM_TOOL = 0;
const int DEFAULT_HEAD_TEMP = 220;
const int DEFAULT_PLATFORM_TEMP = 110;
const int MAX_FILENAME_SIZE = 32;

/****************************************************************************************
 * GCode Motion Configuration
 ****************************************************************************************/
#define BELFRY_FABBER_1 1
//#define CUPCAKE_CNC 1

const float NO_ENDSTOP = -999.0;

#ifdef BELFRY_FABBER_1
//const float STEPS_PER_MM_XY    =     7.874;    // For 10 tooth XL pulley.
const float STEPS_PER_MM_XY     =    13.1234;   // For 15 tooth MXL pulley.
const float STEPS_PER_MM_Z      =   302.3622;   // 3/8" - 8 ACME threaded rod
const int   MAX_MM_PER_MIN_XY   =  3600;
const int   MAX_MM_PER_MIN_Z    =   180;
const int   JOG_STEP_DELAY_XY   =  1600;  // microseconds
const int   JOG_STEP_DELAY_Z    =  1000;  // microseconds
const float ENDSTOP_X_MIN_POS   = NO_ENDSTOP;
const float ENDSTOP_X_MAX_POS   =    85.0;
const float ENDSTOP_Y_MIN_POS   = NO_ENDSTOP;
const float ENDSTOP_Y_MAX_POS   =    85.0;
const float ENDSTOP_Z_MIN_POS   =    0.0;
const float Z_AXIS_REST_POS     =    10.0;
#endif

#ifdef CUPCAKE_CNC
const float STEPS_PER_MM_XY     =    11.767463;
const float STEPS_PER_MM_Z      =   320.0;
const int   MAX_MM_PER_MIN_XY   =  5000;
const int   MAX_MM_PER_MIN_Z    =   150;
const int   JOG_STEP_DELAY_XY   =  1700;  // microseconds
const int   JOG_STEP_DELAY_Z    =  1800;  // microseconds
const float ENDSTOP_X_MIN_POS   =   -50.0;
const float ENDSTOP_X_MAX_POS   = NO_ENDSTOP;
const float ENDSTOP_Y_MIN_POS   = NO_ENDSTOP;
const float ENDSTOP_Y_MAX_POS   =    50.0;
const float ENDSTOP_Z_MIN_POS   =    0.0;
const float Z_AXIS_REST_POS     =    10.0;
#endif

} // namespace

#endif // GCODE_H
