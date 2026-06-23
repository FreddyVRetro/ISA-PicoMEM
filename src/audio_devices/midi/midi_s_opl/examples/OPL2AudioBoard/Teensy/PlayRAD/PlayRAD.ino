/**
 * This is an example sketch from the OPL2 library for Teensy. It plays Reality Adlib Tracker (RAD) files from SD card
 * using the YM3812 audio chip. This example requires a Teensy with onboard SD card slot and the SdFat library by Bill
 * Greiman. You can install it using the Arduino Library Manager or download it from github at
 * https://github.com/greiman/SdFat. If you use an SPI based breakout board for SD then please use the Arduino version
 * of this sketch.
 *
 * OPL2 board is connected as follows:
 *   Pin  8 - Reset
 *   Pin  9 - A0
 *   Pin 10 - Latch
 *   Pin 11 - Data     (Use pin 22 for Teensy ++ 2.0)
 *   Pin 13 - Shift    (Use pin 21 for Teensy ++ 2.0)
 *
 * By default this example will look for the shotrad file in the root of the SD card. This file is found among the
 * files for this example. For more information about the RAD file format download the Reality Adlib Tracker from
 * http://www.pouet.net/prod.php?which=48994
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2018-07-09
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <SPI.h>
#include <SdFat.h>
#include <OPL2.h>


// RAD effect codes.
#define EFFECT_NONE              0x00
#define EFFECT_NOTE_SLIDE_UP     0x01
#define EFFECT_NOTE_SLIDE_DOWN   0x02
#define EFFECT_NOTE_SLIDE_TO     0x03
#define EFFECT_NOTE_SLIDE_VOLUME 0x05
#define EFFECT_VOLUME_SLIDE      0x0A
#define EFFECT_SET_VOLUME        0x0C
#define EFFECT_PATTERN_BREAK     0x0D
#define EFFECT_SET_SPEED         0x0F


// Minimum and maximum F-numbers in each octave.
#define FNUMBER_MIN 0x156
#define FNUMBER_MAX 0x2AE


// Different note F-numbers stored in program memory.
const short noteFrequencies[12] PROGMEM = {
  0x16B, 0x181, 0x198, 0x1B0, 0x1CA, 0x1E5,
  0x202, 0x220, 0x241, 0x263, 0x287, 0x2AE
};


// Song data.
unsigned int patternOffsets[32] = { 0x0000 };     // File offsets to pattern data of each pattern.
unsigned int orderListOffset = 0x0000;            // File offset to order list.
byte instruments[31][10] = { 0x00 };              // Instrument data.
byte songLength = 0;                              // Number of orders in order list.
byte initialSpeed = 6;                            // Initial song speed.
bool hasSlowTimer = false;                        // Normal speed is 50Hz, slow speed is 18.2Hz.


// Player variables.
bool loopSong = true;                             // Restart the song after playing the last order.
byte order = 0;                                   // Index in order list.
byte line  = 0;                                   // Current line in pattern.
byte tick  = 0;                                   // Current tick of line.
byte speed = initialSpeed;                        // Current tick speed.
bool endOfPattern = false;                        // Set when last byte of pattern data is read.
unsigned int channelNote[9] = { 0x0000 };         // Note, octave, instrument and effect per channel.
unsigned int pitchSlideDest[9] { 0x0000 };        // Destination note + octave of pitch slides per channel.
byte efftectParameter[9] = { 0x00 };              // Effect parameters for each channel.
byte pitchSlideSpeed[9] = { 0x00 };               // Pitch slide speeds of each channel.
byte patternBreak = 0xFF;                         // Pattern break at end of line (0xFF if no pattern break).


OPL2 opl2;
SdFatSdio SD;
SdFile radFile;


void setup() {
  SD.begin();

  // Load one of the included RAD files.
  loadRadFile("adlibsp.rad");
  // loadRadFile("shoot.rad");
  // loadRadFile("action.rad");

  initPlayer();
}


void loop() {
  unsigned long time = millis();

  if (order < songLength) {
    playMusic();
  }

  // Calculate delay based on song speed and time spent in player routine.
  int timeSpent = millis() - time;
  int wait = max(0, (hasSlowTimer ? 55 : 20) - timeSpent);
  delay(wait);
}


/**
 * Load instrument data for the given channel.
 */
