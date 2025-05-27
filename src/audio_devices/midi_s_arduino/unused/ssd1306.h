/*
    SSD1306 display driver
    https://github.com/DLehenbauer/arduino-midi-sound-module

    A specialized driver for SSD1306-based OLED display, used to concurrently update the real-time bar
    from the main 'loop()' in tiny slices, interleaved with dispatching MIDI messages.
    
    The only drawing primitive exposed is the 'set7x8' command, which allows us to optimally update bars
    in 7x8 blocks with no double buffering in the uC's data memory.  (The 8th column is always unset to
    create a visual space between bars in the bar graph.)

    Connection to Arduino Uno:

                  .-----------------.
                  | .---------. GND o------< pin 7*
                  | |         | VCC o------< +5v
                  | |   SSD   |  D0 o------< pin 13
                  | |  1306   |  D1 o------< pin 11
                  | | 4-Wire  | RES o------< pin 2
                  | |   SPI   |  DC o------< pin 3
                  | '---------'  CS o------< pin 4
                  '-----------------'

    Notes:
      * We use pin 7, which is pulled low in this driver, instead of connecting the OLED to gnd to
        reduce the amount of audio noise introduced by the OLED.  (For me, it made a notable difference.)
*/

#ifndef __SSD1306_H__
#define __SSD1306_H__

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
enum Command: uint8_t {
  Command_SetMemAddressMode   = 0x20,         // 00 = Horiz, 01 = Vert, 10 = Page, 11 = Invalid
  Command_SetDisplayStartLine = 0x40,         // +0..63 to select the start line.
  Command_SetMultiplexRatio   = 0xA8,         // A[7:0] = Multiplex ratio in the range 16..63
  Command_SetDisplayClock     = 0xD5,         // A[3:0] = DCLK divide ratio, A[7:4] = Oscillator frequency
  Command_SetSegmentRemap     = 0xA0,         // +1 maps column 127 to SEG 0
  Command_SetDisplayOff       = 0xAE,
  Command_SetDisplayOn        = 0xAF,
  Command_SetComOutScanDir    = 0xC0,         // Scan from COM0 -> COM[N-1], N = Multiplex Ratio (C8 is reverse)
  Command_SetDisplayOffset    = 0xD3,
  Command_SetChargePump       = 0x8D,         // 0x10 - Disable, 0x14 - Enable [See App Note pg 3/6.]
  Command_SetCOMPins          = 0xDA,         // A[4] = Alternate config, A[5] = Enable Left/Right remap
  Command_SetContrast         = 0x81,         // A[7:0] = Value (increases to 0xFF, reset = 0x7F)
  Command_SetPreChargePeriod  = 0xD9,         // A[3:0] = Phase 1 (1..15 DCLK), A[7:4] = Phase 2 (1..15 DCLK)
  Command_DisplayResume       = 0xA4,         // Resume displaying RAM contents
  Command_SetNormalDisplay    = 0xA6,         // +1 to invert display
  Command_DeactivateScroll    = 0x2E,         // Stop scrolling
  Command_SetVComHDeselectLvl = 0xDB,         // [A6:4] = 00 ~0.65 Vcc, 10 = ~0.77 Vcc, 11 = ~0.83 Vcc
  Command_SetColumnAddr       = 0x21,
  Command_SetPageAddr         = 0x22,
};

template <bool rotate180>
class Ssd1306 final {
  private:
    static constexpr uint8_t _resPin = _BV(DDD2);
    static constexpr uint8_t  _dcPin = _BV(DDD3);
    static constexpr uint8_t  _csPin = _BV(DDD4);
    static constexpr uint8_t _gndPin = _BV(DDD7);
    
    static constexpr uint8_t _cmdPins   = _csPin | _dcPin;
    static constexpr uint8_t _dataPins  = _csPin;

  public:
    Ssd1306() {}
  
