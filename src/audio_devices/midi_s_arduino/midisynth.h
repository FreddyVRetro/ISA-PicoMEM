/*
    MidiSynth
    https://github.com/DLehenbauer/arduino-midi-sound-module

    Extends Synth with methods and required state for processing MIDI messages.
*/

#ifndef __MIDISYNTH_H__
#define __MIDISYNTH_H__

#include <stdint.h>
#include "synth.h"

class MidiSynth final : public Synth {
  private:
    constexpr static uint8_t numMidiChannels    = 16;                   // MIDI standard has 16 channels.
    constexpr static uint8_t maxMidiChannel     = numMidiChannels - 1;  // Maximum channel is 15 when 0-indexed.
    constexpr static uint8_t percussionChannel  = 9;                    // Channel 10 is percussion (9 when 0-indexed).

    static uint8_t voiceToNote[numVoices];                      // Map synth voice to the current MIDI note (or 0xFF if off).
    static uint8_t voiceToChannel[numVoices];                   // Map synth voice to the current MIDI channel (or 0xFF if off).
    static Instrument channelToInstrument[numMidiChannels];     // Map MIDI channel to the current MIDI program (i.e., instrument).
    static uint8_t channelToVolume[numMidiChannels];            // Map MIDI channel to the current 8-bit instrument volume.
    static uint8_t voiceToVolume[numVoices];                    // Map synth voice to 8-bit volume scalar.
    static uint8_t voiceToVelocity[numVoices];                  // Map synth voice to 7-bit velocity scalar.

  public:
    MidiSynth() : Synth() {
      for (int8_t channel = maxMidiChannel; channel >= 0; channel--) {
        Instruments::getInstrument(0, channelToInstrument[channel]);
      }
    }

    void midiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
      if (channel == percussionChannel) {               // If playing the percussion channel
        note = Instruments::getPercussiveInstrument(    //   Update the channel instrument for the given note, and
          note, channelToInstrument[channel]);          //   replace the note with the correct playback frequency
      }                                                 //   for the instrument (expressed as a midi note).

      uint8_t voice = getNextVoice();                   // Find an available voice and play the note.

      // Combine the incoming velocity with the current channel volume.
      voiceToVelocity[voice] = velocity;
      uint8_t volume = mixVolume(channelToVolume[channel], velocity);

      noteOn(voice, note, volume, channelToInstrument[channel]);

      voiceToNote[voice] = note;                        // Update our voice -> note/channel maps (used for processing MIDI
      voiceToChannel[voice] = channel;                  // pitch bend and note off messages).
    }

    void midiNoteOff(uint8_t channel, uint8_t note)  {
      for (int8_t voice = maxVoice; voice >= 0; voice--) {                      // For each voice
        if (voiceToNote[voice] == note && voiceToChannel[voice] == channel) {   //   that is currently playing the note on this channel
          noteOff(voice);                                                       //      stop playing the note
        }
      }
    }

    void midiProgramChange(uint8_t channel, uint8_t program) {
      Instruments::getInstrument(program, channelToInstrument[channel]);        // Load the instrument corresponding to the given MIDI program
    }                                                                           // into the MIDI channel -> instrument map.

    void midiPitchBend(uint8_t channel, int16_t value) {
      for (int8_t voice = maxVoice; voice >= 0; voice--) {                      // For each voice
        if (voiceToChannel[voice] == channel) {                                 //   which is currently playing a note on this channel
          pitchBend(voice, value);                                              //     update pitch bench with the given value.
        }
      }
    }

    // Combines the given channel volume and note velocity into a single volume for the
    // synth.  (Inputs and outputs are all 7 bit).
    uint8_t mixVolume(uint8_t volume, uint8_t velocity) {
      // Map 8-bit channel volume and 7-bit velocity to 7-bit total volume.  We've scaled MIDI
      // channel volume up to 8-bit so that we can perform an inexpensive '>> 8'.
      return (static_cast<uint16_t>(volume) * static_cast<uint16_t>(velocity)) >> 8;
    }
  
    void midiControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
      switch (controller) {
        // Set channel volume
        case 0x07: {
          // Scale 7-bit MIDI channel volume up to 8-bit.  This reduces the instructions require to convert
          // from 16-bit to 8-bit later in mixVolume().
          value <<= 1;

          channelToVolume[channel] = value;
          for (int8_t voice = maxVoice; voice >= 0; voice--) {      // For each voice
            if (voiceToChannel[voice] == channel) {                 //   currently playing any note on this channel
              // Combine the incoming channel volume with the current note velocity.
              setVolume(voice, mixVolume(value, voiceToVelocity[voice]));
            }
          }
          break;
        }
        
        case 0x78: {
          switch (value) {
            case 0: {
              // All Sound Off (for current channel):
              for (int8_t voice = maxVoice; voice >= 0; voice--) {  // For each voice
                if (voiceToChannel[voice] == channel) {             //   currently playing any note on this channel
                  noteOff(voice);                                   //     stop playing the note
                  setVolume(voice, 0);                              //     immediately silence the voice
                }
              }
              break;
            }
          }
          break;
        }
        
        case 0x7B: {
          switch (value) {
            case 0: {                                                    
              // All Notes Off (for current channel):
              for (int8_t voice = maxVoice; voice >= 0; voice--) {  // For each voice
                if (voiceToChannel[voice] == channel) {             //   currently playing any note on this channel
                  noteOff(voice);                                   //     stop playing the note
                }
              }
              break;
            }
          }
          break;
        }
      }
    }
}; //MidiSynth

uint8_t     MidiSynth::voiceToNote[numVoices]               = { 0 };
uint8_t     MidiSynth::voiceToChannel[numVoices]            = { 0 };
Instrument  MidiSynth::channelToInstrument[numMidiChannels] = { 0 };
uint8_t     MidiSynth::channelToVolume[numMidiChannels]     = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t     MidiSynth::voiceToVolume[numVoices]             = { 0 };
uint8_t     MidiSynth::voiceToVelocity[numVoices]           = { 0 };

#endif //__MIDISYNTH_H__