void setInstrument(byte channel, byte instrumentIndex) {
  byte registerOffset;

  registerOffset = opl2.getRegisterOffset(channel, CARRIER);
  opl2.setRegister(0x20 + registerOffset, instruments[instrumentIndex][0]);
  opl2.setRegister(0x40 + registerOffset, instruments[instrumentIndex][2]);
  opl2.setRegister(0x60 + registerOffset, instruments[instrumentIndex][4]);
  opl2.setRegister(0x80 + registerOffset, instruments[instrumentIndex][6]);
  opl2.setRegister(0xE0 + registerOffset, instruments[instrumentIndex][9] & 0x0F);

  registerOffset = opl2.getRegisterOffset(channel, MODULATOR);
  opl2.setRegister(0x20 + registerOffset, instruments[instrumentIndex][1]);
  opl2.setRegister(0x40 + registerOffset, instruments[instrumentIndex][3]);
  opl2.setRegister(0x60 + registerOffset, instruments[instrumentIndex][5]);
  opl2.setRegister(0x80 + registerOffset, instruments[instrumentIndex][7]);
  opl2.setRegister(0xE0 + registerOffset, (instruments[instrumentIndex][9] & 0xF0) >> 4);

  opl2.setRegister(0xC0 + channel, instruments[instrumentIndex][8]);
}


/**
 * Play a note on the given channel.
 */
void playNote(byte channel, byte octave, byte note) {
  opl2.setKeyOn(channel, false);
  opl2.setBlock(channel, octave + (note / 12));
  opl2.setFNumber(channel, pgm_read_word_near(noteFrequencies + (note % 12)));
  opl2.setKeyOn(channel, true);
}


/**
 * Slide the pitch of a channel by a given F-number amount and return the new block + F-number.
 */
unsigned int pitchAdjust(byte channel, short amount) {
  byte block = opl2.getBlock(channel);
  short fNumber = opl2.getFNumber(channel);
  fNumber = max(0x000, min(fNumber + amount, 0x3FF));

  // Drop one octave (if possible) when the F-number drops below octave minimum.
  if (fNumber < FNUMBER_MIN) {
    if (block > 0) {
      block --;
      fNumber = FNUMBER_MAX - (FNUMBER_MIN - fNumber);
    }

  // Increase one octave (if possible) when the F-number reaches above octave maximum.
  } else if (fNumber > FNUMBER_MAX) {
    if (block < 7) {
      block ++;
      fNumber = FNUMBER_MIN + (fNumber - FNUMBER_MAX);
    }
  }

  // Set new octave and frequency and return combind octave + F-number value.
  opl2.setBlock(channel, block);
  opl2.setFNumber(channel, fNumber);
  return (block << 12) + fNumber;
}


/**
 * Adjust the pitch of the note on the given channel toward pitchSlideDest for the channel and stop sliding once the
 * destination pitch is reached.
 */
