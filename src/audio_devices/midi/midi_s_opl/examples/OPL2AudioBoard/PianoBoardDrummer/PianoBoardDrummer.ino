/**
 * This is a simple demo sketch for the OPL2 library. It makes use of the Cheerful Electronic Piano Board
 * (https://www.tindie.com/products/cheerful/arduino-piano-board-2/) and assumes then you also have the
 * ArduinoPianoBoard library installed (see https://github.com/DhrBaksteen/ArduinoPianoBoard).
 *
 * This sketch makes a drum machine out of the Piano Board. The first 5 white keys are used to play various drum sounds.
 * 
 * OPL2 Board is connected as follows:
 *   Pin  8 - Reset
 *   Pin  9 - A0
 *   Pin 10 - Latch
 *   Pin 11 - Data
 *   Pin 13 - Shift
 * 
 * Piano Board is connected as follows:
 *   Pin 7  - /SS
 *   Pin 12 - MISO
 *   Pin 13 - SCK
 * 
 * Code by Maarten Janssen (maarten@cheerful.nl) 2020-04-11
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL2.h>
#include <PianoKeys.h>
#include <instruments.h>

#define NO_DRUM 255

OPL2 opl2;
PianoKeys piano(7);


byte drums[8] = {DRUM_BASS, NO_DRUM, DRUM_SNARE, NO_DRUM, DRUM_TOM, DRUM_HI_HAT, NO_DRUM, DRUM_CYMBAL};


void setup() {
	opl2.begin();

	// Load drum instruments and set percusive mode.
	Instrument bass = opl2.loadInstrument(INSTRUMENT_BDRUM2);
	Instrument snare = opl2.loadInstrument(INSTRUMENT_RKSNARE1);
	Instrument tom = opl2.loadInstrument(INSTRUMENT_TOM2);
	Instrument cymbal = opl2.loadInstrument(INSTRUMENT_CYMBAL1);
	Instrument hihat = opl2.loadInstrument(INSTRUMENT_HIHAT2);

	opl2.setPercussion(true);
	opl2.setDrumInstrument(bass);
	opl2.setDrumInstrument(snare);
	opl2.setDrumInstrument(tom);
	opl2.setDrumInstrument(cymbal);
	opl2.setDrumInstrument(hihat);
}


void loop() {
	piano.updateKeys();

	// Handle keys that are being pressed.
	for (int i = KEY_C; i <= KEY_G; i ++) {
		if (piano.wasKeyPressed(i) && drums[i] != NO_DRUM) {
			opl2.playDrum(drums[i], 4, NOTE_C);
		}
	}

	delay(20);
}
