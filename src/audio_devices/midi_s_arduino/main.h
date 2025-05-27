/*
    Main 'setup()' & 'loop()' definitions, shared with main.cpp and *.ino
    https://github.com/DLehenbauer/arduino-midi-sound-module

    For schematics, see:
      midi.h         - MIDI input options, serial driver, and midi dispatch

    For more sound generation, see:
      synth.h        - Core synth engine
      envelope.h     - Flexible envelope generator used to modulate amp, freq, and wave
      midisynth.h    - Extends synth engine w/additional state for MIDI processing
      instruments*.h - Wavetable and envelope generator programs for GM MIDI instruments
*/

#ifndef MAIN_H_
#define MAIN_H_

// NOTE: The *.ino file '#defines USE_HAIRLESS_MIDI', so if you built this from an Arduino
//       IDE sketch, you're configured to use Hairless MIDI at 38400 baud by default.


#include <stdint.h>
#include "midi.h"
#include "midisynth.h"

MidiSynth synth;

// The below thunks are invoked during Midi::Dispatch() and forwarded to our MidiSynth.
void noteOn(uint8_t channel, uint8_t note, uint8_t velocity)		    { synth.midiNoteOn(channel, note, velocity); }
void noteOff(uint8_t channel, uint8_t note)							            { synth.midiNoteOff(channel, note); }
void sysex(uint8_t cbData, uint8_t data[])							            { /* do nothing */ }
void controlChange(uint8_t channel, uint8_t control, uint8_t value) { synth.midiControlChange(channel, control, value); }
void programChange(uint8_t channel, uint8_t value)					        { synth.midiProgramChange(channel, value); }
void pitchBend(uint8_t channel, int16_t value)						          { synth.midiPitchBend(channel, value); }

// Invoked once after the device is reset, prior to starting the main 'loop()' below.
void setup() {
  
  synth.begin();                          // Start synth sample/mixing on Timer2 ISR

  Midi::begin(MIDI_BAUD);                 // Start receiving MIDI messages via USART.
  
}

// Helper used by main 'loop()' to set each column of an 8x7 block of pixels to the given mask,
// and then dispatch any MIDI messages that were queued while the SPI transfer was in progress.
//
// Note: Because this is the only call to 'display.send7()', AVR8/GNU C Compiler v5.4.0 will
//       inline it (even with -Os), and then jump into Midi::dispatch().

// There are four activities happening concurrently, roughly in priority order:
//
//    1. The USART RX ISR started by Midi::begin() is queuing incoming bytes from the serial port
//       in a circular buffer for later dispatch.
//
//    2. The Timer2 ISR started by synth.begin() is sampling/mixing and updating the output
//       waveform (by default, at ~20khz).
//
//    3. The main 'loop()' below interleaves the following two activities:
//        a. Handling the MIDI messages queued by the USART RX ISR by updating the state of the synth.
//        b. Updating the bar graph on the OLED display with the current amplitude of each voice.
//


// Note: Defining `main()` prevents initialization of the Arduino core runtime.
int main() {
  setup();
  
  while(true) {
  Midi::dispatch();                           // Each time through the loop, we drain the circular buffor of pending MIDI
                                              // messages.
  }
  
  return 0;
}

#endif /* MAIN_H_ */
