/*
    Arduino Midi Synth (Core Engine)
    https://github.com/DLehenbauer/arduino-midi-sound-module

    Optimized for ATMega328P @ 16mhz w/AVR8/GNU C Compiler : 5.4.0 (-Os):
      - 16 voices sampled & mixed in real-time at ~20kHz
      - Wavetable and white noise sources
      - Amplitude, frequency, and wavetable offset modulated by envelope generators
      - Additional volume control per voice (MIDI velocity and channel volume)

    Sample, mixing, and output occurs on the Timer 2 ISR.  (Timer 2 was chosen because
    it's PWM output pins conflict with SPI, making it ill suited for PWM output for this
    design.)
    
    Notes:
      - The sample/mix ISR is carefully arranged to minimize spilling registers to memory.
      
      - To avoid missing arriving MIDI messages, the Timer2 ISR re-enables interrupts so
        that it can be preempted by the USART RX ISR.
        
        (However, it disables Timer2 ISRs until it's ready to exit to avoid reentrancy.)
*/

#ifndef __SYNTH_H__
#define __SYNTH_H__

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <math.h>
#include <stdint.h>
#include "instruments.h"
#include "envelope.h"
#include "dac.h"

// With GCC, we can calculate the _noteToPitch table at compile time.
#ifndef __EMSCRIPTEN__
constexpr static uint16_t interval(double sampleRate, double note) {
  return round(pow(2, (note - 69.0) / 12.0) * 440.0 / sampleRate * static_cast<double>(0xFFFF));
}
#endif

class Synth {
  public:
    constexpr static uint8_t numVoices = 16;
    constexpr static uint8_t maxVoice = Synth::numVoices - 1;
    constexpr static uint8_t samplingInterval = 0x65      // 0x65 ~= 19.8 kHz
  #if DEBUG
      >> 1	// On Debug, halve sampling interval to avoid starving MIDI dispatch.
  #endif
      ;
  
  constexpr static double sampleRate = static_cast<double>(F_CPU) / 8.0 / static_cast<double>(Synth::samplingInterval);
  
  private:
    // Used in 'noteOn()' to shift wave offset or amplitude modulation program based on the current note played.
    // (e.g., to alter the waveform and sustain for instruments with a wide range like the piano where these
    // qualities vary based on the note played.)
    static constexpr uint8_t offsetTable[] = { 0, 0, 1, 1, 2, 3, 3, 3 };

