/*
    Arduino Midi Synth v0.2.2
    https://github.com/DLehenbauer/arduino-midi-sound-module
    
    This sketch turns an Arduino Uno into a 16-voice wavetable synthesizer functioning as a
    MIDI sound module.  Typical usages include generating sound for a MIDI keyboard controller
    or playback of MIDI files.
    
    The MIDI synth implements the most commonly used features of the General MIDI 1.0 standard,
    including:
    
      - 128 standard instruments
      - 45 percussion instruments
      - Key velocity
      - Pitch bend
     
    (See open GitHub issues for caveats: https://github.com/dlehenbauer/arduino-midi-sound-module/issues)
      
    The synth engine features:
      - 16 voices sampled & mixed in real-time at ~20kHz
      - Wavetable and white noise sources
      - Amplitude, frequency, and wavetable offset modulated by envelope generators
      - Additional volume control per voice (matching MIDI velocity)
    
    The circuit:
    
                    1M (1%)                  10uf*
        pin 5 >----^v^v^------o--------o------|(----> audio out
                              |        |
                   3.9k (1%)  |       === 3.3nf**
        pin 6 >----^v^v^------o        |
                              |       gnd
                              |
                              |
                    1M (1%)   |
        pin 9 >----^v^v^------o
                              |
                   3.9k (1%)  |
       pin 10 >----^v^v^------'

                                                                                                         
      * Note: A/C coupling capacitor typically optional.  (Negative is on audio out side.)
     ** Note: RC filtering capacitor is optional and can be adjusted to taste:
     
                          8kHz      10kHz      30kHz
               2.2nf ~=  -0.7db    -1.1db     -5.6db
               3.3nf ~=  -1.5db    -2.2db     -8.4db
               4.7nf ~=  -2.7db    -3.6db    -11.1db

    Sending MIDI data to the Arduino:
    
    The easiest/fastest way to send MIDI data from your computer is to use a MIDI <-> Serial Bridge:
    http://projectgus.github.io/hairless-midiserial/

    NOTE: To avoid overrunning the small buffer for incoming MIDI data, set the lower the baud rate
          of hairless to 38400.

    (See midi.h for other MIDI input options)
    
    For more details on the hardware & schematics see:
      dac.h          - Audio output circuit
      midi.h         - MIDI input options, serial driver, and midi dispatch
      ssd1306.h      - OLED display connections and driver

    For more information about sound generation, see:
      synth.h        - Core synth engine
      envelope.h     - Flexible envelope generator used to modulate amp, freq, and wave
      midisynth.h    - Extends synth engine w/additional state for MIDI processing
      instruments*.h - Wavetable and envelope generator programs for GM MIDI instruments
*/

// Defining 'USE_HAIRLESS_MIDI' will set the serial baud rate to 38400.  Comment out the
// below #define to use standard MIDI speed of 31250 baud.
#define USE_HAIRLESS_MIDI

// See "main.h" for the definitions of `setup()` and `loop()`
#include "main.h"
