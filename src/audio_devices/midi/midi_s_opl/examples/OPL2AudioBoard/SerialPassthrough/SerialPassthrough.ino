/**
 * This sketch provides a simple serial passthrough to the OPL2 Audio Board from an Arduino or compatible board. Use
 * this together with the OPL3BankEditor or DosBox to use the OPL2 Audio Board as a real OPL2 output device.
 *
 * The OPL3BankEditor can be found here: https://github.com/Wohlstand/OPL3BankEditor
 * To use the OPL2 Audio Board go to Settings > Hardware OPL and set the serial port of the Arduino. Next set Settings >
 * Choose chip emulator to 'Serial port OPL interface'.
 *
 * For DosBox you need a special build found here: https://github.com/DhrBaksteen/ArduinoOPL2.DosBox
 * To use the OPL2 Audio Board edit the 'dosbox-OPL2 Audio Board.conf' file and set 'oplemu' to 'opl2board' and set
 * 'oplport' to the serial port of your arduino, for example 'COM4'.
 *
 * OPL2 board is connected as follows:
 * Pin  8 - Reset
 * Pin  9 - A0
 * Pin 10 - Latch
 * Pin 11 - Data
 * Pin 13 - Shift
 *
 * Refer to the wiki at https://github.com/DhrBaksteen/ArduinoOPL2/wiki/Connecting to learn how to connect your platform
 * of choice!
 *
 * The 'protocol' implemented in this sketch is very simple. It expects a [register, data] byte pair that is written to
 * the OPL2. There is no synchronization!
 *
 * Code by Maarten Janssen (maarten@cheerful.nl) 2018-07-21
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <SPI.h>
#include <OPL2.h>

OPL2 opl2;

void setup() {
  Serial.begin(115200);
  opl2.begin();
}

void loop() {
  while (Serial.available() > 1) {
    byte reg = Serial.read();
    byte val = Serial.read();
    opl2.write(reg, val);
  }
}