    // Map MIDI notes [0..127] to the corresponding Q8.8 sampling interval
    constexpr static uint16_t _noteToSamplingInterval[] PROGMEM = {
    #ifndef __EMSCRIPTEN__
      interval(sampleRate, 0x00), interval(sampleRate, 0x01), interval(sampleRate, 0x02), interval(sampleRate, 0x03), interval(sampleRate, 0x04), interval(sampleRate, 0x05), interval(sampleRate, 0x06), interval(sampleRate, 0x07),
      interval(sampleRate, 0x08), interval(sampleRate, 0x09), interval(sampleRate, 0x0A), interval(sampleRate, 0x0B), interval(sampleRate, 0x0C), interval(sampleRate, 0x0D), interval(sampleRate, 0x0E), interval(sampleRate, 0x0F),
      interval(sampleRate, 0x10), interval(sampleRate, 0x11), interval(sampleRate, 0x12), interval(sampleRate, 0x13), interval(sampleRate, 0x14), interval(sampleRate, 0x15), interval(sampleRate, 0x16), interval(sampleRate, 0x17),
      interval(sampleRate, 0x18), interval(sampleRate, 0x19), interval(sampleRate, 0x1A), interval(sampleRate, 0x1B), interval(sampleRate, 0x1C), interval(sampleRate, 0x1D), interval(sampleRate, 0x1E), interval(sampleRate, 0x1F),
      interval(sampleRate, 0x20), interval(sampleRate, 0x21), interval(sampleRate, 0x22), interval(sampleRate, 0x23), interval(sampleRate, 0x24), interval(sampleRate, 0x25), interval(sampleRate, 0x26), interval(sampleRate, 0x27),
      interval(sampleRate, 0x28), interval(sampleRate, 0x29), interval(sampleRate, 0x2A), interval(sampleRate, 0x2B), interval(sampleRate, 0x2C), interval(sampleRate, 0x2D), interval(sampleRate, 0x2E), interval(sampleRate, 0x2F),
      interval(sampleRate, 0x30), interval(sampleRate, 0x31), interval(sampleRate, 0x32), interval(sampleRate, 0x33), interval(sampleRate, 0x34), interval(sampleRate, 0x35), interval(sampleRate, 0x36), interval(sampleRate, 0x37),
      interval(sampleRate, 0x38), interval(sampleRate, 0x39), interval(sampleRate, 0x3A), interval(sampleRate, 0x3B), interval(sampleRate, 0x3C), interval(sampleRate, 0x3D), interval(sampleRate, 0x3E), interval(sampleRate, 0x3F),
      interval(sampleRate, 0x40), interval(sampleRate, 0x41), interval(sampleRate, 0x42), interval(sampleRate, 0x43), interval(sampleRate, 0x44), interval(sampleRate, 0x45), interval(sampleRate, 0x46), interval(sampleRate, 0x47),
      interval(sampleRate, 0x48), interval(sampleRate, 0x49), interval(sampleRate, 0x4A), interval(sampleRate, 0x4B), interval(sampleRate, 0x4C), interval(sampleRate, 0x4D), interval(sampleRate, 0x4E), interval(sampleRate, 0x4F),
      interval(sampleRate, 0x50), interval(sampleRate, 0x51), interval(sampleRate, 0x52), interval(sampleRate, 0x53), interval(sampleRate, 0x54), interval(sampleRate, 0x55), interval(sampleRate, 0x56), interval(sampleRate, 0x57),
      interval(sampleRate, 0x58), interval(sampleRate, 0x59), interval(sampleRate, 0x5A), interval(sampleRate, 0x5B), interval(sampleRate, 0x5C), interval(sampleRate, 0x5D), interval(sampleRate, 0x5E), interval(sampleRate, 0x5F),
      interval(sampleRate, 0x60), interval(sampleRate, 0x61), interval(sampleRate, 0x62), interval(sampleRate, 0x63), interval(sampleRate, 0x64), interval(sampleRate, 0x65), interval(sampleRate, 0x66), interval(sampleRate, 0x67),
      interval(sampleRate, 0x68), interval(sampleRate, 0x69), interval(sampleRate, 0x6A), interval(sampleRate, 0x6B), interval(sampleRate, 0x6C), interval(sampleRate, 0x6D), interval(sampleRate, 0x6E), interval(sampleRate, 0x6F),
      interval(sampleRate, 0x70), interval(sampleRate, 0x71), interval(sampleRate, 0x72), interval(sampleRate, 0x73), interval(sampleRate, 0x74), interval(sampleRate, 0x75), interval(sampleRate, 0x76), interval(sampleRate, 0x77),
      interval(sampleRate, 0x78), interval(sampleRate, 0x79), interval(sampleRate, 0x7A), interval(sampleRate, 0x7B), interval(sampleRate, 0x7C), interval(sampleRate, 0x7D), interval(sampleRate, 0x7E), interval(sampleRate, 0x7F),
    #else
      // For samplingInterval = 0x65:
      0x001B, 0x001D, 0x001E, 0x0020, 0x0022, 0x0024, 0x0026, 0x0029, 0x002B, 0x002E, 0x0030, 0x0033, 0x0036, 0x0039, 0x003D, 0x0040,
      0x0044, 0x0048, 0x004D, 0x0051, 0x0056, 0x005B, 0x0060, 0x0066, 0x006C, 0x0073, 0x0079, 0x0081, 0x0088, 0x0090, 0x0099, 0x00A2,
      0x00AC, 0x00B6, 0x00C1, 0x00CC, 0x00D8, 0x00E5, 0x00F3, 0x0101, 0x0111, 0x0121, 0x0132, 0x0144, 0x0158, 0x016C, 0x0182, 0x0199,
      0x01B1, 0x01CB, 0x01E6, 0x0203, 0x0221, 0x0242, 0x0264, 0x0289, 0x02AF, 0x02D8, 0x0303, 0x0331, 0x0362, 0x0395, 0x03CC, 0x0406,
      0x0443, 0x0484, 0x04C9, 0x0511, 0x055E, 0x05B0, 0x0607, 0x0663, 0x06C4, 0x072B, 0x0798, 0x080B, 0x0886, 0x0908, 0x0991, 0x0A23,
      0x0ABD, 0x0B60, 0x0C0E, 0x0CC5, 0x0D87, 0x0E55, 0x0F30, 0x1017, 0x110C, 0x120F, 0x1322, 0x1445, 0x157A, 0x16C1, 0x181B, 0x198A,
      0x1B0F, 0x1CAB, 0x1E5F, 0x202D, 0x2217, 0x241E, 0x2644, 0x288B, 0x2AF4, 0x2D82, 0x3036, 0x3314, 0x361E, 0x3955, 0x3CBE, 0x405B,
      0x442F, 0x483C, 0x4C88, 0x5115, 0x55E7, 0x5B03, 0x606C, 0x6628, 0x6C3B, 0x72AB, 0x797C, 0x80B6, 0x885D, 0x9079, 0x9910, 0xA22A,
    #endif
    };