void pitchAdjustToNote(byte channel) {
  if (pitchSlideDest[channel] == 0x0000) return;

  unsigned int slidePos = opl2.getBlock(channel) << 12;
  slidePos += opl2.getFNumber(channel);

  // Slide pitch up and compensate any overshoot.
  if (slidePos < pitchSlideDest[channel]) {
    slidePos = pitchAdjust(channel, pitchSlideSpeed[channel]);
    if (slidePos >= pitchSlideDest[channel]) {
      opl2.setBlock  (channel, (pitchSlideDest[channel] & 0xF000) >> 12);
      opl2.setFNumber(channel,  pitchSlideDest[channel] & 0x0FFF);
      pitchSlideDest[channel] = 0x0000;
    }

  // Slide pitch down and compensate for any undershoot.
  } else if (slidePos > pitchSlideDest[channel]) {
    slidePos = pitchAdjust(channel, -pitchSlideSpeed[channel]);
    if (slidePos <= pitchSlideDest[channel]) {
      opl2.setBlock  (channel, (pitchSlideDest[channel] & 0xF000) >> 12);
      opl2.setFNumber(channel,  pitchSlideDest[channel] & 0x0FFF);
      pitchSlideDest[channel] = 0x0000;
    }
  }
}


/**
 * Adjust the volume of a channel by the gven amount.
 */
void volumeAdjust(byte channel, byte amount) {
  byte volume = opl2.getVolume(channel, CARRIER);

  if (amount > 0 && amount < 50) {
    volume = min(63, volume + amount);
  } else if (amount > 50 && amount < 100) {
    volume = max(0, volume - (amount - 50));
  }

  opl2.setVolume(channel, CARRIER, volume);
}


/**
 * Read the next line in the pattern from the current file position.
 */
void readLine() {
  // Reset note data on each channel.
  for (byte i = 0; i < 9; i ++) {
    channelNote[i] = 0x0000;
  }

  // If the previous line was the last line of the pattern then we're done.
  if (endOfPattern) {
    return;
  }

  // If the next line number does not match our current line then it's empty
  // so there's nothing to do for us here.
  byte lineNumber = radFile.read();
  if ((lineNumber & 0x3F) != line) {
    radFile.seekSet(radFile.curPosition() - 1);
    return;
  }

  // Set end of pattern marker if this is the last line in the pattern.
  endOfPattern = lineNumber & 0x80;

  // Read note and effect data for each channel.
  bool isLastChannel = false;
  do {
    byte channelNumber = radFile.read();
    isLastChannel = channelNumber & 0x80;
    channelNumber = channelNumber & 0x0F;

    channelNote[channelNumber]  = radFile.read() << 8;
    channelNote[channelNumber] += radFile.read();
    if (channelNote[channelNumber] & 0x000F) {
      efftectParameter[channelNumber] = radFile.read();
    } else {
      efftectParameter[channelNumber] = 0x00;
    }
  } while(!isLastChannel);
}


/**
 * Jump to the next order by setting the file pointer to the offset of the next pattern to be played. A startLine may
 * be given since the pattern break effect can start the next order on any particular line. When the end of the song is
 * reached we may loop back to the beginning if loopSong is true.
 */
void nextOrder(byte startLine = 0) {
  // Increment order and read patternIndex, may loop song when we've reached the end.
  order ++;
  if (order >= songLength && loopSong) {
    order = 0;
  }
  radFile.seekSet(orderListOffset + order);
  byte patternIndex = radFile.read();

  // If bit 7 is set then we're looking at an order jump.
  if (patternIndex & 0x80) {
    order = patternIndex & 0x7F;
    radFile.seekSet(orderListOffset + order);
    patternIndex = radFile.read();
  }

  // Set file offset to the start of the next order and skip lines until we're
  // at the requested startLine.
  radFile.seekSet(patternOffsets[patternIndex]);
  endOfPattern = false;
  for (line = 0; line < startLine; line ++) {
    readLine();
  }
}


/**
 * Initialize the OPL2 board and load the first line of the first order.
 */
void initPlayer() {
  opl2.begin();
  opl2.setWaveFormSelect(true);
  opl2.setPercussion(false);
  speed = initialSpeed;

  order = -1;
  line = 0;
  tick = 0;
  nextOrder();
  readLine();
}


/**
 * Process one tick of the current line and advance the song if needed.
 */
