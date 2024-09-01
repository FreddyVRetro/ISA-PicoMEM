/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
//https://smittytone.net/docs/ht16k33_segment14.html

// Modified for Pi Pico by FreddyV

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"

// How many digits are on our display.
#define NUM_DIGITS 4

extern int pm_qwiic_writeb(uint8_t addr, uint8_t val);
extern int pm_qwiic_write(uint8_t addr,uint8_t *buf,uint8_t size);

// By default these display drivers are on bus address 0x70. Often there are
// solder on options on the PCB of the backpack to set an address between
// 0x70 and 0x77 to allow multiple devices to be used.
const int I2C_addr = 0x70;

uint8_t displayRAM[17];  // 0 + 16 RAM Address
uint8_t digitPosition = 0;

// commands
#define HT16K33_SYSTEM_STANDBY  0x20
#define HT16K33_SYSTEM_RUN      0x21
#define HT16K33_SET_ROW_INT     0xA0
#define HT16K33_BRIGHTNESS      0xE0

// Display on/off/blink
#define HT16K33_DISPLAY_SETUP   0x80

// OR/clear these to display setup register
#define HT16K33_DISPLAY_OFF     0x0
#define HT16K33_DISPLAY_ON      0x1
#define HT16K33_BLINK_2HZ       0x2
#define HT16K33_BLINK_1HZ       0x4
#define HT16K33_BLINK_0p5HZ     0x6

/*--------------------------- Character Map ----------------------------------*/
#define SFE_ALPHANUM_UNKNOWN_CHAR 95

static const uint16_t alphanumeric_segs[96]{
	// nmlkjihgfedcba
	0b00000000000000, // ' ' (space)
	0b00001000001000, // '!'
	0b00001000000010, // '"'
 	0b01001101001110, // '#'
	0b01001101101101, // '$'
	0b10010000100100, // '%'
	0b00110011011001, // '&'
	0b00001000000000, // '''
	0b00000000111001, // '('
	0b00000000001111, // ')'
	0b11111010000000, // '*'
	0b01001101000000, // '+'
	0b10000000000000, // ','
	0b00000101000000, // '-'
	0b00000000000000, // '.'
	0b10010000000000, // '/'
	0b00000000111111, // '0'
	0b00010000000110, // '1'
	0b00000101011011, // '2'
	0b00000101001111, // '3'
	0b00000101100110, // '4'
	0b00000101101101, // '5'
	0b00000101111101, // '6'
	0b01010000000001, // '7'
	0b00000101111111, // '8'
	0b00000101100111, // '9'
	0b00000000000000, // ':'
	0b10001000000000, // ';'
	0b00110000000000, // '<'
	0b00000101001000, // '='
	0b01000010000000, // '>'
    0b01000100000011, // '?'
	0b00001100111011, // '@'
	0b00000101110111, // 'A'
	0b01001100001111, // 'B'
	0b00000000111001, // 'C'
	0b01001000001111, // 'D'
	0b00000101111001, // 'E'
	0b00000101110001, // 'F'
	0b00000100111101, // 'G'
	0b00000101110110, // 'H'
	0b01001000001001, // 'I'
	0b00000000011110, // 'J'
	0b00110001110000, // 'K'
	0b00000000111000, // 'L'
	0b00010010110110, // 'M'
	0b00100010110110, // 'N'
	0b00000000111111, // 'O'
	0b00000101110011, // 'P'
	0b00100000111111, // 'Q'
	0b00100101110011, // 'R'
	0b00000110001101, // 'S'
	0b01001000000001, // 'T'
	0b00000000111110, // 'U'
	0b10010000110000, // 'V'
	0b10100000110110, // 'W'
	0b10110010000000, // 'X'
	0b01010010000000, // 'Y'
	0b10010000001001, // 'Z'
	0b00000000111001, // '['
	0b00100010000000, // '\'
	0b00000000001111, // ']'
    0b10100000000000, // '^'
	0b00000000001000, // '_'
	0b00000010000000, // '`'
	0b00000101011111, // 'a'
	0b00100001111000, // 'b'
	0b00000101011000, // 'c'
	0b10000100001110, // 'd'
	0b00000001111001, // 'e'
	0b00000001110001, // 'f'
	0b00000110001111, // 'g'
	0b00000101110100, // 'h'
	0b01000000000000, // 'i'
	0b00000000001110, // 'j'
	0b01111000000000, // 'k'
	0b01001000000000, // 'l'
	0b01000101010100, // 'm'
	0b00100001010000, // 'n'
	0b00000101011100, // 'o'
	0b00010001110001, // 'p'
	0b00100101100011, // 'q'
	0b00000001010000, // 'r'
	0b00000110001101, // 's'
	0b00000001111000, // 't'
	0b00000000011100, // 'u'
	0b10000000010000, // 'v'
	0b10100000010100, // 'w'
	0b10110010000000, // 'x'
	0b00001100001110, // 'y'
	0b10010000001001, // 'z'
	0b10000011001001, // '{'
	0b01001000000000, // '|'
	0b00110100001001, // '}'
	0b00000101010010, // '~'
	0b11111111111111, // Unknown character (DEL or RUBOUT)
};


void ht16k33_init() {
    pm_qwiic_writeb(I2C_addr,HT16K33_SYSTEM_RUN);
    pm_qwiic_writeb(I2C_addr,HT16K33_SET_ROW_INT);
    pm_qwiic_writeb(I2C_addr,HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON);
}


// Get the character map from the default table
uint16_t getSegmentsToTurnOn(uint8_t charPos)
{
  return alphanumeric_segs[charPos];
}

/*---------------------------- Light up functions ---------------------------------*/

