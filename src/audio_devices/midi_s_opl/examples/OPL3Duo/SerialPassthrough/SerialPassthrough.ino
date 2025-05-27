/**
 * This demonstration sketch for the OPL3 Duo shows how you can use an Arduino to receive serial data to control the
 * OPL3 Duo board. The 'protocol' implemented in this sketch is very simple. It expects 3 bytes [BANK, REGISTER, DATA]
 * for every register write to one of the chips.
 *
 * The BANK is defined as follows:
 *    bits 7 .. 2 - Unused
 *    bit  1      - Chip 0 or 1
 *    bit  0      - Bank 0 or 1
 * The REGISTER sets the register of the selected chip and bank to write to.
 * The DATA defines the data to write to the selected register.
 *
 * You can change and improve the serial transfer to suit your application.
 *
 * Code by Maarten Janssen, 2020-11-07
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL3Duo.h>

#define NUKEYKTPROTO 0

OPL3Duo opl3Duo;

void setup() {
	Serial.begin(115200);
	opl3Duo.begin();
}

void loop() {
#ifndef NUKEYKTPROTO
	while (Serial.available() > 2) {
		byte bank = Serial.read() & 0x03;
		byte reg = Serial.read();
		byte val = Serial.read();

		opl3Duo.write(bank, reg, val);
	}
#else
	// If NUKEYKTPROTO is defined then Nuke.YKT's protocol is used. 
	// It provides a basic packet validation to avoid syncronisation issues. 
	// Also it's implemented and ready to use with Wohlstand's OPL3BankEditor.
	
	if(Serial.available()) {
		int x1 = Serial.read();
		if(x1&0x80) { // Check if first byte of the transferred packet is valid
			while(!Serial.available());
				byte x2= Serial.read();
			while(!Serial.available());
				byte x3 = Serial.read();
			byte bank = (x1>>2);
			byte reg = ((x1<<6)&0x0c0)|((x2>>1)&0x3f);
			byte data = ((x2<<7)&0x80)|(x3&0x7f);
			opl3Duo.write(bank, reg, data);
		}
	}
#endif

}