void playMusic() {
  for (byte channel = 0; channel < 9; channel ++) {
    unsigned short channelData = channelNote[channel];
    byte effect = channelData & 0x000F;

    if (tick == 0) {
      byte instrument = ((channelData & 0x8000) >> 11) + ((channelData & 0x00F0) >> 4);
      byte octave     = (channelData & 0x7000) >> 12;
      byte note       = (channelData & 0x0F00) >> 8;

      // Set instrument.
      if (instrument > 0) {
        setInstrument(channel, instrument - 1);
      }

      // Stop note.
      if (note == 0x0F) {
        opl2.setKeyOn(channel, false);
      }

      // Trigger note.
      else if (note && effect != EFFECT_NOTE_SLIDE_TO) {
        playNote(channel, octave, note);
      }

      // Process line effects.
      switch(effect) {
        case EFFECT_NOTE_SLIDE_TO: {
          if (note > 0x00 && note < 0x0F) {
            pitchSlideDest[channel]  = (octave + (note / 12)) << 12;
            pitchSlideDest[channel] += pgm_read_word_near(noteFrequencies + (note % 12));
            pitchSlideSpeed[channel] = efftectParameter[channel];
          }
          break;
        }

        case EFFECT_SET_VOLUME: {
          opl2.setVolume(channel, CARRIER, max(0, min(64 - efftectParameter[channel], 63)));
          break;
        }

        case EFFECT_PATTERN_BREAK: {
          patternBreak = efftectParameter[channel];
          break;
        }

        case EFFECT_SET_SPEED: {
          speed = efftectParameter[channel];
          break;
        }
      }
    }

    // Process tick effects.
    switch(effect) {
      case EFFECT_NOTE_SLIDE_UP: {
        pitchAdjust(channel, efftectParameter[channel]);
        break;
      }

      case EFFECT_NOTE_SLIDE_DOWN: {
        pitchAdjust(channel, -efftectParameter[channel]);
        break;
      }

      case EFFECT_NOTE_SLIDE_VOLUME: {
        pitchAdjustToNote(channel);
        volumeAdjust(channel, efftectParameter[channel]);
        break;
      }

      case EFFECT_NOTE_SLIDE_TO: {
        pitchAdjustToNote(channel);
        break;
      }

      case EFFECT_VOLUME_SLIDE: {
        volumeAdjust(channel, efftectParameter[channel]);
        break;
      }
    }
  }

  // Advance song.
  tick = (tick + 1) % speed;
  if (tick == 0) {
    if (patternBreak == 0xFF) {
      line = (line + 1) % 64;
      if (line == 0) {
        nextOrder();
      }
    } else {
      line = patternBreak;
      patternBreak = 0xFF;
      nextOrder();
    }

    readLine();
  }
}


/**
 * Load the given RAD file from SD card.
 */
void loadRadFile(char* fileName) {
  radFile.open(fileName);

  radFile.seekSet(0x11);
  byte songProps = radFile.read();
  hasSlowTimer = (songProps & 0x40) != 0x00;
  initialSpeed = songProps & 0x1F;

  // Skip song description block if available.
  if ((songProps & 0x80) != 0x00) {
    while (radFile.read() != 0x00);
  }

  // Read instruments.
  byte instrumentNum = 0;
  while ((instrumentNum = radFile.read()) != 0x00) {
    instrumentNum --;
    for (byte i = 0; i < 9; i ++) {
      instruments[instrumentNum][i] = radFile.read();
    }
    instruments[instrumentNum][9]  =  radFile.read() & 0x07;
    instruments[instrumentNum][9] += (radFile.read() & 0x07) << 4;
  }

  // Read song length, order list offset and skip order list.
  songLength = radFile.read();
  orderListOffset = radFile.curPosition();
  radFile.seekSet(orderListOffset + songLength);

  // Read pattern offsets.
  for (byte i = 0; i < 32; i++) {
    patternOffsets[i] = radFile.read() + (radFile.read() << 8);
  }
}