    // Note: Members prefixed with 'v_' (as in volatile) are shared with the ISR, and should only
    //       be accessed outside the ISR after calling 'suspend()' to suspend the ISR.

    static volatile const int8_t*    v_wave[Synth::numVoices];         // Starting address of 256b wave table.
    static volatile uint16_t         v_phase[Synth::numVoices];        // Phase accumulator holding the Q8.8 offset of the next sample.
    static volatile uint16_t         v_interval[Synth::numVoices];     // Q8.8 sampling interval, used to advance the '_phase' accumulator.
    static volatile int8_t           v_xor[Synth::numVoices];          // XOR bits applied to each sample (Note: clobbered if v_isNoise is true).
    static volatile uint8_t          v_amp[Synth::numVoices];          // 6-bit amplitude scale applied to each sample.
    static volatile bool             v_isNoise[Synth::numVoices];      // If true, '_xor' is periodically overwritten with random values.

    static volatile Envelope         v_ampMod[Synth::numVoices];       // Amplitude modulation (0 .. 127, although most instruments peak below 96)
    static volatile Envelope         v_freqMod[Synth::numVoices];      // Frequency modulation (-64 .. +64)
    static volatile Envelope         v_waveMod[Synth::numVoices];      // Wave offset modulation (0 .. 127)

    static volatile uint8_t          v_vol[Synth::numVoices];          // 7-bit volume scalar (0 .. 127)

    static          uint16_t         _baseInterval[Synth::numVoices];  // Original Q8.8 sampling internal, prior to modulation, pitch bend, etc.
    static volatile uint16_t         v_bentInterval[Synth::numVoices]; // Q8.8 sampling internal post pitch bend, but prior to freqMod.
    static volatile const int8_t*    v_baseWave[Synth::numVoices];     // Original starting address in wavetable.
    static          uint8_t          _note[Synth::numVoices];          // Index of '_baseInternal' in the '_noteToSamplintInterval' table (for 'pitchBend()').
  
  public:
    void begin(){
      Dac::setup();

      // Setup Timer2 for sample/mix/output ISR.
      TCCR2A = _BV(WGM21);                // CTC Mode (Clears timer and raises interrupt when OCR2B reaches OCR2A)
      TCCR2B = _BV(CS21);                 // Prescale None = C_FPU / 8 tick frequency
      OCR2A  = samplingInterval;          // Set timer top to sampling interval
      TIMSK2 = _BV(OCIE2A);               // Enable ISR
    }
  
    // Returns the next idle voice, if any.  If no voice is idle, uses envelope stage and amplitude to
    // choose the best candidate for note-stealing.
    uint8_t getNextVoice() {
      uint8_t current = maxVoice;
      uint8_t currentStage;
      int8_t currentAmp;
    
      {
        const volatile Envelope& currentMod    = v_ampMod[current];
        currentStage = currentMod.stageIndex;
        currentAmp = currentMod.value;
      }

      for (uint8_t candidate = maxVoice - 1; candidate < maxVoice; candidate--) {
        const volatile Envelope& candidateMod = v_ampMod[candidate];
        const uint8_t candidateStage = candidateMod.stageIndex;
      
        if (candidateStage >= currentStage) {                 // If the currently chosen voice is in a later amplitude stage, keep it.
          if (candidateStage == currentStage) {               // Otherwise, if both voices are in the same amplitude stage
            const int8_t candidateAmp = candidateMod.value;   //   compare amplitudes to determine which voice to prefer.
          
            bool selectCandidate = candidateMod.slope >= 0    // If amplitude is increasing...
              ? candidateAmp >= currentAmp                    //   prefer the lower amplitude voice
              : candidateAmp <= currentAmp;                   //   otherwise the higher amplitude voice

            if (selectCandidate) {
              current = candidate;
              currentStage = candidateStage;
              currentAmp = candidateAmp;
            }
          } else {
            current = candidate;                              // Else, if the candidate is in a later ADSR stage, prefer it.
            currentStage = candidateStage;
            currentAmp = candidateMod.value;
          }
        }
      }
    
      return current;
    }

