#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_init(void);           
void adc_calibrate_vref(void);    // NEW: 1.1V bandgap calibration
void adc_start_capture(void);  
uint8_t adc_capture_done(void);   
float adc_get_vrms(void);       
float adc_get_ipeak(void);      
float adc_get_power(void);      

#endif