/*
    Instrument type definitions
    https://github.com/DLehenbauer/arduino-midi-sound-module
*/

#ifndef __INSTRUMENT_H__
#define __INSTRUMENT_H__

#include <avr/pgmspace.h>

struct EnvelopeStage {
  int16_t slope;                  // Q8.8 fixed point slope for the EnvelopeStage
  int8_t limit;                   // Limit at which envelope will advance to the next stage
};

struct EnvelopeProgram {
  const EnvelopeStage* start;     // Pointer to first EnvelopeStage in Instruments::EnvelopeStages
  uint8_t initialValue;           // Initial value of the envelope generator
  uint8_t loopStartAndEnd;        // loop start and end indexes nibbles packed into a byte.
};

enum InstrumentFlags : uint8_t {
  InstrumentFlags_None             = 0,
  InstrumentFlags_Noise            = (1 << 0),   // Instrument XOR is periodically clobbered with a random value (white noise)
  InstrumentFlags_HalfAmplitude    = (1 << 1),   // Note velocity is halved, reducing volume (allows some reuse of EnvelopePrograms for softer instruments)
  InstrumentFlags_SelectAmplitude  = (1 << 2),   // +0-3 offset to the amplitude EnvelopeProgram index depending on note played.
  InstrumentFlags_SelectWave       = (1 << 3),   // +0-196 offset to the wavetable pointer depending on the note played.
};

struct Instrument {
  const int8_t* wave;       // Pointer into the wavetable for the instrument
  uint8_t ampMod;           // Index of EnvelopeProgram for amplitude modulation
  uint8_t freqMod;          // Index of EnvelopeProgram for frequency modulation
  uint8_t waveMod;          // Index of EnvelopeProgram for wavetable offset modulation
  uint8_t xorBits;          // Xor applied to each wavetable sample
  InstrumentFlags flags;    // Instrument flags, per above.
};

// Convenience wrapper around memcpy_P that infers the copy size for the src/dest type.
template <typename T> void PROGMEM_copy(const T* src, T& dest) {
  memcpy(&dest, src, sizeof(T));
}

class Instruments {
  private:
    #include "instruments_generated.h"

  public:
    static void getInstrument(uint8_t index, Instrument& instrument) {
      PROGMEM_copy(&instruments[index], instrument);
    }

    static uint8_t getPercussiveInstrument(uint8_t note, Instrument& instrument) {
      /* TODO: Support additional GS/GM2 percussion
    
        27 High Q
        28 Slap
        29 Scratch Push
        30 Scratch Pull
        31 Sticks
        32 Square Click
        33 Metronome Click
        34 Metronome Bell
        ...
        81 Shaker
        82 Jingle Bell
        83 Belltree
        84 Castanets
        85 Mute Surdo
        86 Open Surdo
        */

      uint8_t index = note - 35;                      // Calculate the the index of the percussion instrument relative
      if (index > 45) { index = 45; }                 // to the beginning of the percussion instruments (i.e., less 128).

      Instruments::getInstrument(0x80 + index,        // Load the percussion instrument
        instrument);                                  // (Note: percussion instruments begin at 128)

      return pgm_read_byte(&percussionNotes[index]);  // Return the frequency (i.e., midi note) to play the instrument.
    }

    static void getEnvelopeProgram(uint8_t programIndex, EnvelopeProgram& program) {
      PROGMEM_copy(&EnvelopePrograms[programIndex], program);
    }

    static void getEnvelopeStage(const EnvelopeStage* pStart, EnvelopeStage& stage) {
      PROGMEM_copy(pStart, stage);
    }

  #ifdef __EMSCRIPTEN__
    static const HeapRegion<uint8_t> getPercussionNotes() {
      return HeapRegion<uint8_t>(&percussionNotes[0], sizeof(percussionNotes));
    }

    static const HeapRegion<int8_t> getWavetable() {
      return HeapRegion<int8_t>(&Waveforms[0], sizeof(Waveforms));
    }

    static const HeapRegion<EnvelopeProgram> getEnvelopePrograms() {
      return HeapRegion<EnvelopeProgram>(&EnvelopePrograms[0], sizeof(EnvelopePrograms));
    }

    static const HeapRegion<EnvelopeStage> getEnvelopeStages() {
      return HeapRegion<EnvelopeStage>(&EnvelopeStages[0], sizeof(EnvelopeStages));
    }

    static const HeapRegion<Instrument> getInstruments() {
      return HeapRegion<Instrument>(&instruments[0], sizeof(instruments));
    }
  #endif // __EMSCRIPTEN__
};

constexpr EnvelopeStage Instruments::EnvelopeStages[] PROGMEM;
constexpr EnvelopeProgram Instruments::EnvelopePrograms[] PROGMEM;
constexpr Instrument Instruments::instruments[] PROGMEM;
constexpr int8_t Instruments::Waveforms[] PROGMEM;
constexpr uint8_t Instruments::percussionNotes[] PROGMEM;

#endif // __INSTRUMENT_H__
