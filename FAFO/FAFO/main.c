#include "common.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usart.h"
#include "adc.h"
#include "display.h"

int main(void)
{
	// 1. Hardware Initialization
	usart_init(9600);
	init_display();
	adc_init();
	
	// 2. Initial baseline AVCC calibration before interrupts start
	adc_calibrate_vref();
	
	sei(); // Enable global interrupts
	
	uint8_t display_item = 0; // Tracks whether V, A, or P is on the 7-segment
	
	usart_send_string("--- Electeng 209 Energy Monitor Booting ---\r\n");
	
	while (1) {
		// Start the 4-cycle Equivalent Time Sampling
		adc_start_capture();
		
		// Wait 20ms. 4 ETS waves at 500 Hz take exactly 8 ms.
		_delay_ms(20);

		if (adc_capture_done()) {
			// Calculate real-world physics
			uint16_t vrms_x100 = adc_get_vrms() * 100 + 0.5f;
			uint16_t ipeak_mA  = adc_get_ipeak() * 1000 + 0.5f;

			float power = adc_get_power();
			if (power < 0) power = 0; // Prevent negative wrap-around on cast
			uint16_t power_x100 = power * 100 + 0.5f;
			
			// --- 7-Segment Display Update ---
			switch (display_item) {
				case 0: display_value((vrms_x100 + 5) / 10, 1, LETTER_V); break;  // 14.03 V -> "14.0V"
				case 1: display_value((ipeak_mA + 5) / 10, 0, LETTER_A);  break;  // 754 mA  -> "0.75A"
				case 2: display_value(power_x100, 0, LETTER_P);           break;  // 7.43 W  -> "7.43P"
			}
			
			display_item++;
			if (display_item > 2) {
				display_item = 0;
			}

			// --- Putty Terminal Update ---
			usart_send_string("Vrms: ");
			usart_send_number(vrms_x100, 2);
			usart_send_string(" V  |  Ipeak: ");
			usart_send_number(ipeak_mA, 0);
			usart_send_string(" mA  |  Power: ");
			usart_send_number(power_x100, 2);
			usart_send_string(" W\r\n");
			
			// Safely recalibrate AVCC to account for thermal drift while hardware is idle.
			adc_calibrate_vref();
			
			} else {
			// Display dashes on the board and send an error to Putty
			display_dashes();
			usart_send_string("ERROR: No zero crossings detected. Is the 14V AC source connected?\r\n");
		}

		// Project spec dictates exactly 1 second scroll rate for display information.
		// Timer2 keeps the 7-segment lit in the background while the main loop waits.
		_delay_ms(1000);
	}
}