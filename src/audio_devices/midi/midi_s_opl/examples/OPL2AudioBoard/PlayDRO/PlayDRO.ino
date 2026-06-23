/**
 * This is an example sketch from the OPL2 library for Arduino. It plays a DRO audio file from SD card using the YM3812
 * audio chip.
 *
 * OPL2 board is connected as follows:
 *   Pin  8 - Reset
 *   Pin  9 - A0
 *   Pin 10 - Latch
 *   Pin 11 - Data
 *   Pin 13 - Shift
 *
 * Connect the SD card with Arduino SPI pins as usual and use pin 7 as CS.
 *
 * Refer to the wiki at https://github.com/DhrBaksteen/ArduinoOPL2/wiki/Connecting to learn how to connect your platform
 * of choice!
 *
 * By default this example will look for the phemopop.dro file in the root of the SD card. This file is found among the
 * files for this example. For more information about the DRO file format please visit
 * http://www.shikadi.net/moddingwiki/DRO_Format
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2016-12-17
 * Song Phemo-pop! by Olli Niemitalo/Yehar 1996
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <SPI.h>
#include <SD.h>
#include <OPL2.h>

OPL2 opl2;
File droFile;

unsigned long offset       = 0;
unsigned long songLength   = 0;
unsigned long songDuration = 0;
byte codeShortDelay    = 0;
byte codeLongDelay     = 0;
byte registerMapLength = 0;
byte registerMap[256];


void setup() {
  opl2.begin();
  SD.begin(7);

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
  droFile = SD.open(fileName, FILE_READ);
  droFile.seek(12);

  songLength  = droFile.read();
  songLength += droFile.read() << 8;
  songLength += droFile.read() << 16;
  songLength += droFile.read() << 24;

  songDuration  = droFile.read();
  songDuration += droFile.read() << 8;
  songDuration += droFile.read() << 16;
  songDuration += droFile.read() << 24;

  droFile.seek(23);
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
