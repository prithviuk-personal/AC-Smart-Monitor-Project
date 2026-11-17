#include "common.h"
#include "adc.h"
#include <avr/interrupt.h>
#include <math.h>

// ---------- Exact Hardware Calibrations ----------
// Hardware Specifications
#define R3_OHMS     55600.0f
#define R4_OHMS     2670.0f
#define R12_OHMS    27190.0f
#define R13_OHMS    11990.0f
#define SHUNT_OHMS  0.6622641509f

// V_Scale is the ratio of the voltage divider resistors
#define V_SCALE     ((R3_OHMS + R4_OHMS) / R4_OHMS)
//I_Scale is the ratio of the shunt and the current divider resistors
#define I_SCALE     (1.0f / ((R12_OHMS / R13_OHMS) * SHUNT_OHMS))

// Sampling variable configuration
#define N_SAMPLES_PER_CYCLE 10  // 10 samples per 500Hz wave (100us intervals)
#define TOTAL_CYCLES        4   // 4 shifted waves for Equivalent Time Sampling
#define N_SAMPLES           (N_SAMPLES_PER_CYCLE * TOTAL_CYCLES) // 40 total samples

//Defining the MUX values for the voltage and current channels
#define MUX_V  0b01000000  // AVCC reference, ADC0 (PC0)
#define MUX_I  0b01000001  // AVCC reference, ADC1 (PC1)

//Variable Initialisations
static uint16_t v_samples[N_SAMPLES]; // Array to hold the 40 voltage samples
static uint16_t i_samples[N_SAMPLES]; // Array to hold the 40 current samples
static volatile uint8_t v_count, i_count;  // Counters for the number of samples taken for voltage and current
static volatile uint8_t cycle_count;          // Tracks ETS phase shift
static volatile uint8_t reading_voltage;   // Flag to indicate whether the next ADC reading is for voltage or current    
static volatile uint8_t done; // Flag to indicate when all samples have been captured

//Dynamic Variable Initialisation
static volatile float actual_vref = 5.0f;            // Dynamically updated via bandgap

/*NOTE: The way the real voltage is measured is done using the */

// ---------- 1.1V Bandgap Calibration ----------
void adc_calibrate_vref(void)
{
	// 1. Save the current multiplexer and interrupt states
	uint8_t saved_admux = ADMUX;
	uint8_t saved_adcsra = ADCSRA;
	
	// 2. Temporarily DISABLE ADC interrupts and Auto-Triggering
	// so the ISR cannot hijack our manual calibration readings
	ADCSRA &= ~((1 << ADIE) | (1 << ADATE));
	
	// Select AVCC as ref (REFS0=1) and 1.1V bandgap as input (MUX3..1=1)
	ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);
	
	_delay_ms(2); // Wait for bandgap voltage to stabilize

	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	(void)ADC; // Discard first unreliable conversion

	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	uint16_t adc_reading = ADC;

	// 3. Restore the multiplexer channel
	ADMUX = saved_admux;
	
	// 4. Clear any pending interrupt flags by writing a 1 to ADIF.
	// If we don't do this, the ISR will instantly fire the moment we restore ADCSRA.
	ADCSRA |= (1 << ADIF);
	
	// 5. Safely restore ADC interrupts and Auto-Triggering
	ADCSRA = saved_adcsra;

	if (adc_reading == 0) {
		actual_vref = 5.0f;
		} else {
		actual_vref = (5120f / (float)adc_reading); //Boards manafactured Bandgap is 1.093V, hence 1.903 * 1024 for reference back calculation for Avcc
	}
}
// ---------- Initialization & Control ----------
void adc_init(void)
{
    // Timer0: CTC mode, TOP = 24.
    // At 2 MHz F_CPU, prescaler 8 -> 250 kHz (4 us). 25 ticks = 100 us exactly.
    TCCR0A = (1 << WGM01);
    TCCR0B = 0;
    OCR0A  = 24;
    TIMSK0 = (1 << OCIE0A); 

    // ADC: prescaler 8 -> 250 kHz. Conversion takes 13 clocks = 52 us.
    ADMUX  = MUX_V;
    DIDR0  = (1 << ADC0D) | (1 << ADC1D);
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
    
    ADCSRA |= (1 << ADSC);                  
    while (ADCSRA & (1 << ADSC));           // First conversion is slow, do it now
    
    ADCSRB = (1 << ADTS1) | (1 << ADTS0);   // Trigger source: Timer0 compare match A
    ADCSRA |= (1 << ADATE) | (1 << ADIE) | (1 << ADIF); 

    // INT0 on PD2 (Vzc), rising edge. Enabled by adc_start_capture().
    EICRA = (1 << ISC01) | (1 << ISC00);
}

void adc_start_capture(void)
{
    TCCR0B = 0;
    v_count = 0;
    i_count = 0;
    cycle_count = 0;
    done = 0;
    EIFR   = (1 << INTF0);    
    EIMSK |= (1 << INT0);
}

uint8_t adc_capture_done(void)
{
    return done;
}

