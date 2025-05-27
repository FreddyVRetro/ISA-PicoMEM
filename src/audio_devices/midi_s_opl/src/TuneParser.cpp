#include "TuneParser.h"


/**
 * TuneParser constructor.
 *
 * @param opl3Ref - Reference to the OPL3Duo instance used for playback.
 */
TuneParser::TuneParser(OPL3Duo* opl3Ref) {
	opl3 = opl3Ref;
}


/**
 * Initialize the TuneParser.
 */
void TuneParser::begin() {
	opl3->begin();
	opl3->setOPL3Enabled(true);
	opl3->setAll4OPChannelsEnabled(true);
}


/**
 * Play the given tune with a 1 voice command string.
 *
 * @param voice0 - Command string for voice 0.
 */
void TuneParser::play(const char* voice0) {
	const char* voices[6] = { voice0, NULL, NULL, NULL, NULL, NULL };
	Tune tune = createTune(voices, 1);
	playTune(tune);
}


/**
 * Play the given tune with 2 voices from command strings.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 */
void TuneParser::play(const char* voice0, const char* voice1) {
	const char* voices[6] = { voice0, voice1, NULL, NULL, NULL, NULL };
	Tune tune = createTune(voices, 2);
	playTune(tune);
}


/**
 * Play the given tune with 3 voices from command strings.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 */
void TuneParser::play(const char* voice0, const char* voice1, const char* voice2) {
	const char* voices[6] = { voice0, voice1, voice2, NULL, NULL, NULL };
	Tune tune = createTune(voices, 3);
	playTune(tune);
}


/**
 * Play the given tune with 4 voices from command strings.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @param voice3 - Command string for voice 3.
 */
void TuneParser::play(const char* voice0, const char* voice1, const char* voice2, const char* voice3) {
	const char* voices[6] = { voice0, voice1, voice2, voice3, NULL, NULL };
	Tune tune = createTune(voices, 4);
	playTune(tune);
}


/**
 * Play the given tune with 5 voices from command strings.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @param voice3 - Command string for voice 3.
 * @param voice4 - Command string for voice 4.
 */
void TuneParser::play(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4) {
	const char* voices[6] = { voice0, voice1, voice2, voice3, voice4, NULL };
	Tune tune = createTune(voices, 5);
	playTune(tune);
}


/**
 * Play the given tune with 6 voices from command strings.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @param voice3 - Command string for voice 3.
 * @param voice4 - Command string for voice 4.
 * @param voice5 - Command string for voice 5.
 */
void TuneParser::play(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4, const char* voice5) {
	const char* voices[6] = { voice0, voice1, voice2, voice3, voice4, voice5 };
	Tune tune = createTune(voices, 6);
	playTune(tune);
}


/**
 * Initialize the given tune with 1 voice for playing in the background.
 *
 * @param voice0 - Command string for voice 0.
 * @return The tune ready to be played by calling update.
 */
Tune TuneParser::playBackground(const char* voice0) {
	const char* voices[6] = { voice0, NULL, NULL, NULL, NULL, NULL };
	return createTune(voices, 1);
}


/**
 * Initialize the given tune with 2 voices for playing in the background.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @return The tune ready to be played by calling update.
 */
Tune TuneParser::playBackground(const char* voice0, const char* voice1) {
	const char* voices[6] = { voice0, voice1, NULL, NULL, NULL, NULL };
	return createTune(voices, 2);
}


/**
 * Initialize the given tune with 3 voices for playing in the background.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @return The tune ready to be played by calling update.
 */
Tune TuneParser::playBackground(const char* voice0, const char* voice1, const char* voice2) {
	const char* voices[6] = { voice0, voice1, voice2, NULL, NULL, NULL };
	return createTune(voices, 3);
}


/**
 * Initialize the given tune with 4 voices for playing in the background.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @param voice3 - Command string for voice 3.
 * @return The tune ready to be played by calling update.
 */
