/**
 * This is a demonstration sketch for the OPL3 Duo! This example demonstrates the use of 4-operator instruments and how
 * to play notes on a 4-op channel. It will play random ontes across all 12 4-operator channels.
 *
 * Code by Maarten Janssen, 2020-07-23
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <OPL3Duo.h>
#include <midi_instruments_4op.h>


OPL3Duo opl3Duo;
Instrument4OP instrument;

int ch4 = 0;		// 4-OP channel index.


void setup() {
	// Initialize the OPL3Duo! and enable OPL3 features (requered to enable 4-OP channel support).
	opl3Duo.begin();
	opl3Duo.setOPL3Enabled(true);

	// Load a 4-OP piano instrument.
	instrument = opl3Duo.loadInstrument4OP(INSTRUMENT_XYLO);

	// Enable all 4-OP channels and assign the same instrument to each channel.
	opl3Duo.setAll4OPChannelsEnabled(true);
	for (int i = 0; i < opl3Duo.getNum4OPChannels(); i ++) {
		opl3Duo.setInstrument4OP(i, instrument);
	}
}


void loop() {
	// Define a random note on the 4th or 5th octave.
	byte noteIndex = random(0, 24);
	byte octave = 4 + (noteIndex / 12);
	byte note = noteIndex % 12;

	// Get 2-OP channel that serves as the control channel for frequency, block and key-on for the 4-OP channel pair and
	// play the random note.
	byte controlChannel = opl3Duo.get4OPControlChannel(ch4);
	opl3Duo.playNote(controlChannel, octave, note);

	// play the next note on another 4-OP channel. We take the modulo of num4OPChannels here for good measure to keep
	// within a valid range. it is not strictly needed as the library will also do this internally when the 4-OP channel
	// index exceeds the number of available channels.
	ch4 = (ch4 + 1) % opl3Duo.getNum4OPChannels();

	delay(200);
}
