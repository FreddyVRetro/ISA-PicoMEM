/**
 * This is a demonstration sketch for the OPL3 Duo! This example shows how the panning bits of the OPL3 can be used. It
 * will play a simple ping sound on the left, right and on both speakers in order.
 *
 * Code by Maarten Janssen, 2020-08-23
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <OPL3Duo.h>


OPL3Duo opl3Duo;

void setup() {
	// Initialize the OPL3Duo! and enable OPL3 features (requered to enable panning support).
	opl3Duo.begin();
	opl3Duo.setOPL3Enabled(true);

	// Define a simple ping sound on channel 0.
	for (byte i = 0; i < 2; i ++) {
		opl3Duo.setAttack(0, i, 8);
		opl3Duo.setDecay(0, i, 1);
		opl3Duo.setSustain(0, i, 0);
		opl3Duo.setRelease(0, i, 4);
		opl3Duo.setMaintainSustain(0, i, true);
		opl3Duo.setEnvelopeScaling(0, i, true);
		opl3Duo.setMultiplier(0, i, 1);
		opl3Duo.setScalingLevel(0, i, 1);
		opl3Duo.setVolume(0, i, 0);
	}
	opl3Duo.setSynthMode(0, true);
}


void loop() {
	// Left speaker.
	opl3Duo.setPanning(0, true, false);
	opl3Duo.playNote(0, 5, NOTE_A);
	delay(200);
	opl3Duo.setKeyOn(0, false);

	delay(300);

	// Right speaker.
	opl3Duo.setPanning(0, false, true);
	opl3Duo.playNote(0, 5, NOTE_A);
	delay(200);
	opl3Duo.setKeyOn(0, false);

	delay(300);

	// Both speakers.
	opl3Duo.setPanning(0, true, true);
	opl3Duo.playNote(0, 5, NOTE_A);
	delay(200);
	opl3Duo.setKeyOn(0, false);

	delay(800);
}