Tune TuneParser::playBackground(const char* voice0, const char* voice1, const char* voice2, const char* voice3) {
	const char* voices[6] = { voice0, voice1, voice2, voice3, NULL, NULL };
	return createTune(voices, 4);
}


/**
 * Initialize the given tune with 5 voices for playing in the background.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @param voice3 - Command string for voice 3.
 * @param voice4 - Command string for voice 4.
 * @return The tune ready to be played by calling update.
 */
Tune TuneParser::playBackground(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4) {
	const char* voices[6] = { voice0, voice1, voice2, voice3, voice4, NULL };
	return createTune(voices, 5);
}


/**
 * Initialize the given tune with 6 voices for playing in the background.
 *
 * @param voice0 - Command string for voice 0.
 * @param voice1 - Command string for voice 1.
 * @param voice2 - Command string for voice 2.
 * @param voice3 - Command string for voice 3.
 * @param voice4 - Command string for voice 4.
 * @param voice5 - Command string for voice 5.
 * @return The tune ready to be played by calling update.
 */
Tune TuneParser::playBackground(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4, const char* voice5) {
	const char* voices[6] = { voice0, voice1, voice2, voice3, voice4, voice5 };
	return createTune(voices, 6);
}


/**
 * Create a Tune structure from the given array of command strings.
 *
 * @param voices - Array of command strings for each voice.
 * @param numVoices - The number of voices that is in use.
 */
Tune TuneParser::createTune(const char* voices[6], int numVoices) {
	Tune tune;

	tune.numVoices = min(numVoices, 6);
	for (byte i = 0; i < tune.numVoices; i ++) {
		tune.voice[i].pattern = voices[i];
	}

	restartTune(tune);

	return tune;
}


/**
 * Restart the given background tune.
 *
 * @param tune - The tune to restart.
 */
void TuneParser::restartTune(Tune& tune) {
	tune.numEnded = 0;
	tune.tickDuration = 60000 / (100 * 16);		// duration of a 64th note @ 100 bmp.
	tune.nextTick = millis();

	for (byte i = 0; i < tune.numVoices; i ++) {
		tune.voice[i].ended = false;
		tune.voice[i].position = 0;
		tune.voice[i].ticks = 1;
		tune.voice[i].octave = 4;
		tune.voice[i].volume = 0.8;
		tune.voice[i].instrument = opl3->loadInstrument4OP(midiInstruments[0]);
		tune.voice[i].defaultNoteLength = 16;
		tune.voice[i].channel = tune.numVoices;
	}
}


/**
 * Has the given tune finished playing?
 *
 * @retrun True if the tune has finished playing.
 */
bool TuneParser::tuneEnded(Tune& tune) {
	return tune.numVoices == tune.numEnded;
}


/**
 * Play the given tune.
 *
 * @param tune - The tun to be played.
 */
void TuneParser::playTune(Tune& tune) {
	while(!tuneEnded(tune)) {
		unsigned long wait = update(tune);
		delay(wait);
	}
}


/**
 * Update the tune parser to play the given tune in the background and return the number of milliseconds until it's time
 * to process the next tick. If it not yet time to process the next tick then this function will exit immediately and
 * return the number of milliseconds remaining until the next tick can be processed.
 *
 * @param tune - The tune to update.
 * @return The number of ms to wait until the next tick.
 */
unsigned long TuneParser::update(Tune& tune) {
	// Return immediately if it's not yet time to process the next tick.
	if (millis() < tune.nextTick) {
		return tune.nextTick - millis();
	}

	tune.nextTick = millis();
	unsigned long t = millis();

	for (byte i = 0; i < tune.numVoices; i ++) {
		if (!tune.voice[i].ended) {
			tune.voice[i].ticks --;

			if (tune.voice[i].ticks == 0) {
				opl3->setKeyOn(opl3->get4OPControlChannel(tune.voice[i].channel), false);
				channelInUse[tune.voice[i].channel] = false;

				// Parse commands until we find a note, a rest or the end of the tune for this voice.
				bool playingNote = false;
				while (!playingNote) {
					playingNote = parseTuneCommand(tune, i);
				}

				if (tune.voice[i].ended) {
					tune.numEnded ++;
				}
			}
		}
	}

	// Calculate time of next tick.
	unsigned long wait = tune.tickDuration - (millis() - t);
	tune.nextTick += wait;

	return wait;
}


