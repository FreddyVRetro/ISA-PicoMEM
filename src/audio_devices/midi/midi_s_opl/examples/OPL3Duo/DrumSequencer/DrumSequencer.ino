/**
 * This is a demonstration sketch for the OPL3 Duo! It implements a simple 16-step drum sequencer using 4-operator drum
 * instruments.
 *
 * Code by Maarten Janssen, 2021-01-18
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <OPL3Duo.h>
#include <midi_drums_4op.h>

struct Pattern {
	Instrument4OP instrument;
	bool isPlaying;
	byte steps[16];
};

OPL3Duo opl3Duo;

// Setup a demo sequence.
// A sequence consists of 12 Patterns. Each pattern can have its own instrument, a boolean to indicate whether the
// pattern is to be played and an array of 16 bytes where a value other than 0 indicates that there should be a drum
//sound playing at that step in the sequence.

const byte BPM = 110;
Pattern sequence[OPL3DUO_NUM_4OP_CHANNELS]  = {
	{ opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), true,  { 1,0,0,0,1,0,0,0,1,0,0,0,1,0,1,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_SNARE_AC), true,  { 0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,1 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_CLAP),     true,  { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_HIHAT_CL), true,  { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_LO_WDBLK), true,  { 0,0,1,0,0,0,1,0,1,1,0,1,0,0,0,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_HI_WDBLK), true,  { 1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_HI_AGOGO), true,  { 1,0,1,0,0,0,0,0,1,1,0,1,0,0,0,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_LO_AGOGO), true,  { 0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
	{ opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}
};


//const byte BPM = 90;
//Pattern sequence[OPL3DUO_NUM_4OP_CHANNELS]  = {
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), true,  { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), true,  { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), true,  { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR2), true,  { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_TAIKO),    true,  { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_SNARE_AC), true,  { 1,1,1,1,1,1,1,1,1,0,0,0,1,0,0,0 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }},
//  { opl3Duo.loadInstrument4OP(DRUMINS_BASS_DR1), false, { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}
//};


unsigned long tTick = 60000 / (BPM * 4);
byte step = 0;

void setup() {    
	opl3Duo.begin();
	opl3Duo.setOPL3Enabled(true);
	opl3Duo.setAll4OPChannelsEnabled(true);

	// Assign instruments to all channels.
	for (byte i = 0; i < OPL3DUO_NUM_4OP_CHANNELS; i ++) {
		opl3Duo.setInstrument4OP(i, sequence[i].instrument);
	}
}


void loop() {
	unsigned long t = millis();

	for (byte i = 0; i < OPL3DUO_NUM_4OP_CHANNELS; i ++) {
		if (sequence[i].isPlaying && sequence[i].steps[step]) {
			byte note = sequence[i].instrument.subInstrument[0].transpose;
			byte octave = note / 12;
			note = note % 12;

			byte ch4op = opl3Duo.get4OPControlChannel(i);
			opl3Duo.playNote(ch4op, octave, note);
		}
	}

	step = (step + 1) % 16;
	delay(tTick - (millis() - t));
}
