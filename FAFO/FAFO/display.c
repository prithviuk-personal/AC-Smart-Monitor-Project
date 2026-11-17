#include "display.h"
#include "common.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// Shift register pins (PORTC)
#define SH_CP PC3
#define SH_DS PC4
#define SH_ST PC5

// Digit enable pins (PORTD)
#define DS1 PD4
#define DS2 PD5
#define DS3 PD6
#define DS4 PD7

const uint8_t seg_pattern[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};

static volatile uint8_t disp_characters[4]={0,0,0,0};
static volatile uint8_t disp_position=0;

void init_display(void){
	// all digits off (logic 1 = off) before the pins become outputs, so nothing flashes at start-up
	PORTD |= (1<<DS1) | (1<<DS2) | (1<<DS3) | (1<<DS4);

	DDRC |= (1<<SH_CP) | (1<<SH_DS) | (1<<SH_ST);
	DDRD |= (1<<DS1) | (1<<DS2) | (1<<DS3) | (1<<DS4);

	// Timer2 in CTC mode: interrupt every ~1 ms, each interrupt shows the next digit
	// 2 MHz / 64 = 31.25 kHz -> 31 counts (OCR2A = 30) = 0.992 ms per digit
	
	TCCR2A = (1<<WGM21);   // CTC, TOP = OCR2A
	TCCR2B = (1<<CS22);    // prescaler 64 (Timer2: CS22 alone = /64)
	OCR2A  = 30;
	TIMSK2 = (1<<OCIE2A);  // enable compare match A interrupt (needs sei() in main)
}

// ISR_NOBLOCK re-enables interrupts on entry, so the ADC interrupt can still
// run on time while the digit is being shifted out
ISR(TIMER2_COMPA_vect, ISR_NOBLOCK){
	send_next_character_to_display();
}

void seperate_and_load_characters(uint16_t number, uint8_t decimal_pos){
	uint8_t thousands = number / 1000;
	uint8_t hundreds = (number / 100) % 10;
	uint8_t tens = (number / 10) % 10;
	uint8_t ones = number % 10;

	// disp_characters[0] is Ds1 (leftmost)
	disp_characters[0] = seg_pattern[thousands];
	disp_characters[1] = seg_pattern[hundreds];
	disp_characters[2] = seg_pattern[tens];
	disp_characters[3] = seg_pattern[ones];

	// decimal_pos 0-3 lights the dp on that digit; anything else = no dp
	if (decimal_pos < 4) {
		disp_characters[decimal_pos] |= 0x80;
	}
}

void display_value(uint16_t value, uint8_t decimal_pos, uint8_t unit){
	if (value > 999) {
		value = 999;   // only 3 digits available
	}

	uint8_t p[3];
	p[0] = seg_pattern[value / 100];
	p[1] = seg_pattern[(value / 10) % 10];
	p[2] = seg_pattern[value % 10];

	// blank leading zeros, but never the digit carrying the dp ("0.95" stays "0.95")
	if (value < 100 && decimal_pos > 0) {
		p[0] = LETTER_NONE;
		if (value < 10 && decimal_pos > 1) {
			p[1] = LETTER_NONE;
		}
	}

	if (decimal_pos < 3) {
		p[decimal_pos] |= 0x80;
	}

	disp_characters[0] = p[0];
	disp_characters[1] = p[1];
	disp_characters[2] = p[2];
	disp_characters[3] = unit;
}

void display_dashes(void){
	for (uint8_t i = 0; i < 4; i++) {
		disp_characters[i] = LETTER_DASH;
	}
}

void send_next_character_to_display(void){
	uint8_t pattern = disp_characters[disp_position];

	// clocks low
	PORTC &= ~((1<<SH_CP) | (1<<SH_ST));

	// shift out MSB first (dp first, a last)
	for (int8_t bit = 7; bit >= 0; bit--) {
		uint8_t bit_value = (pattern >> bit) & 1;
		if (bit_value) {
			PORTC |= (1<<SH_DS);
			} else {
			PORTC &= ~(1<<SH_DS);
		}
		PORTC |= (1<<SH_CP);
		PORTC &= ~(1<<SH_CP);
	}

	// disable all digits (logic 1 = off)
	PORTD |= (1<<DS1) | (1<<DS2) | (1<<DS3) | (1<<DS4);

	// latch
	PORTC |= (1<<SH_ST);
	PORTC &= ~(1<<SH_ST);

	// enable the correct digit (logic 0 = on)
	switch (disp_position) {
		case 0: PORTD &= ~(1<<DS1); break;
		case 1: PORTD &= ~(1<<DS2); break;
		case 2: PORTD &= ~(1<<DS3); break;
		case 3: PORTD &= ~(1<<DS4); break;
	}

	// next digit
	disp_position++;
	if (disp_position > 3) {
		disp_position = 0;
	}
}

void load_unit_character(uint8_t pattern){
	disp_characters[3] = pattern;
}