#include <Arduino.h>

#define NUM_MIDI_CHANNELS    16
#define NUM_MELODIC_CHANNELS 12
#define NUM_DRUM_CHANNELS    12

#define MIDI_DRUM_CHANNEL    10
#define VALUE_UNDEFINED      255

// MIDI note number of the first drum sound.
#define DRUM_NOTE_BASE 27
#define NUM_MIDI_DRUMS 60


struct MidiChannel {
    Instrument4OP instrument;           // Current instrument.
    byte program;                       // Program number currenly associated witht this MIDI channel.
    float volume;                       // Channel volume.
    float modulation;                   // Channel modulation.
    float afterTouch;                   // Channel aftertouch.
    unsigned long tAfterTouch;          // Aftertouch start.
};


struct OPLChannel {
    unsigned long eventIndex;           // Midi event index to determine oldest channel to reuse.
    byte midiChannel;                   // Midi channel associated with the event.
    byte program;                       // Program number of the instrument loaded on the OPL channel.
    byte note;                          // Note number playing on the channel (0xFF when channel is free).
    byte transpose;                     // Transpose notes on this OPL channel for drums.
    float noteVelocity;                 // Velocity of the note on event.
};


// Note FNumbers per octave +/- 2 semitones for pitch bend.
const unsigned int notePitches[16] = {
	0x132, 0x144,
	0x156, 0x16B, 0x181, 0x198, 0x1B0, 0x1CA,
	0x1E5, 0x202, 0x220, 0x241, 0x263, 0x287,
	0x2AC, 0x2D6
};

// OPL channels used for drums.
const byte drumChannelsOPL[12] = {
	 6,  7,  8, 15, 16, 17,
	24, 25, 26, 33, 34, 35
};


void setup();
void loop();
byte getFreeMelodicChannel();
byte getFreeDrumChannel();
void playDrum(byte note, byte velocity);
void playMelodic(byte midiChannel, byte note, byte velocity);
void setOplChannelVolume(byte channel4OP, byte midiChannel);
void onNoteOff(byte midiChannel, byte note, byte velocity);
void onNoteOn(byte midiChannel, byte note, byte velocity);
void onProgramChange(byte midiChannel, byte program);
void onControlChange(byte midiChannel, byte control, byte value);
void onPitchChange(byte midiChannel, int pitch);
void onSystemReset();
