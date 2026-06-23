/**
 * This is an example sketch from the OPL2 library for Arduino. It plays a YM3812-based VGM audio file from SD card using the YM3812
 * audio chip.
 *
 * OPL2 board is connected as follows:
 *   Pin  8 - Reset  (OPL2 reset)
 *   Pin  9 - A0     (OPL2 register/value selector)
 *   Pin 10 - Latch  (OPL2 latch SPI byte)
 *   Pin 11 - Data   (SPI MOSI)
 *   Pin 13 - Shift  (SPI clock)
 *
 * Connect the SD card with Arduino SPI pins as usual and use pin 7 as CS.
 *   Pin  7 - CS     (SD select)
 *   Pin 11 - MOSI   (SPI MOSI)
 *   Pin 12 - MISO   (SPI MISO)
 *   Pin 13 - SCK    (SPI clock)
 *
 * Refer to the wiki at https://github.com/DhrBaksteen/ArduinoOPL2/wiki/Connecting to learn how to connect your platform
 * of choice!
 *
 * By default this example will look for the stunts01.vgm file in the root of the SD card. This file is found among the
 * files for this example.
 * 
 * stunts01.vgm:   https://vgmrips.net/packs/pack/stunts-pc (01. Title Screen)
 * Track title:    Title Screen
 * Game Name:      Stunts
 * Composer:       Michael J. Sokyrka, Kris Hatlelid, Brian Plank
 * Release:        1990-10
 * VGM by:         Tom
 * 
 * wacky02.vgm:    https://vgmrips.net/packs/pack/wacky-wheels-ibm-pc-at (02. Dream)
 * Track title:    Dream
 * Game Name:      Wacky Wheels
 * Composer:       Mark Klem
 * Release:        1994-10-24
 * VGM by:         RandomName
 * 
 * This program will also print song info to serial output, 9600 baud.
 *
 * VGM is a 44100Hz sample-accurate logging format, able of logging YM3812 data (register/data pairs) and delays.
 *
 * Compatible VGM rips is available at https://vgmrips.net/packs/chip/ym3812
 * These files must be uncompressed to *.vgm.
 * Windows: Download gzip, copy gzip.exe and decompressVgz.bat to folder with vgz-files, and run the script.
 * Mac/Linux: Copy decompressVgz.sh to folder with vgz-files, and run the script.
 * Note that file names should not extend 8 characters.
 *
 * For more information about the VGM file format please visit
 * http://www.smspower.org/Music/VGMFileFormat
 *
 * Code by Eirik Stople (eirik@pcfood.net) 24-10-19
 * Most recent version of the Arduino OPL2 library can be found at GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <SD.h>
#include <OPL2.h>

OPL2 opl2;
File vgmFile;

enum playbackStatus{
  PLAYBACK_PLAYING = 1,
  PLAYBACK_COMPLETE = 2,
  PLAYBACK_ERROR_UNSUPPORTED_CODE = 3,
  PLAYBACK_ERROR_SD_INIT_FAILURE = 4,
  PLAYBACK_ERROR_FILE_OPEN_FAILURE = 5,
  PLAYBACK_ERROR_INVALID_FILE_TYPE = 6,
};

const uint32_t SAMPLES_PR_SECOND = 44100;

const uint8_t OFFSET_GD3 = 0x14;
const uint8_t OFFSET_SAMPLE_COUNT = 0x18;
const uint8_t OFFSET_LOOP_OFFSET = 0x1C;
const uint8_t OFFSET_VGM_DATA_OFFSET = 0x34;

const uint8_t CODE_CHIP_YM3812 = 0x5A;
const uint8_t CODE_WAIT_N_SAMPLES = 0x61;
const uint8_t CODE_WAIT_735_SAMPLES = 0x62;
const uint8_t CODE_WAIT_882_SAMPLES = 0x63;
const uint8_t CODE_END_OF_DATA = 0x66;
const uint8_t CODE_WAIT_1_SAMPLE = 0x70;
const uint8_t CODE_WAIT_16_SAMPLES = 0x7F;

uint32_t relativeLoopOffset = 0;
uint16_t waitSamples = 0;
enum playbackStatus PlaybackStatus = PLAYBACK_PLAYING;

const byte filename[] = "stunts01.vgm";
//const byte filename[] = "wacky02.vgm";

void error(enum playbackStatus errorcode) {
  PlaybackStatus = errorcode;
  Serial.print(F("Error: "));
  Serial.println(errorcode);
}

void setup() {
  Serial.begin(9600);
  opl2.begin();
  if (!SD.begin(7)) {
    error(PLAYBACK_ERROR_SD_INIT_FAILURE);
    return;
  }
  loadVgmSong(filename);
}

void loadVgmSong(char* fileName) {
  vgmFile = SD.open(fileName, FILE_READ);
  if (!vgmFile) {
    error(PLAYBACK_ERROR_FILE_OPEN_FAILURE);
    return;
  }

  const uint8_t header[] = "Vgm ";
  for (int i = 0; i < 4; ++i) {
    if (vgmFile.read() != (int)header[i]) {
      error(PLAYBACK_ERROR_INVALID_FILE_TYPE);
      return;
    }
  }

  dumpVgmMetadata();

  vgmFile.seek(OFFSET_LOOP_OFFSET); //VGM loop offset, return to this offset when reaching end of song
  relativeLoopOffset = readUint32FromFile();

  vgmFile.seek(OFFSET_VGM_DATA_OFFSET); //VGM data offset, beginning of song
  uint32_t relativeVgmOffset  = readUint32FromFile();

  vgmFile.seek(OFFSET_VGM_DATA_OFFSET + relativeVgmOffset);
}

uint32_t readUint32FromFile() {
  uint32_t ret  = (uint32_t)vgmFile.read();
  ret          += (uint32_t)vgmFile.read() << 8;
  ret          += (uint32_t)vgmFile.read() << 16;
  ret          += (uint32_t)vgmFile.read() << 24;
  return ret;
}

uint16_t readUint16FromFile() {
  uint16_t ret  = (uint16_t)vgmFile.read();
  ret          += (uint16_t)vgmFile.read() << 8;
  return ret;
}

void dumpVgmMetadata() {
  const int BUF_SIZE = 100;
  byte buf[BUF_SIZE];

  //Output similar to VGMPlay
  Serial.print(F("VGM player (YM3812)\n\nFile name:      "));
  printStringln(filename);

  vgmFile.seek(OFFSET_SAMPLE_COUNT); //Sample count: Number of samples in file, 44100 pr second
  uint32_t sampleCount  = readUint32FromFile();
  uint32_t minutes = sampleCount / (SAMPLES_PR_SECOND * 60); //Number of minutes
  uint32_t remainder = sampleCount - minutes * (SAMPLES_PR_SECOND * 60);
  uint8_t seconds = remainder / SAMPLES_PR_SECOND; //Number of seconds
  remainder = remainder - seconds * SAMPLES_PR_SECOND;
  uint8_t centiseconds = remainder / (SAMPLES_PR_SECOND / 100); //Number of centiseconds
  sprintf(buf, "Duration:       %lu:%02u.%02u (%lu samples)", minutes, seconds, centiseconds, sampleCount); //minutes:seconds.centiseconds
  printStringln(buf);

  vgmFile.seek(OFFSET_GD3); //GD3 data offset
  uint32_t relativeGd3Offset  = readUint32FromFile();

  if (relativeGd3Offset == 0) {
    Serial.println(F("No GD3 tag in file"));
  } else {
    //Expecting compability with GD3 1.00
    vgmFile.seek(OFFSET_GD3 + relativeGd3Offset + 12);

    printGd3Field(buf, BUF_SIZE, F("Track title:    ")); //1:  Track title
    readGd3Field( buf, BUF_SIZE                       ); //2:  Track title (Japanese), ignore
    printGd3Field(buf, BUF_SIZE, F("Game Name:      ")); //3:  Game name
    readGd3Field( buf, BUF_SIZE                       ); //4:  Game name (Japanese), ignore
    printGd3Field(buf, BUF_SIZE, F("System:         ")); //5:  System name
    readGd3Field( buf, BUF_SIZE                       ); //6:  System name (Japanese), ignore
    printGd3Field(buf, BUF_SIZE, F("Composer:       ")); //7:  Name of Original Track Author
    readGd3Field( buf, BUF_SIZE                       ); //8:  Name of Original Track Author (Japanese), ignore
    printGd3Field(buf, BUF_SIZE, F("Release:        ")); //9:  Date of game's release
    printGd3Field(buf, BUF_SIZE, F("VGM by:         ")); //10: Name of person who converted it to a VGM file
  }
}

void printGd3Field(byte *buf, int maxsize, const __FlashStringHelper* fieldName) {
  readGd3Field(buf, maxsize);
  Serial.print(fieldName);
  printStringln(buf);
}

void readGd3Field(byte *p, int maxsize) {
  //Gd3-fields use 16bit characters, zero-terminated
  uint16_t c;
  int pos = 0;
  do {
    c  = readUint16FromFile();
    p[pos] = (byte)c; //Ignore high byte
    if (pos < maxsize - 1) pos++;
  } while (c != 0);
  p[pos] = 0;
}

void printString(byte *p) {
  while (*p) Serial.print((char)*p++);
}

void printStringln(byte *p) {
  printString(p);
  Serial.println("");
}

void loop() {
  //Play until finished/error
  uint32_t time = millis();
  while (PlaybackStatus == PLAYBACK_PLAYING) {
    playVgmSongYM3812(); //Fetch and execute one command

    if (waitSamples) { //Delay command found, delay n samples
      const double samplesPrMs = (double)SAMPLES_PR_SECOND / 1000; //44.1
      uint16_t waitMs = (uint16_t)((double)waitSamples / samplesPrMs);

      //Reduce delay with time spent on IO
      uint32_t ioTime = millis() - time;
      if (ioTime < waitMs) {
        delay(waitMs - ioTime);
      }
      time = millis();
    }
    waitSamples = 0;
  }
}

void unsupportedCode(byte b) {
  uint8_t buf[50];
  sprintf(buf, "Unsupported code 0x%02X offset 0x%08lX", b, vgmFile.position() - 1);
  printStringln(buf);
  PlaybackStatus = PLAYBACK_ERROR_UNSUPPORTED_CODE;
}

void playVgmSongYM3812() {

  //Simple stupid implementation, will stop if reading unsupported byte

  uint8_t code = vgmFile.read();

  switch (code) {
    case CODE_CHIP_YM3812: //Single OPL2 chip
      {
        byte reg  = vgmFile.read();
        byte data = vgmFile.read();
        opl2.write(reg, data);
      }
      break;

    case CODE_WAIT_N_SAMPLES: //Delay 0-65535 samples (1.49 second)
      waitSamples  = readUint16FromFile();
      break;

    case CODE_WAIT_735_SAMPLES: //Delay 735 samples (1/60 second)
      waitSamples  = 735;
      break;

    case CODE_WAIT_882_SAMPLES: //Delay 882 samples (1/50 second)
      waitSamples  = 882;
      break;

    case CODE_END_OF_DATA: //End of data
      if (relativeLoopOffset) {
        vgmFile.seek(OFFSET_LOOP_OFFSET + relativeLoopOffset);
      } else {
        PlaybackStatus = PLAYBACK_COMPLETE;
      }
      break;

    case CODE_WAIT_1_SAMPLE ... CODE_WAIT_16_SAMPLES: //0-15 sample delay
      waitSamples = code - CODE_WAIT_1_SAMPLE + 1;
      break;

    default:
      unsupportedCode(code);
      break;
  }
}
