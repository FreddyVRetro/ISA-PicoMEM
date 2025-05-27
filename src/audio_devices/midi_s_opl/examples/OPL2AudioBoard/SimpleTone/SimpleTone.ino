/**
 * This is a simple demo sketch for the OPL2 library. It plays a musical scale on channels 0, 1 and 2 of the YM3812
 * using a bell sound.
 *
 * OPL2 board is connected as follows:
 *   Pin  8 - Reset
 *   Pin  9 - A0
 *   Pin 10 - Latch
 *   Pin 11 - Data
 *   Pin 13 - Shift
 *
 * Refer to the wiki at https://github.com/DhrBaksteen/ArduinoOPL2/wiki/Connecting to learn how to connect your platform
 * of choice!
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2016-12-18
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <SPI.h>
#include <OPL2.h>


OPL2 opl2;


void setup() {
	opl2.begin();

	// Setup channels 0, 1 and 2 to produce a bell sound.
	for (byte i = 0; i < 3; i ++) {
		opl2.setTremolo   (i, CARRIER, true);
		opl2.setVibrato   (i, CARRIER, true);
		opl2.setMultiplier(i, CARRIER, 0x04);
		opl2.setAttack    (i, CARRIER, 0x0A);
		opl2.setDecay     (i, CARRIER, 0x04);
		opl2.setSustain   (i, CARRIER, 0x0F);
		opl2.setRelease   (i, CARRIER, 0x0F);
		opl2.setVolume    (i, CARRIER, 0x00);
	}

	// Play notes C-3 through B-4 on alternating channels.
	for (byte i = 0; i < 24; i ++) {
		byte octave = 3 + (i / 12);
		byte note = i % 12;
		opl2.playNote(i % 3, octave, note);
		delay(300);
	}
}


void loop() {
}