    // Note that MidiSynth preprocesses both note velocity and channel volume into the single
    // volume scalar.
    void noteOn(uint8_t voice, uint8_t note, uint8_t volume, const Instrument& instrument) {
      const uint8_t flags = instrument.flags;
      
      if (flags & InstrumentFlags_HalfAmplitude) {                  // If the half-amplitude flag is set, play at 1/2 volume.  (Allows some
        volume >>= 1;                                               // reuse of amplitude envelopes for softer instruments, like hi-hats.)
      }
    
      bool isNoise = flags & InstrumentFlags_Noise;                 // If true, the ISR will periodically overwrite 'v_xor[]' with a random
                                                                    // value, mixing the wavetable sample with noise.
    
      const uint8_t noteOffset = offsetTable[note >> 4];            // Optionally change the amplitude envelope from +[0 .. 3] based on the
      uint8_t ampOffset = flags & InstrumentFlags_SelectAmplitude   // note played.  This helps with the realism of instruments with a wide
        ? noteOffset                                                // range, like the piano, where higher notes have a shorter sustain.
        : 0;
    
      uint8_t waveOffset = flags & InstrumentFlags_SelectWave       // Similarly, we can optionally shift the wavetable offset forward by
        ? noteOffset << 6                                           // multiples of 64 in the range +[0 .. 192].
        : 0;
    
      _note[voice] = note;                                          // Remember the note being played (for calculating pitch bench later).

      uint16_t samplingInterval = pgm_read_word(&_noteToSamplingInterval[note]);

      // Suspend audio processing before updating state shared with the ISR.
      suspend();

      v_wave[voice] = v_baseWave[voice] = instrument.wave + waveOffset;
      v_interval[voice] = v_bentInterval[voice] = _baseInterval[voice] = samplingInterval;
      v_xor[voice] = instrument.xorBits;
      v_amp[voice] = 0;
      v_isNoise[voice] = isNoise;
      v_vol[voice] = volume;
      v_ampMod[voice].start(instrument.ampMod + ampOffset);
      v_freqMod[voice].start(instrument.freqMod);
      v_waveMod[voice].start(instrument.waveMod);
    
      resume();
    }

    void noteOff(uint8_t voice) {
      suspend();                    // Suspend audio processing before updating state shared with the ISR.
      v_ampMod[voice].stop();       // Move amplitude envelope to 'release' stage, if not there already.
      resume();                     // Resume audio processing.
    }

    // Update the current volume for the given voice.  Note that MidiSynth preprocesses both
    // note velocity and channel volume into the single volume scalar.
    void setVolume(uint8_t voice, uint8_t volume) {
      // Suspend audio processing before updating state shared with the ISR.
      suspend();
      v_vol[voice] = volume;
      resume();
    }
  
