#ifndef GCODE_H
#define GCODE_H

bool is_playing_gcode();
void gcode_run_slice();
bool gcode_wants_data();
void gcode_process_byte(uint8_t ch);

#endif // GCODE_H
