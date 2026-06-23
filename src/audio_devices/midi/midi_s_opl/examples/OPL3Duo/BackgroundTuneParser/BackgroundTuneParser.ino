/**
 * This is a demonstration sketch for the OPL3 Duo! It demonstrates how the TuneParser can be used to play music in the
 * and loop a piece of music in the background while the Arduino is performing other tasks.
 *
 * Code by Maarten Janssen, 2020-10-04
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <OPL3Duo.h>
#include <midi_instruments_4op.h>
#include <TuneParser.h>

const char pattern1[] PROGMEM = "t120i44o3l16 frfrfrfrfrfrfrfr grgrgrgrgrgrgrgr  brbrbrbr arararar  grgrgrgrgrgrgrfr grgrgrgrgrgrgrgr";
const char pattern2[] PROGMEM = "    i44o3l16 arararararararar brbrbrbrbrbrbrbr >drdrdrdr crcrcrcr <brbrbrbrbrbrbrar brbrbrbrbrbrbrbr";
const char pattern3[] PROGMEM = "    i44o4l16 drdrdrdrdrdrdrdr drdrdrdrdrdrdrdr  grgrgrgr frfrfrfr  drdrdrdrdrdrdrdr drdrdrdrdrdrdrdr";

OPL3Duo opl3;
TuneParser tuneParser(&opl3);
Tune tune;


void setup() {
	// Initialize the TuneParser and the tune that we want to play in the background.
	// OPL3Duo is initialized by the TuneParser.
	tuneParser.begin();
	tune = tuneParser.playBackground(pattern1, pattern2, pattern3);
}


void loop() {
	// When the tune has reached its end restart it.
	if (tuneParser.tuneEnded(tune)) {
		tuneParser.restartTune(tune); 
	}

	// Update the tune parser and return the number of ms until the next tick needs to be processed and update should be
	// called again. If this function is called before it's time to process the next tick then it will do nothing, but
	// return the time remaining until the next tick.
	unsigned long timeUntilNextTick = tuneParser.update(tune);

	// Do whatever your code should be doing...
	// In this example we're just waiting a bit.
	delay(1);
}