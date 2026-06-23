/**
 * This example can be used together with a Teensy 2.0 or later to use the OPL3Duo as a MIDI device. To configure the
 * Teensy as a MIDI device set USB Type to MIDI in the IDE using Tools > USB Type > MIDI. Once connected the board
 * should appear in the device list as 'OPL3Duo MIDI'. You can now use test the board with, for example, MIDI-OX, your
 * favorite music creation software or DosBox!
 *
 * Code by Maarten Janssen, 2020-06-20
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL3Duo.h>
#include <midi_instruments_4op.h>
#include <midi_drums.h>
#include "TeensyMidi.h"


#define CONTROL_MODULATION      1
#define CONTROL_VOLUME          7
#define CONTROL_ALL_SOUND_OFF 120
#define CONTROL_RESET_ALL     121
#define CONTROL_ALL_NOTES_OFF 123
#define PI2 6.28318



OPL3Duo opl3;

MidiChannel midiChannels[NUM_MIDI_CHANNELS];
OPLChannel melodicChannels[NUM_MELODIC_CHANNELS];
OPLChannel drumChannels[NUM_DRUM_CHANNELS];
unsigned long midiEventIndex = 0;


/**
 * Register MIDI event handlers and initialize.
 */
void setup() {
	usbMIDI.setHandleNoteOn(onNoteOn);
	usbMIDI.setHandleNoteOff(onNoteOff);
	usbMIDI.setHandleProgramChange(onProgramChange);
	usbMIDI.setHandleControlChange(onControlChange);
	usbMIDI.setHandlePitchChange(onPitchChange);
	usbMIDI.setHandleAfterTouch(onAfterTouch);
	usbMIDI.setHandleSystemReset(onSystemReset);
	onSystemReset();
}


/**
 * Read and handle MIDI events.
 */
void loop() {
	usbMIDI.read();

	for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
		byte midiChannel = melodicChannels[i].midiChannel;
		float modulation = max(
			midiChannels[midiChannel].modulation,
			midiChannels[midiChannel].afterTouch
		);

		if (modulation > 0.0) {
			float tModulation  = (millis() - midiChannels[midiChannel].tAfterTouch) * (PI2 / 200);
			byte controlChannel = opl3.get4OPControlChannel(i);
			byte baseNote = (melodicChannels[i].note % 12) + 2;
			float fModulation = (notePitches[baseNote + 1] - notePitches[baseNote]) * modulation;
			float fDelta = (1.0 - ((cos(tModulation) * 0.5) + 0.5)) * fModulation;
			opl3.setFNumber(controlChannel, notePitches[baseNote] + fDelta);
		}
	}
}


