/**
 * This is a simple demo sketch for the OPL2 library that demonstrates altering the frequency of an active channel. This
 * demo sweeps the frequency between 250 and 750 Hz on channel 0.
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
 * Code by Maarten Janssen (maarten@cheerful.nl) 2018-04-30
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <SPI.h>
#include <OPL2.h>


OPL2 opl2;
float t = 0.0;


void setup() {
	opl2.begin();

	// Setup channel 0 carrier.
	opl2.setMaintainSustain(0, CARRIER, true);
	opl2.setMultiplier(0, CARRIER, 0x01);
	opl2.setAttack    (0, CARRIER, 0x0A);
	opl2.setSustain   (0, CARRIER, 0x04);
	opl2.setVolume    (0, CARRIER, 0x00);

	// Start tone.
	opl2.setKeyOn(0, true);
}


void loop() {
	float freq = sin(t) * 250 + 500;
	opl2.setFrequency(0, freq);

	t += .01;
	delay(10);
}
