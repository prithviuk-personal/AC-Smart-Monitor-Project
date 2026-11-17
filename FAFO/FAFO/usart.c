/*
 * usart.c - Polled USART0 transmit (PD1/TXD), 8N1, no parity
 *
 * At 9600 baud one character takes ~1 ms, so a ~40 character line takes
 * ~40 ms. Fine here because we only send once per second.
 */
#ifndef F_CPU
#define F_CPU 2000000UL
#endif

#include <avr/io.h>
#include "usart.h"

void usart_init(uint32_t baud)
{
    uint16_t ubrr = F_CPU / (16 * baud) - 1;   // 2 MHz, 9600 baud -> 12 (actual 9615, 0.2% error)
    UBRR0H = ubrr >> 8;
    UBRR0L = ubrr;
    UCSR0B = (1 << TXEN0);                     // enable transmitter
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    // 8 data bits, no parity, 1 stop bit
}

void usart_send_char(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));          // wait until the transmit buffer is empty
    UDR0 = c;
}

void usart_send_string(const char *s)
{
    while (*s) {
        usart_send_char(*s++);
    }
}

// Sends an unsigned number with a decimal point inserted 'decimals' digits from the right.
// usart_send_number(1403, 2) -> "14.03", usart_send_number(512, 0) -> "512", (5, 2) -> "0.05"
void usart_send_number(uint16_t value, uint8_t decimals)
{
    char digits[5];
    uint8_t n = 0;

    // Pull digits off the right-hand end; always produce at least decimals + 1 digits
    do {
        digits[n++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0 || n <= decimals);

    // Send them back in the right order, adding the decimal point where needed
    while (n > 0) {
        if (n == decimals) {
            usart_send_char('.');
        }
        usart_send_char(digits[--n]);
    }
}