void playMelodic(byte midiChannel, byte note, byte velocity) {
	midiChannel = midiChannel % NUM_MIDI_CHANNELS;

	byte program = midiChannels[midiChannel].program;
	byte oplChannelIndex = VALUE_UNDEFINED;

	// Look for the oldest free melodic channel with the same program. There should be at least 2 free channels with the
	// same program so we're not clubbering the release portion of an older note.
	unsigned long oldest = -1;
	byte sameProgramCount = 0;
	for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
		if (melodicChannels[i].program == program && melodicChannels[i].note == VALUE_UNDEFINED) { 
			sameProgramCount ++;
			if (melodicChannels[i].eventIndex < oldest) {
				oldest = melodicChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	// If there is no free channel or less than 2 with the smae program then look for any free channel.
	if (oplChannelIndex == VALUE_UNDEFINED || sameProgramCount < 2) {
		oldest = -1;
		for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
			if (melodicChannels[i].note == VALUE_UNDEFINED && melodicChannels[i].eventIndex < oldest) {
				oldest = melodicChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	// No free channels, recycle the oldest channel with the same program.
	if (oplChannelIndex == VALUE_UNDEFINED) {
		oldest = -1;
		for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
			if (melodicChannels[i].program == program && melodicChannels[i].eventIndex < oldest) {
				oldest = melodicChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	// Still no channel, simply recycle the oldest.
	if (oplChannelIndex == VALUE_UNDEFINED) {
		oldest = -1;
		for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
			if (melodicChannels[i].eventIndex < oldest) {
				oldest = melodicChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	if (oplChannelIndex != VALUE_UNDEFINED) {
		opl3.setKeyOn(opl3.get4OPControlChannel(oplChannelIndex), false);

		midiEventIndex ++;
		melodicChannels[oplChannelIndex].eventIndex = midiEventIndex;
		melodicChannels[oplChannelIndex].midiChannel = midiChannel;
		melodicChannels[oplChannelIndex].note = note;
		melodicChannels[oplChannelIndex].noteVelocity = log(min((float)velocity, 127.0)) / log(127.0);

		// If the program loaded on the OPL channel is differs from the MIDI channel, then first send new instrument
		// parameters to the OPL.
		if (melodicChannels[oplChannelIndex].program != program) {
			melodicChannels[oplChannelIndex].program = program;
			opl3.setFNumber(opl3.get4OPControlChannel(oplChannelIndex), 0);
			opl3.setInstrument4OP(
				oplChannelIndex,
				midiChannels[midiChannel].instrument,
				0.0
			);
		}
		setOplChannelVolume(oplChannelIndex, midiChannel);

		note = max(24, min(note, 119));
		byte octave = 1 + (note - 24) / 12;
		note = note % 12;
		opl3.playNote(
			opl3.get4OPControlChannel(oplChannelIndex),
			octave,
			note
		);
	}
}


void playDrum(byte note, byte velocity) {
	byte program;
	if (note >= DRUM_NOTE_BASE && note < DRUM_NOTE_BASE + NUM_MIDI_DRUMS) {
		program = note - DRUM_NOTE_BASE;
	} else {
		return;
	}

	byte oplChannelIndex = VALUE_UNDEFINED;
	unsigned long oldest = -1;

	// Look for the oldest free drum channel with the same program.
	oldest = -1;
	for (byte i = 0; i < NUM_DRUM_CHANNELS; i ++) {
		if (drumChannels[i].program == program && drumChannels[i].note == VALUE_UNDEFINED && drumChannels[i].eventIndex < oldest) {
			oldest = drumChannels[i].eventIndex;
			oplChannelIndex = i;
		}
	}

	// Look for any free drum channel.
	if (oplChannelIndex == VALUE_UNDEFINED) {
		oldest = -1;
		for (byte i = 0; i < NUM_DRUM_CHANNELS; i ++) {
			if (drumChannels[i].note == VALUE_UNDEFINED && drumChannels[i].eventIndex < oldest) {
				oldest = drumChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	// Look for the oldest drum channel with the same instrument.
	if (oplChannelIndex == VALUE_UNDEFINED) {
		oldest = -1;
		for (byte i = 0; i < NUM_DRUM_CHANNELS; i ++) {
			if (drumChannels[i].program == program && drumChannels[i].eventIndex < oldest) {
				oldest = drumChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	// Recycle the oldest drum channel if still all channels are occupied.
	if (oplChannelIndex == VALUE_UNDEFINED) {
		oldest = -1;
		for (byte i = 0; i < NUM_DRUM_CHANNELS; i ++) {
			if (drumChannels[i].eventIndex < oldest) {
				oldest = drumChannels[i].eventIndex;
				oplChannelIndex = i;
			}
		}
	}

	if (oplChannelIndex != VALUE_UNDEFINED) {
		opl3.setKeyOn(drumChannelsOPL[oplChannelIndex], false);

		midiEventIndex ++;
		drumChannels[oplChannelIndex].eventIndex = midiEventIndex;
		drumChannels[oplChannelIndex].note = note;
		drumChannels[oplChannelIndex].noteVelocity = log(min((float)velocity, 127.0)) / log(127.0);

		// If the program loaded on the OPL channel is differs from the MIDI channel, then first send new instrument
		// parameters to the OPL.
		if (drumChannels[oplChannelIndex].program != program) {
			Instrument drumInstrument = opl3.loadInstrument(midiDrums[program]);
			drumChannels[oplChannelIndex].program = program;
			drumChannels[oplChannelIndex].transpose = drumInstrument.transpose;
			opl3.setInstrument(
				drumChannelsOPL[oplChannelIndex],
				drumInstrument,
				log(min((float)velocity, 127.0)) / log(127.0)
			);
		} else {
			// setOplChannelVolume(oplChannelIndex, MIDI_DRUM_CHANNEL);
		}

		opl3.playNote(
			drumChannelsOPL[oplChannelIndex],
			drumChannels[oplChannelIndex].transpose / 12,
			drumChannels[oplChannelIndex].transpose % 12
		);
	}
}


/**
 * Set the volume of operators 1 and 2 of the given OPL2 channel according to the settings of the given MIDI channel.
 */
void setOplChannelVolume(byte channel4OP, byte midiChannel) {
	if (midiChannel == MIDI_DRUM_CHANNEL) {
		return;
	}

	Instrument4OP instrument = midiChannels[midiChannel].instrument;
	float volume = melodicChannels[channel4OP].noteVelocity * midiChannels[midiChannel].volume;
	for (byte i = 0; i < 2; i ++) {
		float op1Level = (float)(63 - instrument.subInstrument[i].operators[OPERATOR1].outputLevel) / 63.0;
		float op2Level = (float)(63 - instrument.subInstrument[i].operators[OPERATOR2].outputLevel) / 63.0;
		byte volumeOp1 = round(op1Level * volume * 63.0);
		byte volumeOp2 = round(op2Level * volume * 63.0);
		opl3.setVolume(opl3.get4OPControlChannel(channel4OP, i), OPERATOR1, 63 - volumeOp1);
		opl3.setVolume(opl3.get4OPControlChannel(channel4OP, i), OPERATOR2, 63 - volumeOp2);
	}
}


/**
 * Handle a note off MIDI event to stop playing a note.
 */
void onNoteOff(byte midiChannel, byte note, byte velocity) {
	midiChannel = midiChannel % NUM_MIDI_CHANNELS;

	if (midiChannel == MIDI_DRUM_CHANNEL) {
		for (byte i = 0; i < NUM_DRUM_CHANNELS; i ++) {
			if (drumChannels[i].note == note) {
				opl3.setKeyOn(drumChannelsOPL[i], false);
				drumChannels[i].note = VALUE_UNDEFINED;
			}
		}
	} else {
		for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
			if (melodicChannels[i].midiChannel == midiChannel && melodicChannels[i].note == note) {
				opl3.setKeyOn(opl3.get4OPControlChannel(i), false);
				melodicChannels[i].note = VALUE_UNDEFINED;
			}
		}
	}
}


/**
 * Handle a note on MIDI event to play a note.
 */
void onNoteOn(byte midiChannel, byte note, byte velocity) {
	midiChannel = midiChannel % NUM_MIDI_CHANNELS;

	// Treat notes with a velocity of 0 as note off.
	if (velocity == 0) {
		onNoteOff(midiChannel, note, velocity); 
	} else {
		if (midiChannel == MIDI_DRUM_CHANNEL) {
			playDrum(note, velocity);
		} else {
			playMelodic(midiChannel, note, velocity);
		}
	}
}


/**
 * Handle program changes. If the progranm change occurs on a melodic channel then load the instrument and store it with
 * the MIDI channel.
 */
void onProgramChange(byte midiChannel, byte program) {
	midiChannel = midiChannel % NUM_MIDI_CHANNELS;

	if (midiChannel != MIDI_DRUM_CHANNEL) {
		program = program % 128;
		const unsigned char *instrumentDataPtr = midiInstruments[program];
		Instrument4OP instrument = opl3.loadInstrument4OP(instrumentDataPtr);
 
		midiChannels[midiChannel].program = program;
		midiChannels[midiChannel].instrument = instrument;
	}
}


/**
 * Handle MIDI control changes on the given channel.
 */
void onControlChange(byte midiChannel, byte control, byte value) {
	midiChannel = midiChannel % NUM_MIDI_CHANNELS;

	switch (control) {

		// Change channel modulation.
		case CONTROL_MODULATION: {
			midiChannels[midiChannel].modulation = value / 127.0;
			break;
		}

		// Change volume of a MIDI channel. If volume is changed on a melodic channel then the change is applied
		// immediately.
		case CONTROL_VOLUME: {
			midiChannels[midiChannel].volume = log(min((float)value, 127.0)) / log(127.0);
			for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
				if (melodicChannels[i].midiChannel == midiChannel && melodicChannels[i].note != VALUE_UNDEFINED) {
					setOplChannelVolume(i, midiChannel);
				}
			}
			break;
		}

		// Reset all controller values.
		case CONTROL_RESET_ALL:
			for (byte i = 0; i < NUM_MIDI_CHANNELS; i ++) {
				midiChannels[i].volume = log(127.0 * 0.8) / log(127.0);
			}
			break;

		// Immediately silence all channels immedialtey, ignores release part of envelope.
		// Intentionally cascade into CONTROL_ALL_NOTES_OFF!
		case CONTROL_ALL_SOUND_OFF:
			for (byte i = 0; i < opl3.getNumChannels(); i ++) {
				opl3.setRelease(i, OPERATOR1, 0);
				opl3.setRelease(i, OPERATOR2, 0);
				opl3.setKeyOn(i, false);
			}

		// Silence all MIDI channels, but still allow for note release to sound.
		case CONTROL_ALL_NOTES_OFF: {
			for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
				// if (oplDrumChannel[i].midiNote != 0x00) {
				// 	onNoteOff(MIDI_DRUM_CHANNEL, oplDrumChannel[i].midiNote, 0);
				// }
				if (melodicChannels[i].note != VALUE_UNDEFINED) {
					onNoteOff(melodicChannels[i].midiChannel, melodicChannels[i].note, 0);
				}
			}
			break;
		}

		// Ignore any other MIDI controls.
		default:
			break;
	}
}


/**
 * Apply a pitch bend to all notes currently playing on the given MIDI channel.
 * 
 * Note: Pitch bend is not applied to OPL channels that have a note in release, because when notes of different
 * frequencies are released changing their frequency may cause audible jitter.
 */
void onPitchChange(byte midiChannel, int pitch) {
	midiChannel = midiChannel % NUM_MIDI_CHANNELS;
	float pitchBend = abs(pitch) / 8192.0;

	for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
		if (melodicChannels[i].midiChannel == midiChannel) {
			byte controlChannel = opl3.get4OPControlChannel(i);
			byte baseNote = (melodicChannels[i].note % 12) + 2;

			if (pitch < 0) {
				byte fDelta = (notePitches[baseNote] - notePitches[baseNote - 2]) * pitchBend;
				opl3.setFNumber(controlChannel, notePitches[baseNote] - fDelta);
			} else if (pitch > 0) {
				byte fDelta = (notePitches[baseNote + 2] - notePitches[baseNote]) * pitchBend;
				opl3.setFNumber(controlChannel, notePitches[baseNote] + fDelta);
			} else {
				opl3.setFNumber(controlChannel, notePitches[baseNote]);
			}
		}
	}
}


void onAfterTouch(byte midiChannel, byte pressure) {
	if (midiChannels[midiChannel].afterTouch == 0.0) {
		midiChannels[midiChannel].tAfterTouch = millis();
	}
	midiChannels[midiChannel].afterTouch = pressure / 127.0;
}


/**
 * Handle full system reset.
 */
void onSystemReset() {
	opl3.begin();
	opl3.setDeepVibrato(true);
	opl3.setDeepTremolo(true);
	opl3.setOPL3Enabled(true);
	opl3.setAll4OPChannelsEnabled(true);

	// Default channel volume to 80%
	float defaultVolume = log(127.0 * 0.8) / log(127.0);

	// Reset default MIDI player parameters.
	for (byte i = 0; i < NUM_MIDI_CHANNELS; i ++) {
		onProgramChange(i, 0);
		midiChannels[i].volume = defaultVolume;
		midiChannels[i].modulation = 0.0;
		midiChannels[i].afterTouch = 0.0;
		midiChannels[i].tAfterTouch = 0;
	}

	// Initialize melodic channels.
	for (byte i = 0; i < NUM_MELODIC_CHANNELS; i ++) {
		melodicChannels[i].eventIndex = 0;
		melodicChannels[i].midiChannel = 0;
		melodicChannels[i].program = VALUE_UNDEFINED;
		melodicChannels[i].note = VALUE_UNDEFINED;
		melodicChannels[i].noteVelocity = 0.0;
	}

	// Initialize drum channels.
	for (byte i = 0; i < NUM_DRUM_CHANNELS; i ++) {
		drumChannels[i].eventIndex = 0;
		drumChannels[i].midiChannel = MIDI_DRUM_CHANNEL;
		drumChannels[i].program = VALUE_UNDEFINED;
		drumChannels[i].note = VALUE_UNDEFINED;
		drumChannels[i].noteVelocity = 0.0;
	}

	midiEventIndex = 0;
}


