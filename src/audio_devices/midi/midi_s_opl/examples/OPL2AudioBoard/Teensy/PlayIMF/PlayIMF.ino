/**
 * This is an example sketch from the OPL2 library for Arduino. It plays an IMF audio file from SD card using the YM3812
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
 * By default this example will look for the city.imf file in the root of the SD card. This file is found among the
 * files for this example. For more information about the IMF file format please visit
 * http://www.shikadi.net/moddingwiki/IMF_Format
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2018-07-09
 * Songs from the games Bio Menace and Duke Nukem II by Bobby Prince
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <SPI.h>
#include <SdFat.h>
#include <OPL2.h>

OPL2 opl2;
SdFatSdio SD;
SdFile imfFile;

float imfSpeed;
long songLength = 0;

void setup() {
  opl2.begin();
  SD.begin();

  imfSpeed = 560.0;
  loadImfSong("city.imf");

  // imfSpeed = 280.0;
  // loadImfSong("kickbuta.imf");
}

void loop() {
  unsigned long time = millis();

  while (songLength > 0) {
    int wait = playImfSong();

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


void loadImfSong(char* fileName) {
  imfFile.open(fileName, FILE_READ);
  imfFile.seekSet(0);

  songLength  = imfFile.read();
  songLength += imfFile.read() << 8;
  if (songLength == 0) {
    songLength = 65535;
    imfFile.seekSet(4);
  }
}


int playImfSong() {
  byte reg  = imfFile.read();
  byte data = imfFile.read();
  float  wait = imfFile.read();
  wait += imfFile.read() << 8;

  if (reg != 0x00) {
    opl2.write(reg, data);
  }
  songLength -= 3;

  return round(wait * (1000 / imfSpeed));
}
