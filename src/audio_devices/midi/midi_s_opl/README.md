# Arduino OPL2 Library
This repository contains the OPL2 / OPL3 audio library for the OPL2 Audio Board or OPL3 Duo! board. It is compatible with Arduino, Teensy, Raspberry Pi and other Arduino-like boards. A number of examples are included on how the library can be used. You can use the OPL2 Audio Board or OPL3 Duo! to:
* Experiment with the YM3812 or YMF262 chip
* Build your own synthesizer
* Play your own OPL2 tunes
* Instrument definitions from Adlib and The Fat Man are included in the library for both 2-op OPL3 and 4-op OPL3.
* Play exported OPL2 music (DRO, IMF, VGM) or Reality Adlib Tracker music files
* Use the board as a MIDI synthesizer (Teensy++ 2.0 and later example included)
* Emulation with DosBox; you can use the board to output MIDI music (Teensy++ 2.0 and later)
* Use the board directly as a synthesizer by using the [OPL3BankEditor](https://github.com/Wohlstand/OPL3BankEditor) software by Wohlstand

Current library version is 2.1.3, 23rd July 2024

### OPL2 Audio Board
The OPL2 Audio Board is fun and easy board to get started with an OPL2 synthesizer. It is built around the YM3812 OPL2 chip that gives you 9 channels with 2-operators each to produce the classic OPL2 sound that you may remember from early 90s computer games.
For more info visit the [Hackaday project page](https://hackaday.io/project/18995-opl2-audio-board-for-arduino-raspberry-pi).

To obtain your own OPL2 Audio Board visit my [Tindie store](https://www.tindie.com/products/DhrBaksteen/opl2-audio-board/).
![](https://raw.githubusercontent.com/DhrBaksteen/ArduinoOPL2/master/extra/OPL2_board.jpg)


### OPL3 Duo! Board
The OPL3 Duo! is a more capable version of the OPL2 Audio Board. It has 2 YMF262 OPL3 chips that allow for up to 36 channels of 2-operator sounds or up to 12 4-operator + 12 2-operator sounds. It also has stereo sound and is fully OPL2 compatible. Each OPL3 chip can be controlled individually or the library can control the board as if it were one big OPL3 chip with 36 channels.

The OPL3 Duo! board is available on my [Tindie store](https://www.tindie.com/products/cheerful/opl3-duo/).
![](https://raw.githubusercontent.com/DhrBaksteen/ArduinoOPL2/master/extra/OPL3Duo_board.jpg)


## 1. Installing the library
#### Arduino / Teensy
The easiest way to install the library it do download it through the Arduino Library Manager. Open the Library Manager from your Arduino IDE's Sketch > Include Library > Library Manager menu. A new window will open that allows you to search for a library. Search for 'Arduino OPL2' and it should show this libarary. Select the library, click the install button and you're good to go.

Alternatively you can clone this repo in the `libraries` folder of the Arduino IDE. or you can download the zip file and extract it in the libraries folder.

For more information see the [instructions](https://www.arduino.cc/en/Guide/Libraries) on the Arduino website.

#### Raspberry Pi / Orange Pi and compatibles
To install the library onto your Pi clone this repo and run `./build`. The OPL2 library requires [WiringPi](http://wiringpi.com/) to be installed on your Pi. Normally this library is already installed, but if this is not the case then the build script can install it for you.
