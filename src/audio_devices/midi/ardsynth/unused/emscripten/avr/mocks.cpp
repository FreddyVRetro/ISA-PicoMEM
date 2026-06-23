/*    Mock AVR environment used when compiling to JavaScript with Emscripten.        (Only used by private tests and tools.)*/

#include "common.h"
#include "pgmspace.h"

uint8_t TCNT1L;
uint8_t TCNT1H;
uint8_t OCR0A;
uint8_t OCR0B;
uint8_t OCR1A;
uint8_t OCR1B;
uint8_t OCR2A;
uint8_t SPCR;
uint8_t TCCR2A;
uint8_t TCCR2B;
uint8_t UBRR0H;
uint8_t UBRR0L;
uint8_t UCSR0B;
uint8_t UCSR0C;
uint8_t GTCCR;
uint8_t TCCR1B;
uint8_t TCCR1A;
uint8_t PORTB;
uint8_t SPDR;
uint8_t SPSR = 1 << SPIF;     // Note: Initialized w/SPIF so that SPI wait loops will terminate.
uint8_t TIMSK2;

void cli() {}
void sei() {}

uint8_t pgm_read_byte(const volatile void* ptr) { return *(reinterpret_cast<const volatile uint8_t*>(ptr)); }
uint16_t pgm_read_word(const volatile void* ptr) { return *(reinterpret_cast<const volatile uint16_t*>(ptr)); }