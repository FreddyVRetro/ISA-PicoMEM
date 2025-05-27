/**
 * This is a simple demo sketch for the OPL2 library. It plays a drum pattern using the MIDI drum patches to show how
 * you can create drum sounds that sound better than using the percussive mode (shown in the drums example). Notice that
 * the drum instruments define the note to be played in the transpose byte of the instrument.
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
 * Code by Maarten Janssen (maarten@cheerful.nl) 2024-07-23
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL2.h>
#include <midi_drums.h>


OPL2 opl2;
Instrument bass, snare, hihat, crash, cowbell, claves;
int ticks = 0;

void setup() {
	opl2.begin();

	// Load drum instruments and assign them to OPL2 channels.
	bass = opl2.loadInstrument(DRUMINS_BASS_DR1);
	opl2.setInstrument(0, bass);
	snare = opl2.loadInstrument(DRUMINS_SNARE_AC);
	opl2.setInstrument(1, snare);
	hihat = opl2.loadInstrument(DRUMINS_HIHAT_CL);
	opl2.setInstrument(2, hihat);
	crash = opl2.loadInstrument(DRUMINS_CRASH);
	opl2.setInstrument(3, crash);
	cowbell = opl2.loadInstrument(DRUMINS_COWBELL);
	opl2.setInstrument(4, cowbell);
	claves = opl2.loadInstrument(DRUMINS_CLAVES);
	opl2.setInstrument(5, claves);
}


void loop() {
	// Trigger the drum sounds depending on the current tick. The note and octave to be played is taken from the
	// instrument data.
	if (ticks % 4 == 0) opl2.playNote(0, bass.transpose / 12, bass.transpose % 12);
	if (ticks % 4 == 2) opl2.playNote(1, snare.transpose / 12, snare.transpose % 12);
	if (ticks % 1 == 0) opl2.playNote(2, hihat.transpose / 12, hihat.transpose % 12);
	if (ticks % 32 == 0) opl2.playNote(3, crash.transpose / 12, crash.transpose % 12);
	if (ticks % 3 == 0) opl2.playNote(4, cowbell.transpose / 12, cowbell.transpose % 12);
	if (ticks % 8 > 4) opl2.playNote(5, claves.transpose / 12, claves.transpose % 12);

	delay(100);

	// Switch all notes off.
	for (byte i = 0; i < 6; i ++) {
		opl2.setKeyOn(i, false);
	}

	delay(100);

	ticks ++;
}