// Given a segment and a digit, set the matching bit within the RAM of the Holtek RAM set
void illuminateSegment(uint8_t segment, uint8_t digit)
{
	uint8_t com;
	uint8_t row;

	com = segment - 'A'; // Convert the segment letter back to a number

	if (com > 6)
		com -= 7;
	if (segment == 'I')
		com = 0;
	if (segment == 'H')
		com = 1;

	row = digit % 4; // Convert digit (1 to 16) back to a relative position on a given digit on display
	if (segment > 'G')
		row += 4;

	uint8_t offset = digit / 4 * 16;
	uint8_t adr = com * 2 + offset;

	// Determine the address
	if (row > 7)
		adr++;

	// Determine the data bit
	if (row > 7)
		row -= 8;
	uint8_t dat = 1 << row;

	displayRAM[adr+1] = displayRAM[adr+1] | dat;
}

// Given a binary set of segments and a digit, store this data into the RAM array
void illuminateChar(uint16_t segmentsToTurnOn, uint8_t digit)
{
	for (uint8_t i = 0; i < 14; i++) // There are 14 segments on this display
	{
		if ((segmentsToTurnOn >> i) & 0b1)
			illuminateSegment('A' + i, digit); // Convert the segment number to a letter
	}
}

// Push the contents of displayRAM out to the various displays in 16 byte chunks
bool updateDisplay()
{
    //for (int i=0;i<17;i++) printf("%x",displayRAM[i]);
	return pm_qwiic_write(I2C_addr,displayRAM,17);
}

// Print a character, for a given digit, on display
void printChar(uint8_t displayChar, uint8_t digit)
{
	// moved alphanumeric_segs array to PROGMEM
	uint16_t characterPosition = 65532;

	// space
	if (displayChar == ' ')
		characterPosition = 0;
	// Printable Symbols -- Between first character ! and last character ~
	else if (displayChar >= '!' && displayChar <= '~')
	{
		characterPosition = displayChar - '!' + 1;
	}

	uint8_t dispNum = digitPosition / 4;

	// Take care of special characters by turning correct segment on
//	if (characterPosition == 14) // '.'
//		decimalOnSingle(dispNum+1, false);
//	if (characterPosition == 26) // ':'
//		colonOnSingle(dispNum+1, false);
	if (characterPosition == 65532) // unknown character
		characterPosition = SFE_ALPHANUM_UNKNOWN_CHAR;

	uint16_t segmentsToTurnOn = alphanumeric_segs[characterPosition];

	illuminateChar(segmentsToTurnOn, digit);
}

static inline void ht16k33_display_char(int position, char ch) 
{
   //printf ("%d,%c ",position,ch);
   printChar(ch,position);
}
    
void ht16k33_display_string(const char *str) {
    int digit = 0;
	
    for (uint8_t i = 0; i < 17; i++)
		displayRAM[i] = 0;

    while (*str && digit <= NUM_DIGITS) {
        ht16k33_display_char(digit++, *str++);
    }
    updateDisplay();
}

void ht16k33_scroll_string(const char *str, int interval_ms) {
    int l = strlen(str);

    if (l <= NUM_DIGITS) {
        ht16k33_display_string(str);
    }
    else {
        for (int i = 0; i < l - NUM_DIGITS + 1; i++) {
            ht16k33_display_string(&str[i]);
            sleep_ms(interval_ms);
        }
    }
}

void ht16k33_set_brightness(int bright) {
    pm_qwiic_writeb(I2C_addr,HT16K33_BRIGHTNESS | (bright <= 15 ? bright : 15));
}

void ht16k33_set_blink(int blink) {
    int s = 0;
    switch (blink) {
        default: break;
        case 1: s = HT16K33_BLINK_2HZ; break;
        case 2: s = HT16K33_BLINK_1HZ; break;
        case 3: s = HT16K33_BLINK_0p5HZ; break;
    }

    pm_qwiic_writeb(I2C_addr,HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON | s);
}

void ht16k33_demo()
{
  
    ht16k33_init();

    printf("Welcome to HT33k16!\n");

again:

     ht16k33_display_string("   0");
//     printf("   0");
     sleep_ms(1000);
     ht16k33_display_string("  1 ");
//     printf("  1 ");     
     sleep_ms(1000);
     ht16k33_display_string(" 2  ");
//     printf(" 2  ");     
     sleep_ms(1000);
     ht16k33_display_string("3   ");
//     printf("3   ");     
     sleep_ms(1000);

     ht16k33_display_string("   0");
//     printf("   0");
     sleep_ms(1000);
     ht16k33_display_string("  1 ");
//     printf("  1 ");     
     sleep_ms(1000);
     ht16k33_display_string(" 2  ");
//     printf(" 2  ");     
     sleep_ms(1000);
     ht16k33_display_string("3   ");
//     printf("3   ");     
     sleep_ms(1000);

    ht16k33_scroll_string("The PicoMEM Display this !", 150);

    const char *strs[] = {
        "Help", "I am", "in a", "Pico", "MEM ", "Cant", "get ", "out "
    };

    for (int i = 0; i < count_of(strs); i++) {
        ht16k33_display_string((char *) strs[i]);
        sleep_ms(500);
    }

    sleep_ms(1000);

    // Test brightness and blinking

    // Set all segments on all digits on

    // Need a function for this

    // Fade up and down
    for (int j=0;j<5;j++) {
        for (int i = 0; i < 15; i++) {
            ht16k33_set_brightness(i);
            sleep_ms(30);
        }

        for (int i = 14; i >=0; i--) {
            ht16k33_set_brightness(i);
            sleep_ms(30);
        }
    }

    ht16k33_set_brightness(15);

    ht16k33_set_blink(1); // 0 for no blink, 1 for 2Hz, 2 for 1Hz, 3 for 0.5Hz
    sleep_ms(5000);
    ht16k33_set_blink(0);

    goto again;
}
