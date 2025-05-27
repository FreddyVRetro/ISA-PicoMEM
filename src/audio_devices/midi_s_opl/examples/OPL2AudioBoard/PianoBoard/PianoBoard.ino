/**
 * This is a simple demo sketch for the OPL2 library. It makes use of the Cheerful Electronic Piano Board
 * (https://www.tindie.com/products/cheerful/arduino-piano-board-2/) and assumes then you also have the
 * ArduinoPianoBoard library installed (see https://github.com/DhrBaksteen/ArduinoPianoBoard).
 *
 * This sketch makes a simple piano out of the OPL2 Board and the Piano Board. Use the piano keys to play a tune. User
 * key 1 will lower the octave, user key 2 will increase the octave. User key 3 is free for your own experiments!
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
 * Code by Maarten Janssen (maarten@cheerful.nl) 2020-04-10
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL2.h>
#include <PianoKeys.h>
#include <midi_instruments.h>

OPL2 opl2;
PianoKeys piano(7);

int octave = 4;
int chIndex = 0;
int keyChannel[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};


void setup() {
	opl2.begin();

	// Load an instrument and assign it to all OPL2 channels.
	Instrument piano = opl2.loadInstrument(INSTRUMENT_CRYSTAL);
	for (int i = 0; i < 9; i ++) {
		opl2.setInstrument(i, piano);
	}
}


void loop() {
	piano.updateKeys();

	// Handle keys that are being pressed or held down.
	for (int i = KEY_C; i <= KEY_C2; i ++) {
		// Handle a key press.
		if (piano.wasKeyPressed(i)) {
			// Find a free OPL2 channel where we can play the note.
			int opl2Channel = getFreeChannel();

			// If a channel is available then register it as used and play the note.
			if (opl2Channel > -1) {
				keyChannel[opl2Channel] = i ;
				if (i == KEY_C2) {
					opl2.playNote(opl2Channel, octave + 1, NOTE_C);
				} else {
					opl2.playNote(opl2Channel, octave, i);
				}
			}
		}

		// Handle a key release.
		if (piano.wasKeyReleased(i)) {
			// Find the OPL2 channel where the not is playing.
			int opl2Channel = getKeyChannel(i);

			// If the channel is found then mark it unused and stop playing the note.
			if (opl2Channel > -1) {
				keyChannel[opl2Channel] = -1;
				opl2.setKeyOn(opl2Channel, false);   
			}
		}
	}

	// Lower the octave when user key 1 is pressed.
	if (piano.wasKeyPressed(KEY_USER_1) && octave > 0) {
		octave --;
	}

	// Raise the octave when user key 2 is pressed.
	if (piano.wasKeyPressed(KEY_USER_2) && octave < 6) {
		octave ++;
	}

	delay(20);
}


/**
 * Look for a free channel on the OPL2 that we can use to play a note on. There is a prefered channel that is
 * incremented each time a note is played. This is to allow for the sound of a node to fade out when a key is released.
 * If the prefered channel is already in use then look for an available channel across all available ones.
 * 
 * @return {int} The index of the OPL2 channel to use or -1 if no channels are available.
 */
int getFreeChannel() {
	int opl2Channel = -1;

	// If the prefered channel is available then use it.
	if (keyChannel[chIndex] == -1) {
		opl2Channel = chIndex;
		chIndex = (chIndex + 1) % 9;

	// The preferd channel is used, so look for one that is available.
	} else {
		for (int i = 0; i < 9; i ++) {
			if (keyChannel[i] == 0) {
				opl2Channel = i;
				break;
			}
		}
	}

	return opl2Channel;
}


/**
 * Get the OPL2 channel that is playing the given note.
 * 
 * @param {int} key - The index of the PianoBoard key for which to find the associated channel.
 * @return {int} The OPL2 channel that is playing the note of the given key or -1 if note is not playing.
 */
int getKeyChannel(int key) {
	for (int i = 0; i < 9; i ++) {
		if (keyChannel[i] == key) {
			return i;
		}
	}

	return -1;
}
