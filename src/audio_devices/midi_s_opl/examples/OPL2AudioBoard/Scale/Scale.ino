/**
 * This is a simple demo sketch for the OPL2 library. It plays a repeating musical scale on channels 0 using a piano
 * instrument.
 *
 * OPL2 board is connected as follows:
 *   Pin  8 - Reset
 *   Pin  9 - A0
 *   Pin 10 - Latch
 *   Pin 11 - Data
 *   Pin 13 - Shift
 *
 * Refer to the wiki at https://github.com/DhrBaksteen/ArduinoOPL2/wiki/Connecting to learn how to connect your platform
 * of choice!
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2020-04-10
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL2.h>
#include <midi_instruments.h>


OPL2 opl2;

void setup() {
    opl2.begin();

    Instrument piano = opl2.loadInstrument(INSTRUMENT_PIANO1);      // Load a piano instrument.
    opl2.setInstrument(0, piano);                                   // Assign the instrument to OPL2 channel 0.
}


void loop() {
    for (int note = NOTE_C; note <= NOTE_B; note ++) {
        opl2.playNote(0, 4, note);                                  // Play the note on octave 4, channel 0.
        delay(400);                                                 // Wait a bit (note is held down: sustain).
        opl2.setKeyOn(0, false);                                    // Release the key (stop playing the note).
        delay(200);                                                 // Wait a bit (note will die down: release).
    }

    delay(500);
}