// ---------- Interrupt Service Routines ----------
ISR(INT0_vect)
{
    EIMSK &= ~(1 << INT0);    // Disable ZCD to ignore chatter

    // Equivalent Time Sampling: Slip the phase by 50us (9 degrees) per cycle
    // _delay_us() uses compile-time constants for strict timing accuracy
    if (cycle_count == 1) {
        _delay_us(50);
    } else if (cycle_count == 2) {
        _delay_us(100);
    } else if (cycle_count == 3) {
        _delay_us(150);
    }

    reading_voltage = 1;      
    ADMUX  = MUX_V;
    ADCSRA |= (1 << ADSC);    // Take the zero-degree (or phase-slipped) sample
    
    TCNT0  = 0;               
    TIFR0  = (1 << OCF0A);
    TCCR0B = (1 << CS01);     // Start Timer0 (clock/8) for the remaining 9 samples
}

EMPTY_INTERRUPT(TIMER0_COMPA_vect); 

ISR(ADC_vect)
{
    if (reading_voltage) {
        v_samples[v_count++] = ADC;
        ADMUX = MUX_I;
    } else {
        i_samples[i_count++] = ADC;
        ADMUX = MUX_V;
    }
    reading_voltage = !reading_voltage;

    // Check if we finished the 10 samples for the CURRENT phase-shifted cycle
    uint8_t expected_samples_this_cycle = (cycle_count + 1) * N_SAMPLES_PER_CYCLE;
    
    if (i_count == expected_samples_this_cycle) { 
        TCCR0B = 0;               // Stop Timer0 
        cycle_count++;            

        if (cycle_count == TOTAL_CYCLES) {
            done = 1;             // All 40 samples successfully interleaved
        } else {
            EIFR  = (1 << INTF0); 
            EIMSK |= (1 << INT0); // Re-arm ZCD to catch the next AC wave
        }
    }
}

// ---------- Processing Calculations ----------
float adc_get_vrms(void)
{
    // 1. Find dynamic DC offset
    float dc_offset = 0;
    for (uint8_t k = 0; k < N_SAMPLES; k++) {
        dc_offset += v_samples[k];
    }
    dc_offset /= N_SAMPLES;
    //Addind all the values and then dividing by N_Samples to give real offset

    // 2. Center wave and square
    //sum_sq is the sum of the squares of the centered voltage samples
    float sum_sq = 0;
    for (uint8_t k = 0; k < N_SAMPLES; k++) {
        float centered_adc = v_samples[k] - dc_offset;
        float pin_voltage = (centered_adc * actual_vref) / 1024.0f;
        sum_sq += (pin_voltage * pin_voltage);
    }

    // 3. Root and scale to true Mains Voltage
    return sqrtf(sum_sq / N_SAMPLES) * V_SCALE;
}

float adc_get_ipeak(void)
{
    // 1. Find dynamic DC offset
    float dc_offset = 0;
    for (uint8_t k = 0; k < N_SAMPLES; k++) {
        dc_offset += i_samples[k];
    }
    dc_offset /= N_SAMPLES;

    // 2. Find absolute max
    float max_centered_adc = 0;
    for (uint8_t k = 0; k < N_SAMPLES; k++) {
        float centered_adc = i_samples[k] - dc_offset;
        if (centered_adc < 0) centered_adc = -centered_adc; 
        
        if (centered_adc > max_centered_adc) {
            max_centered_adc = centered_adc;
        }
    }

    // 3. Convert max count to true Amps
    float peak_pin_voltage = (max_centered_adc * actual_vref) / 1024.0f;
    
    // Returning standard Amps here because main.c already handles the * 1000 multiplier
    return peak_pin_voltage * I_SCALE; 
}

float adc_get_power(void)
{
    // 1. Calculate dynamic DC offsets for both channels
    float v_offset = 0, i_offset = 0;
    for (uint8_t k = 0; k < N_SAMPLES; k++) {
        v_offset += v_samples[k];
        i_offset += i_samples[k];
    }
    v_offset /= N_SAMPLES;
    i_offset /= N_SAMPLES;

    // 2. Center and scale all samples to pure real-world values
    float centered_v[N_SAMPLES];
    float centered_i[N_SAMPLES];
    for (uint8_t k = 0; k < N_SAMPLES; k++) {
        centered_v[k] = ((v_samples[k] - v_offset) * actual_vref / 1024.0f) * V_SCALE;
        centered_i[k] = ((i_samples[k] - i_offset) * actual_vref / 1024.0f) * I_SCALE;
    }

    // 3. Integrate real power by multiplying V * I with linear phase interpolation
    float sum_power = 0;
    for (uint8_t cycle = 0; cycle < TOTAL_CYCLES; cycle++) {
        uint8_t base = cycle * N_SAMPLES_PER_CYCLE;
        for (uint8_t k = 0; k < N_SAMPLES_PER_CYCLE; k++) {
            uint8_t next = (k + 1) % N_SAMPLES_PER_CYCLE; 
            
            // Interpolate voltage to align perfectly with the delayed current sample time
            float v_interp = (centered_v[base + k] + centered_v[base + next]) * 0.5f;
            sum_power += (v_interp * centered_i[base + k]);
        }
    }
    
    // 4. Return the average Real Power (Watts)
    return sum_power / N_SAMPLES;
}