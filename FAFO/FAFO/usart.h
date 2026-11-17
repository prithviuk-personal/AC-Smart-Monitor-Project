#ifndef USART_H
#define USART_H

#include <stdint.h>

void usart_init(uint32_t baud);                          // 8N1, no parity, transmit only
void usart_send_char(char c);
void usart_send_string(const char *s);
void usart_send_number(uint16_t value, uint8_t decimals); // e.g. (1403, 2) sends "14.03"

#endif