#include <OPL3Duo.h>

#define TUNE_CMD_END '\0'
#define TUNE_CMD_INSTRUMENT 'I'
#define TUNE_CMD_NOTE_A 'A'
#define TUNE_CMD_NOTE_G 'G'
#define TUNE_CMD_NOTE_DOUBLE '.'
#define TUNE_CMD_NOTE_FLAT '-'
#define TUNE_CMD_NOTE_LENGTH 'L'
#define TUNE_CMD_NOTE_SHARP '#'
#define TUNE_CMD_NOTE_SHARP2 '+'
#define TUNE_CMD_OCTAVE 'O'
#define TUNE_CMD_OCTAVE_DOWN '<'
#define TUNE_CMD_OCTAVE_UP '>'
#define TUNE_CMD_PAUSE 'P'
#define TUNE_CMD_REST 'R'
#define TUNE_CMD_TEMPO 'T'
#define TUNE_CMD_VOLUME 'V'


#define TP_NUM_CHANNELS 12
#define TP_NAN 255


struct Voice {
	const char* pattern;			// String of commands to be played.
	unsigned long position;			// Position within the pattern string.
	byte ticks;						// Number of ticks left until the next command.
	Instrument4OP instrument;		// Current instrument for this voice.
	float volume;					// Current volume of this voice.
	byte octave;					// Current octave of this voice.
	byte defaultNoteLength;			// Current default not length for this voice.
	bool ended;						// Indicates that the voice has processed all commands.
	byte channel;					// OPL channel used by this voice.
};


struct Tune {
	byte numVoices;					// Number of voices in use.
	byte numEnded;					// Number of voices that has ended.
	unsigned long tickDuration;		// Duration of each tick in ms (tempo)
	unsigned long nextTick;			// Time in ms of the next tick.
	Voice voice[6];					// Data of each voice.
};


extern const unsigned char *midiInstruments[];
const byte notes[3][7] = {{ NOTE_A,  NOTE_B,  NOTE_C,  NOTE_D,  NOTE_E,  NOTE_F,  NOTE_G  },
						  { NOTE_GS, NOTE_AS, NOTE_B,  NOTE_CS, NOTE_DS, NOTE_E,  NOTE_FS },
						  { NOTE_AS, NOTE_C,  NOTE_CS, NOTE_DS, NOTE_F,  NOTE_FS, NOTE_GS }};


class TuneParser {
	public:
		TuneParser(OPL3Duo* opl3Ref);
		TuneParser(OPL2* opl2Ref);
		void begin();
		void play(const char* voice0);
		void play(const char* voice0, const char* voice1);
		void play(const char* voice0, const char* voice1, const char* voice2);
		void play(const char* voice0, const char* voice1, const char* voice2, const char* voice3);
		void play(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4);
		void play(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4, const char* voice5);
		Tune playBackground(const char* voice0);
		Tune playBackground(const char* voice0, const char* voice1);
		Tune playBackground(const char* voice0, const char* voice1, const char* voice2);
		Tune playBackground(const char* voice0, const char* voice1, const char* voice2, const char* voice3);
		Tune playBackground(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4);
		Tune playBackground(const char* voice0, const char* voice1, const char* voice2, const char* voice3, const char* voice4, const char* voice5);
		Tune createTune(const char* voices[6], int numVoices);
		void playTune(Tune& tune);
		void restartTune(Tune& tune);
		bool tuneEnded(Tune& tune);
		unsigned long update(Tune& tune);
		bool parseTuneCommand(Tune& tune, byte voiceIndex);
		void parseNote(Voice& voice);
		void parseRest(Voice& voice);
		byte parseNoteLength(Voice voice);
		byte parseNumber(Voice voice, int nMin, int nMax);

	private:
		OPL3Duo* opl3 = NULL;
		byte oplChannel4OP = 0;
		bool channelInUse[TP_NUM_CHANNELS] = {
			false, false, false, false, false, false,
			false, false, false, false, false, false
		};
};
