/**
 * This is a demonstration sketch for the OPL3 Duo! it shows a more elegant panning technique.
 *
 * Though the OPL3 can produce stereo sound, it's panning capabilities are only limited. You can eather play the sound
 * of a channel on the left speaker only, the right speaker only or on both. Gradual panning between the left and right
 * speaker is not possible. This demonstration shows how gradual panning can still be achieved using two channels.
 * Channel 0 will output to the left speaker and channel 1 will output to the right speaker. By inverting the volume of
 * channel 0 on channel 1 we can achieve 63 steps of panning as this demo will show with a ping sound that drifts
 * between the left and the right speaker.
 *
 * Code by Maarten Janssen, 2020-08-23
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */


#include <OPL3Duo.h>


OPL3Duo opl3Duo;
int t = 0;


void setup() {
	// Initialize the OPL3Duo! and enable OPL3 features (requered to enable panning support).
	opl3Duo.begin();
	opl3Duo.setOPL3Enabled(true);

	// Define a simple continuous sound for channels 0 and 1.
	for (byte i = 0; i < 2; i++) {
		opl3Duo.setAttack(i, CARRIER, 8);
		opl3Duo.setSustain(i, CARRIER, 4);
		opl3Duo.setMaintainSustain(i, CARRIER, true);
		opl3Duo.setMultiplier(i, CARRIER, 1);
		opl3Duo.setVolume(i, CARRIER, 0);
	}

	// Set panning for channels 0 (left) and 1 (right).
	opl3Duo.setPanning(0, true, false);
	opl3Duo.setPanning(1, false, true);

	// Start playing the tone on both channels.
	opl3Duo.playNote(0, 5, NOTE_A);
	opl3Duo.playNote(1, 5, NOTE_A);
}


void loop() {
	// Define channel volume as a sine wave so it will gradually increase and decrease.
	byte volume = (sin((float)t / 50) * 31) + 31;

	// Apply volume to both channels.
	opl3Duo.setChannelVolume(0, volume);
	opl3Duo.setChannelVolume(1, 63 - volume);

	delay(10);
	t ++;
}
