/**
 * This is a demonstration sketch for the OPL3 Duo! It demonstrates how the TuneParser can be used with the OPL3 Duo! to
 * play music that is defined by simple strings of notes and other commands. The TuneParser can play up to 6 voices at
 * the same time.
 *
 * Code by Maarten Janssen, 2020-10-04
 * WWW.CHEERFUL.NL
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 */

#include <OPL3Duo.h>
#include <midi_instruments_4op.h>
#include <TuneParser.h>

const char voice1[] PROGMEM = "i96t150o5l8egredgrdcerc<b>er<ba>a<a>agdefefedr4.regredgrdcerc<b>er<ba>a<a>agdedcr4.c<g>cea>cr<ag>cr<gfarfearedgrdcfrc<bagab>cdfegredgrdcerc<b>er<ba>a<a>agdedcr4.cro3c2r2";
const char voice2[] PROGMEM = "i96o4l8crer<br>dr<ar>cr<grbrfr>cr<grbr>crer<gb>dgcrer<br>dr<ar>cr<grbrfr>cr<grbr>ceger4.rfrafergedrfdcrec<br>d<bar>c<agrgd<gr4.o4crer<br>dr<ar>cr<grbrfr>cr<grbr>cege";
const char voice3[] PROGMEM = "i2o3l8r4gr4.gr4.er4.err4fr4.gr4.gr4.grr4gr4.er4.er4.frr4gr4>ccr4ccr4<aarraar4ggr4ffr4.ro4gab>dr4.r<gr4.gr4.err4er4.fr4.g";

OPL3Duo opl3;
TuneParser tuneParser(&opl3);
Tune tune;


void setup() {
	// Initialize the TuneParser and the tune that we want to play in the background.
	// OPL3Duo is initialized by the TuneParser.
	tuneParser.begin();
	tuneParser.play(voice1, voice2, voice3);
}


void loop() {
}