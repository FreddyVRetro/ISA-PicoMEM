/**
 * This is an example sketch for the OPL2 Audio Board that shows how MIDI controls can be used to change the properties
 * of an operator. In this example we assume that serial MIDI data is received from a MIDI shield connected to the Rx(1)
 * pin of the Arduino.
 *
 * The carriers of all OPL2 channels will be set to some preset values and with you MIDI controller or keyboard you can
 * play notes. The controls of the MIDI controller can be used to change ADRS, waveform and multiplier ot the carriers.
 *
 * In this example we assume that a MIDI controller is used that has at least 6 rotary encoders and that each rotary
 * encoder controls a different channel. The channel of the control determines what property of the carrier is changed.
 * If your MIDI controller is layed out differently you can adjust the changeControl function according to your setup.
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2020-11-23
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 * WWW.CHEERFUL.NL
 */

#include <OPL2.h>
#include <midi_instruments.h>

// MIDI statuses
#define MIDI_NOTE_OFF           0x80
#define MIDI_NOTE_ON            0x90
#define MIDI_POLY_AFTERTOUCH    0xA0
#define MIDI_CONTROL_CHANGE     0xB0
#define MIDI_PROGRAM_CHANGE     0xC0
#define MIDI_CHANNEL_AFTERTOUCH 0xD0
#define MIDI_SYSEX_START        0xF0
#define MIDI_SYSEX_END          0xF7

#define NUM_OPL2_CHANNELS 9
#define NO_NOTE 255

const byte MIN_NOTE = 24;
const byte MAX_NOTE = 119;

byte midiCommand = 0x00;
byte midiChannel = 0;
byte midiData[2] = { 0x00, 0x00 };
byte midiDataBytes = 0;
byte midiDataIndex = 0;

byte oplChannel = 0;
byte oplNotes[NUM_OPL2_CHANNELS] = {
	NO_NOTE, NO_NOTE, NO_NOTE,
	NO_NOTE, NO_NOTE, NO_NOTE,
	NO_NOTE, NO_NOTE, NO_NOTE
};

OPL2 opl2;
Instrument instrument;


void setup() {
	Serial.begin(31250);
	opl2.begin();

	// Set properties for all channels.
	opl2.setWaveFormSelect(true);
	for (byte i = 0; i < opl2.getNumChannels(); i ++) {
		opl2.setTremolo   (i, CARRIER, true);
		opl2.setVibrato   (i, CARRIER, true);
		opl2.setMultiplier(i, CARRIER, 0x04);
		opl2.setAttack    (i, CARRIER, 0x0A);
		opl2.setDecay     (i, CARRIER, 0x04);
		opl2.setSustain   (i, CARRIER, 0x08);
		opl2.setRelease   (i, CARRIER, 0x04);
		opl2.setVolume    (i, CARRIER, 0x00);
		opl2.setWaveForm  (i, CARRIER, 0x00);
		opl2.setMaintainSustain(i, CARRIER, false);
	}
}


void loop() {
	if (Serial.available() > 0 ) {
		byte data = Serial.read();

		// Is this a MIDI status?
		if (data & 0x80) {

			// Handle system exclusive data.
			if (data == MIDI_SYSEX_START) {
				handleSysExEvent();

			// For other MIDI statuses extract command, channel and perapre to receive data.
			} else {
				midiCommand = data & 0xF0;
				midiChannel = data & 0x0F;
				midiDataIndex = 0;
				midiDataBytes = 2;
				if (midiCommand == MIDI_PROGRAM_CHANGE || midiCommand == MIDI_CHANNEL_AFTERTOUCH) {
					midiDataBytes = 1;
				}
			}

		// Receive MIDI data.
		} else {
			midiData[midiDataIndex] = (data & 0x7F);
			midiDataIndex ++;

			// If all data for the MIDI command is received then handle it!
			if (midiDataIndex == midiDataBytes) {
				handleMidiEvent();
				midiDataIndex = 0;
			}
		}
	}
}


/**
 * Handle the current MIDI status.
 */
void handleMidiEvent() {
	switch (midiCommand) {
		case MIDI_NOTE_ON:
			playNote();
			break;
		case MIDI_NOTE_OFF:
			stopNote();
			break;
		case MIDI_POLY_AFTERTOUCH:
			break;
		case MIDI_CONTROL_CHANGE:
			changeControl();
			break;
		case MIDI_PROGRAM_CHANGE:
			break;
		case MIDI_CHANNEL_AFTERTOUCH:
			break;
		default:
			break;
	}
}


/**
 * Do not handle any system exclusive events. Just wait until the end of the system exclusive structure.
 */
void handleSysExEvent() {
	byte data = 0x00;
	while (data != MIDI_SYSEX_END) {
		if (Serial.available()) {
			data = Serial.read();
		}
	}
}


/**
 * Play a note on the next available OPL2 channel.
 */
void playNote() {
	byte note = midiData[0];
	byte velocity = midiData[1];

	// Register which note is playing on which channel.
	oplNotes[oplChannel] = note;

	// Adjust note to valid range and extract octave.
	note = max(MIN_NOTE, min(note, MAX_NOTE));
	byte octave = 1 + (note - 24) / 12;
	note = note % 12;
	opl2.playNote(oplChannel, octave, note);

	// Set OPL2 channel for the next note.
	oplChannel = (oplChannel + 1) % NUM_OPL2_CHANNELS;
}


/**
 * Stop playing a note by looking up its OPL2 channel and releasing the key.
 */
void stopNote() {
	byte note = midiData[0];
	byte velocity = midiData[1];

	for (byte i = 0; i < NUM_OPL2_CHANNELS; i ++) {
		if (oplNotes[i] == note) {
			oplNotes[i] = NO_NOTE;
			opl2.setKeyOn(i, false);
		}
	}
}


/**
 * Change some of the carrier properties on control changes. Here the control's channel is used to pick the property to
 * change. If it's more convenient to use the actual control numbers from your MIDI controller then use midiData[0] to
 * get the control number.
 */
void changeControl() {
	byte control = midiChannel;
	// byte control = midiData[0];
	byte value = midiData[1];
	stopAll();

	for (byte i = 0; i < NUM_OPL2_CHANNELS; i ++) {
		switch (control) {
			case 0:
				opl2.setAttack(i, CARRIER, value);
				break;
			case 1:
				opl2.setDecay(i, CARRIER, value);
				break;
			case 2:
				opl2.setSustain(i, CARRIER, value);
				break;
			case 3:
				opl2.setRelease(i, CARRIER, value);
				break;
			case 4:
				opl2.setWaveForm(i, CARRIER, value);
				break;
			case 5:
				opl2.setMultiplier(i, CARRIER, value);
				break;
			default:
				break;
		}
	}
}


/**
 * Immediately stop playing notes on all OPL2 channels when a control is changed.
 */
void stopAll() {
	for (byte i = 0; i < NUM_OPL2_CHANNELS; i ++) {
		opl2.setFNumber(i, 0);
		opl2.setKeyOn(i, false);
		oplNotes[i] = NO_NOTE;
	}
}