/**
 * Parse the next command in the command string of the given tune and voice index and leave the data pointer at the next
 * command.
 *
 * @param tune - The tune that's being played.
 * @param voiceIndex - The index of the voice that is being parsed/
 */
bool TuneParser::parseTuneCommand(Tune& tune, byte voiceIndex) {
	bool isPlaying = false;
	Voice& voice = tune.voice[voiceIndex];

	// Get command character and convert to upper case.
	char command = pgm_read_byte_near(voice.pattern + voice.position);
	if (command >= 'a') {
		command -= 32;
	}

	switch (command) {
		// Handle end of tune for this voice.
		case TUNE_CMD_END: {
			voice.ended = true;
			isPlaying = true;
			break;
		}

		// Parse note and cascade into handling rest for note delay.
		case TUNE_CMD_NOTE_A ... TUNE_CMD_NOTE_G: {
			parseNote(voice);
		}

		// Handle a rest or pause in the tune.
		case TUNE_CMD_REST:
		case TUNE_CMD_PAUSE: {
			parseRest(voice);
			isPlaying = true;
			break;
		}

		// Handle 'On' to change octave.
		case TUNE_CMD_OCTAVE: {
			byte octave = parseNumber(voice, 0, 7);
			if (octave != TP_NAN) {
				voice.octave = octave;
			}
			break;
		}

		// Handle '>' to increment octave.
		case TUNE_CMD_OCTAVE_UP: {
			if (voice.octave < 7) {
				voice.octave ++;
			}
			break;
		}

		// Handle '<' to decrement octave.
		case TUNE_CMD_OCTAVE_DOWN: {
			if (voice.octave > 0) {
				voice.octave --;
			}
			break;
		}

		// Handle 'Tnnn' to change sone temop.
		case TUNE_CMD_TEMPO: {
			byte tempo = parseNumber(voice, 40, 250);
			if (tempo != TP_NAN) {
				tune.tickDuration = 60000 / (tempo * 16);
			}
			break;
		}

		// Handle 'Lnn' to change the default note length.
		case TUNE_CMD_NOTE_LENGTH: {
			voice.defaultNoteLength = parseNoteLength(voice);
			break;
		}

		// Handle 'Innn' to change the current instrument.
		case TUNE_CMD_INSTRUMENT: {
			byte instrumentIndex = parseNumber(voice, 0, 127);
			if (instrumentIndex != TP_NAN) {
				voice.instrument = opl3->loadInstrument4OP(midiInstruments[instrumentIndex]);
			}
			break;
		}

		// Handle 'Vnn' to change volume.
		case TUNE_CMD_VOLUME: {
			byte volume = parseNumber(voice, 0, 15);
			if (volume != TP_NAN) {
				voice.volume = (float)volume / 15.0;
			}
			break;
		}

		// ignore anything else.
		default:
			break;
	}

	voice.position ++;
	return isPlaying;
}


/**
 * Parse the note found at the current command string position of the given voice and play the note. The duration of the
 * note will be stored with the voice.
 *
 * @param voice - The voice from which to extract the note.
 */
