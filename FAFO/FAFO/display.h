/*
 * display.h
 *
 * Created: 29/09/2026 6:03:25 pm
 *  Author: Student
 */ 


#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

// 7-segment unit patterns (0b DP G F E D C B A)
#define LETTER_V    0x3E  // 'U'/'V' shape (segments b,c,d,e,f)
#define LETTER_A    0x77  // 'A' (segments a,b,c,e,f,g)
#define LETTER_P    0x73  // 'P' for Power (segments a,b,e,f,g)
#define LETTER_DASH 0x40  // '-' (segment g only)
#define LETTER_NONE 0x00  // Blank

#define DP_NONE     0xFF  // pass as decimal_pos for no decimal point

void init_display(void);
void seperate_and_load_characters(uint16_t number, uint8_t decimal_pos);
void send_next_character_to_display(void);
void load_unit_character(uint8_t pattern);

// Show a 3-digit value (0-999) on Ds1-Ds3 and a unit pattern on Ds4.
// decimal_pos 0-2 lights the dp on that digit (e.g. 140 with dp 1 -> "14.0").
void display_value(uint16_t value, uint8_t decimal_pos, uint8_t unit);

// Show "----" (e.g. when there is no valid measurement)
void display_dashes(void);


#endif /* DISPLAY_H_ */