    void pitchBend(uint8_t voice, int16_t value) {
      uint16_t pitch = _baseInterval[voice];
      
      // DANGER: Bending the lowest and highest two notes will read outside the bounds
      //         of the _noteToPitch[] table.  (Probably harmless. :)
      uint16_t delta = value >= 0
        ? pgm_read_word(&_noteToSamplingInterval[_note[voice] + 2]) - pitch
        : pitch - pgm_read_word(&_noteToSamplingInterval[_note[voice] - 2]);

      int32_t product;

    #ifndef __EMSCRIPTEN__
      // It can be challenging for the synth to process a pitch bend in real-time.
      // Pitch bench adjustments often occur at high frequency, and the calculation
      // involves an expensive S16 x U16 multiplication.
      // 
      // The multibyte multiplication code generated by AVR8/GNU C Compiler 5.4.0 (-Os)
      // is suboptimal for S16 x U16.  To help, we resort to inline assembly.
      // 
      // (See https://mekonik.wordpress.com/2009/03/18/arduino-avr-gcc-multiplication/)
      asm volatile (
        "clr r26 \n\t"
        "mul %A1, %A2 \n\t"
        "movw %A0, r0 \n\t"
        "mulsu %B1, %B2 \n\t"
        "movw %C0, r0 \n\t"
        "mul %B2, %A1 \n\t"
        "add %B0, r0 \n\t"
        "adc %C0, r1 \n\t"
        "adc %D0, r26 \n\t"
        "mulsu %B1, %A2 \n\t"
        "sbc %D0, r26 \n\t"
        "add %B0, r0 \n\t"
        "adc %C0, r1 \n\t"
        "adc %D0, r26 \n\t"
        "clr r1 \n\t"
        : "=&r" (product)
        : "a" (value), "a" (delta)
        : "r26");
    #else
      product = value * delta;
    #endif

      pitch += static_cast<int16_t>(product / 0x2000);

      // Suspend audio processing before updating state shared with the ISR.
      suspend();
      v_bentInterval[voice] = pitch;
      resume();
    }
  
    uint8_t getAmp(uint8_t voice) const {
      return v_amp[voice];
    }
  
    static uint16_t isr() __attribute__((always_inline)) {
      TIMSK2 = 0;         // Disable timer2 interrupts to prevent reentrancy.
      sei();              // Re-enable other interrupts to ensure USART RX ISR buffers incoming MIDI messages.
    
      {
        static uint16_t noise = 0xACE1;                   // 16-bit maximal-period Galois LFSR
        noise = (noise >> 1) ^ (-(noise & 1) & 0xB400);   // https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs
      
        static uint8_t divider = 0;                       // Time division is used to spread lower-frequency / periodic work
        divider++;                                        // across interrupts.
      
        const uint8_t voice = divider & 0x0F;             // Bottom 4 bits of 'divider' selects which voice to perform work on.
      
        if (v_isNoise[voice]) {                           // To avoid needing a large wavetable for noise, we use xor to combine
          v_xor[voice] = static_cast<uint8_t>(noise);     // the a 256B wavetable with samples from the LFSR.
        }

        const uint8_t fn = divider & 0xF0;                // Top 4 bits of 'divider' selects which additional work to perform.
        switch (fn) {
          case 0x00: {                                    // Advance frequency modulation and update 'v_pitch' for the current voice.
            int8_t freqMod = (v_freqMod[voice].sample() - 0x40);
            v_interval[voice] = v_bentInterval[voice] + freqMod;
            break;
          }
        
          case 0x50: {                                    // Advance wave modulation and update 'v_wave' for the current voice.
            int8_t waveMod = (v_waveMod[voice].sample());
            v_wave[voice] = v_baseWave[voice] + waveMod;
            break;
          }

          case 0xA0: {                                    // Advance the amplitude modulation and update 'v_amp' for the current voice.
            uint16_t amp = v_ampMod[voice].sample();
            v_amp[voice] = (amp * v_vol[voice]) >> 8;
            break;
          }
        }
      }

      // Macro that advances 'v_phase[voice]' by the sampling interval 'v_interval[voice]' and
      // stores the next 8-bit sample offset as 'offset##voice'.
      #define PHASE(voice) uint8_t offset##voice = ((v_phase[voice] += v_interval[voice]) >> 8)

      // Macro that samples the wavetable at the offset 'v_wave[voice] + offset##voice', and stores as 'sample##voice'.
      #define SAMPLE(voice) int8_t sample##voice = (pgm_read_byte(v_wave[voice] + offset##voice))

      // Macro that applies 'v_xor[voice]' to 'sample##voice' and multiplies by 'v_amp[voice]'.
      #define MIX(voice) ((sample##voice ^ v_xor[voice]) * v_amp[voice])
    
      // The below sampling/mixing code is carefully arranged to allow the compiler to make use of fixed
      // offsets for loads and stores and to leave intermediate calculations in register.
    
      PHASE(0); PHASE(1); PHASE(2); PHASE(3);                             // Advance the Q8.8 phase and calculate the 8-bit offsets into the wavetable.
      PHASE(4); PHASE(5); PHASE(6); PHASE(7);                             // (Load stores should use constant offsets and results should stay in register.)
    
      SAMPLE(0); SAMPLE(1); SAMPLE(2); SAMPLE(3);                         // Sample the wavetable at the offsets calculated above.
      SAMPLE(4); SAMPLE(5); SAMPLE(6); SAMPLE(7);                         // (Samples should stay in register.)
    
      int16_t mix0to7;                                                    // For voices 0..7
      mix0to7  = (MIX(0) + MIX(1) + MIX(2) + MIX(3));                     //   apply xor, modulate by amp, and mix.
      mix0to7 += (MIX(4) + MIX(5) + MIX(6) + MIX(7));
      Dac::set0to7(mix0to7);                                              //   output to PWM on Timer 0 

      PHASE(8); PHASE(9); PHASE(10); PHASE(11);                           // Advance the Q8.8 phase and calculate the 8-bit offsets into the wavetable.
      PHASE(12); PHASE(13); PHASE(14); PHASE(15);                         // (Load stores should use constant offsets and results should stay in register.)
    
      SAMPLE(8); SAMPLE(9); SAMPLE(10); SAMPLE(11);                       // Sample the wavetable at the offsets calculated above.
      SAMPLE(12); SAMPLE(13); SAMPLE(14); SAMPLE(15);                     // (Samples should stay in register.)
    
      int16_t mix8toF;                                                    // For vocies 8..F
      mix8toF  = (MIX(8) + MIX(9) + MIX(10) + MIX(11));                   //   apply xor, modulate by amp, and mix.
      mix8toF += (MIX(12) + MIX(13) + MIX(14) + MIX(15));
      Dac::set8toF(mix8toF);                                              //   output to PWM on Timer 1

      #undef MIX
      #undef SAMPLE
      #undef PHASE
    
      TIMSK2 = _BV(OCIE2A);                                               // Restore timer2 interrupts.
    
      return (mix0to7 >> 1) + (mix8toF >> 1) + 0x8000;                    // For Emscripten, return as a single signed value (GCC/AVR will elide this code).
    }

  
    // Suspends audio processing ISR.  While suspended, it is safe to update of volatile state
    // shared with the ISR.
    void suspend() __attribute__((always_inline)) {
      cli();
      TIMSK2 = 0;
      sei();
    }
  