void TuneParser::parseNote(Voice& voice) {
	byte noteIndex = pgm_read_byte_near(voice.pattern + voice.position);
	if (noteIndex >= 'a' && noteIndex <= 'g') {
		noteIndex = noteIndex - 'a';
	} else if (noteIndex >= 'A' && noteIndex <= 'G') {
		noteIndex = noteIndex - 'A';
	} else {
		return;
	}

	byte octave = voice.octave;
	byte note = notes[0][noteIndex];

	// Handle sharp and flat notes.
	char sharpFlat = pgm_read_byte_near(voice.pattern + voice.position + 1);
	if (sharpFlat == TUNE_CMD_NOTE_FLAT) {
		voice.position ++;
		note = notes[1][noteIndex];
		if (note == NOTE_B) {
			octave = max(octave - 1, 0);
		}
	} else if (sharpFlat == TUNE_CMD_NOTE_SHARP || sharpFlat == TUNE_CMD_NOTE_SHARP2) {
		voice.position ++;
		note = notes[2][noteIndex];
		if (note == NOTE_C) {
			octave = min(octave + 1, 7);
		}
	}

	// Find the next free channel to play the note on.
	for (byte i = 1; i <= TP_NUM_CHANNELS; i ++) {
		oplChannel4OP = (oplChannel4OP + i) % TP_NUM_CHANNELS;
		if (!channelInUse[oplChannel4OP]) {
			break;
		}
	}

	// Do some administartion and play the note!
	channelInUse[oplChannel4OP] = true;
	voice.channel = oplChannel4OP;
	opl3->setInstrument4OP(oplChannel4OP, voice.instrument);
	opl3->set4OPChannelVolume(oplChannel4OP, (1.0 - voice.volume) * 63);
	opl3->playNote(opl3->get4OPControlChannel(oplChannel4OP), octave, note);
}


/**
 * Determine the number of ticks until the next command should be parsed. This is uesed for both rests and notes to
 * determine their length. The number of ticks to wait is sotred with the given voice.
 *
 * @param voice - The voice for which we want to know the duration of the note or rest.
 */
void TuneParser::parseRest(Voice& voice) {
	byte ticks = parseNoteLength(voice);

	// Step through pattern data until we no longer find a digit.
	char digit;
	do {
		voice.position ++;
		digit = pgm_read_byte_near(voice.pattern + voice.position);
	} while (digit >= '0' && digit <= '9');

	// If note has a dot then add half of its duration.
	if (pgm_read_byte_near(voice.pattern + voice.position) == TUNE_CMD_NOTE_DOUBLE) {
		ticks += ticks / 2;
	}

	// Move position back one byte to not skip next command.
	voice.position --;

	// Set the number of ticks to hold the note.
	voice.ticks = ticks;
}


/**
 * Extract the note length at the current position of the command string for the given voice. If there is no number at
 * at the current position then return the default note lengt that ser set with the L command. Note the the '.' is not
 * taken into account in this function!
 *
 * @param voice - the voice from which to extract the note length.
 * @return The length of the note in ticks.
 */
byte TuneParser::parseNoteLength(Voice voice) {
	byte length = parseNumber(voice, 1, 64);

	if (length == TP_NAN) {
		return voice.defaultNoteLength;
	}

	// Reverse the bit order, so if duration is 16th note return 4 ticks.
	for (byte i = 6; i >= 0; i --) {
		if (length & 1 << i) {
			return 1 << (6 - i);
		}
	}

	return 16;
}


/**
 * Extract the number at the current command position of the given voice. The number is bounded by nMain and nMax. If
 * there is no number at the current command string position then TP_NAN is returned.
 *
 * @param voice - The voice where the number needs to be extracted from.
 * @param nMin - Minimum value of the number.
 * @param nMax - Maximum value of the number.
 * @return The number at the current command position in the voice or TP_NAN.
 */
byte TuneParser::parseNumber(Voice voice, int nMin, int nMax) {
	char nextDigit = pgm_read_byte_near(voice.pattern + voice.position + 1);
	if (nextDigit < '0' || nextDigit > '9') {
		return TP_NAN;
	}

	int number = 0;
	while(nextDigit >= '0' && nextDigit <= '9') {
		number *= 10;
		voice.position ++;
		number = number + pgm_read_byte_near(voice.pattern + voice.position) - '0';
		nextDigit = pgm_read_byte_near(voice.pattern + voice.position + 1);
	}

	return (byte)max(nMin, min(number, nMax));
}