    void begin() {
      PORTD |= _resPin | _dcPin | _csPin;             // Set display RES/DC/CS pins HIGH.
      PORTD &= ~_gndPin;                              // Set the gnd pin LOW.
      DDRD  |= _resPin | _dcPin | _csPin | _gndPin;   // Set display RES/DC/CS pins as output.

      PORTB |= _BV(DDB2);                             // Must also set default SPI CS pin as output, even
      DDRB |= _BV(DDB2);                              // if we're not using it for the display.

      SPSR |= _BV(SPI2X);                             // SCK = F_CPU/2
      SPCR = _BV(SPE) | _BV(MSTR);                    // Enable SPI, Master
    
      DDRB |= _BV(DDB5) | _BV(DDB3);                  // Set MOSI and SCK as outputs after enabling SPI.
    }
  
    void reset() {
      PORTD |= _resPin;
      _delay_ms(100);
      PORTD &= ~_resPin;
      _delay_ms(100);
      PORTD |= _resPin;

      // Use the same configuration as olikraus's excellent u8g2 library:
      // https://github.com/olikraus/u8g2/blob/master/csrc/u8x8_d_ssd1306_128x64_noname.c

      beginCommand();
      send(Command_SetDisplayOff);
      send(Command_SetDisplayClock);      // A[3:0] = DCLK divide ratio, A[7:4] = Oscillator frequency
      send(0x80);
      send(Command_SetMultiplexRatio);    // A[7:0] = Multiplex ratio in the range 16..63
      send(0x3F);
      send(Command_SetDisplayOffset);
      send(0x00);
      send(Command_SetDisplayStartLine);
      send(Command_SetChargePump);
      send(0x14);
      send(Command_SetMemAddressMode);
      send(0x00);
      if (rotate180) {
        send(Command_SetSegmentRemap | 0x01);
        send(Command_SetComOutScanDir | 0x08);
      } else {
        send(Command_SetSegmentRemap);
        send(Command_SetComOutScanDir);
      }
      send(Command_SetCOMPins);
      send(0x12);
      send(Command_SetContrast);
      send(0xCF);
      send(Command_SetPreChargePeriod);
      send(0xF1);
      send(Command_SetVComHDeselectLvl);
      send(0x40);
      send(Command_DeactivateScroll);
      send(Command_DisplayResume);
      send(Command_SetNormalDisplay);
      send(Command_SetDisplayOn);
      endCommand();
    }

    void select(uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage) {
      beginCommand();
      send(Command_SetColumnAddr);
      send(minX);
      send(maxX);

      send(Command_SetPageAddr);
      send(minPage);
      send(maxPage);
      endCommand();
    }
  
    void setRegion(uint8_t minX, uint8_t maxX, uint8_t minPage, uint8_t maxPage, uint8_t value) {
      select(minX, maxX, minPage, maxPage);
    
      const uint8_t numX = maxX - minX;
      const uint8_t numPage = maxPage - minPage;
    
      beginData();
      for (int8_t y = numPage; y >= 0; y--) {
        for (int8_t x = numX; x >= 0; x--) {
          send(value);
        }
      }
      endData();
    }
  
    void set7x8(uint8_t value) {
      beginData();
      send(value);
      send(value);
      send(value);
      send(value);
      send(value);
      send(value);
      send(value);
      endData();
    }
    
  private:
    void beginCommand() __attribute__((always_inline)) {
      PORTD &= ~_cmdPins;         // Select SSD1306 for command.
    }
  
    void endCommand() __attribute__((always_inline)) {
      PORTD |= _cmdPins;          // Deselect SSD1306.
    }

    void beginData() __attribute__((always_inline)) {
      PORTD &= ~_dataPins;        // Select SSD1306 for data.
    }
  
    void endData() __attribute__((always_inline)) {
      PORTD |= _dataPins;         // Deselect SSD1306.
    }
    
    void send(const uint8_t data) __attribute__((always_inline)) {
      SPDR = data;
      while (!(SPSR & _BV(SPIF)));
    }
};

#endif //__SSD1306_H__