    // Resumes audio processing ISR.
    void resume() __attribute__((always_inline)) {
      TIMSK2 = _BV(OCIE2A);
    }
  
  #ifdef __EMSCRIPTEN__
    Instrument instrument0;

    void noteOnEm(uint8_t voice, uint8_t note, uint8_t velocity, uint8_t instrumentIndex) {
      Instruments::getInstrument(instrumentIndex, instrument0);
      this->noteOn(voice, note, velocity, instrument0);
    }
  #endif // __EMSCRIPTEN__
};

constexpr uint16_t Synth::_noteToSamplingInterval[] PROGMEM;
constexpr uint8_t Synth::offsetTable[];

volatile const int8_t*  Synth::v_wave[Synth::numVoices]         = { 0 };
volatile uint16_t       Synth::v_phase[Synth::numVoices]        = { 0 };
volatile uint16_t       Synth::v_interval[Synth::numVoices]     = { 0 };
volatile int8_t         Synth::v_xor[Synth::numVoices]          = { 0 };
volatile uint8_t        Synth::v_amp[Synth::numVoices]          = { 0 };
volatile bool           Synth::v_isNoise[Synth::numVoices]      = { 0 };

volatile Envelope       Synth::v_ampMod[Synth::numVoices]       = {};
volatile Envelope       Synth::v_freqMod[Synth::numVoices]      = {};
volatile Envelope       Synth::v_waveMod[Synth::numVoices]      = {};

volatile uint8_t        Synth::v_vol[Synth::numVoices]          = { 0 };

         uint16_t       Synth::_baseInterval[Synth::numVoices]  = { 0 };
volatile uint16_t       Synth::v_bentInterval[Synth::numVoices] = { 0 };
volatile const int8_t*  Synth::v_baseWave[Synth::numVoices]     = { 0 };
         uint8_t        Synth::_note[Synth::numVoices]          = { 0 };

SIGNAL(TIMER2_COMPA_vect) {
  Synth::isr();
}

#endif // __SYNTH_H__
