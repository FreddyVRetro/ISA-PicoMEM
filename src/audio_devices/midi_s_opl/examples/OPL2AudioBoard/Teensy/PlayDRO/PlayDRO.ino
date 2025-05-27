/**
 * This is an example sketch from the OPL2 library for Arduino. It plays a DRO audio file from SD card using the YM3812
 * audio chip. This example requires a Teensy with onboard SD card slot and the SdFat library by Bill Greiman. You can
 * install it using the Arduino Library Manager or download it from github at https://github.com/greiman/SdFat. If you
 * use an SPI based breakout board for SPI then please use the Arduino version of this sketch.
 *
 * OPL2 board is connected as follows:
 *   Pin  8 - Reset
 *   Pin  9 - A0
 *   Pin 10 - Latch
 *   Pin 11 - Data     (Use pin 22 for Teensy ++ 2.0)
 *   Pin 13 - Shift    (Use pin 21 for Teensy ++ 2.0)
 *
 * By default this example will look for the phemopop.dro file in the root of the SD card. This file is found among the
 * files for this example. For more information about the DRO file format please visit
 * http://www.shikadi.net/moddingwiki/DRO_Format
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2018-07-09
 * Song Phemo-pop! by Olli Niemitalo/Yehar 1996
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <SPI.h>
#include <SdFat.h>
#include <OPL2.h>

OPL2 opl2;
SdFatSdio SD;
SdFile droFile;

unsigned long offset       = 0;
unsigned long songLength   = 0;
unsigned long songDuration = 0;
byte codeShortDelay    = 0;
byte codeLongDelay     = 0;
byte registerMapLength = 0;
byte registerMap[256];


void setup() {
  opl2.begin();
  SD.begin();

  loadDroSong("phemopop.dro");
  // loadDroSong("strikefo.dro");
}


void loop() {
  unsigned long time = millis();

  while (songLength > 0) {
    int wait = playDroSong();

    if (wait > 0) {
      // Take into account time that was spent on IO.
      unsigned long ioTime = millis() - time;
      if (ioTime < wait) {
        delay(wait - ioTime);
      }
      time = millis();
    }

    songLength --;
  }
}


void loadDroSong(char* fileName) {
  droFile.open(fileName, FILE_READ);
  droFile.seekSet(12);

  songLength  = droFile.read();
  songLength += droFile.read() << 8;
  songLength += droFile.read() << 16;
  songLength += droFile.read() << 24;

  songDuration  = droFile.read();
  songDuration += droFile.read() << 8;
  songDuration += droFile.read() << 16;
  songDuration += droFile.read() << 24;

  droFile.seekSet(23);
  codeShortDelay = droFile.read();
  codeLongDelay  = droFile.read();
  registerMapLength = droFile.read();

  for (byte i = 0; i < registerMapLength; i ++) {
    registerMap[i] = droFile.read();
  }
}


int playDroSong() {
  byte code = droFile.read();
  byte data = droFile.read();

  if (code == codeShortDelay) {
    return data + 1;
  } else if (code == codeLongDelay) {
    return (data + 1) << 8;
  } else if (code < 128) {
    opl2.write(registerMap[code], data);
  }

  return 0;
}
