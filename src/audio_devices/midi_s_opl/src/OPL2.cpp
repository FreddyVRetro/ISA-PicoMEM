/**
 * ________ __________.____    ________      _____            .___.__         .____    ._____.
 * \_____  \\______   \    |   \_____  \    /  _  \  __ __  __| _/|__| ____   |    |   |__\_ |__
 *  /   |   \|     ___/    |    /  ____/   /  /_\  \|  |  \/ __ | |  |/  _ \  |    |   |  || __ \
 * /    |    \    |   |    |___/       \  /    |    \  |  / /_/ | |  (  <_> ) |    |___|  || \_\ \
 * \_______  /____|   |_______ \_______ \ \____|__  /____/\____ | |__|\____/  |_______ \__||___  /
 *         \/                 \/       \/ _____   \/           \/                     \/       \/
 *                                      _/ ____\___________
 *                                      \   __\/  _ \_  __ \
 *                                       |  | (  <_> )  | \/
 *                                       |__|  \____/|__|
 *               _____            .___    .__                  ____    __________.__
 *              /  _  \_______  __| _/_ __|__| ____   ____    /  _ \   \______   \__|
 *             /  /_\  \_  __ \/ __ |  |  \  |/    \ /  _ \   >  _ </\  |     ___/  |
 *            /    |    \  | \/ /_/ |  |  /  |   |  (  <_> ) /  <_\ \/  |    |   |  |
 *            \____|__  /__|  \____ |____/|__|___|  /\____/  \_____\ \  |____|   |__|
 *                    \/           \/             \/                \/
 *
 * YM3812 OPL2 Audio Library for Arduino, Raspberry Pi and Orange Pi v2.1.1
 * Code by Maarten Janssen (maarten@cheerful.nl) 2016-12-18
 * WWW.CHEERFUL.NL
 *
 * Look for example code on how to use this library in the examples folder.
 *
 * Connect the OPL2 Audio Board as follows. To learn how to connect your favorite development platform visit the wiki at
 * https://github.com/DhrBaksteen/ArduinoOPL2/wiki/Connecting.
 *    OPL2 Board | Arduino | Raspberry Pi
 *   ------------+---------+--------------
 *      Reset    |    8    |      18
 *      A0       |    9    |      16
 *      Latch    |   10    |      12
 *      Data     |   11    |      19
 *      Shift    |   13    |      23
 *
 *
 * IMPORTANT: Make sure you set the correct OPL_BOARD_TYPE in OPL2.h. Default is set to Arduino.
 *
 *
 * Last updated 2021-07-11
 * Most recent version of the library can be found at my GitHub: https://github.com/DhrBaksteen/ArduinoOPL2
 * Details about the YM3812 and YMF262 chips can be found at http://www.shikadi.net/moddingwiki/OPL_chip
 *
 * This library is open source and provided as is under the MIT software license, a copy of which is provided as part of
 * the project's repository. This library makes use of Gordon Henderson's Wiring Pi.
 * WWW.CHEERFUL.NL
 */

#include "OPL2.h"

#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
	#include <SPI.h>
	#include <Arduino.h>
#elif OPL_BOARD_TYPE == OPL2_BOARD_TYPE_PICOMEM
	#include "dev_adlib.h"
    #include "../../pm_debug.h"	
#else
	#include <wiringPi.h>
	#include <wiringPiSPI.h>
#endif


/**
 * Instantiate the OPL2 library with default pin setup.
 */
OPL2::OPL2() {
	#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_RASPBERRY_PI
		wiringPiSetup();
	#endif
}


/**
 * Instantiate the OPL2 library with custom pin setup. This constructor is left for legacy support. Preferably custom
 * pins are specifies when calling begin().
 *
 * @param reset - Pin number to use for RESET.
 * @param address - Pin number to use for A0.
 * @param latch - Pin number to use for LATCH.
 */
OPL2::OPL2(byte reset, byte address, byte latch) : OPL2::OPL2() {
	pinReset   = reset;
	pinAddress = address;
	pinLatch   = latch;
}


/**
 * Initialize the YM3812.
 */
void OPL2::begin() {
	#ifdef OPL_SERIAL_DEBUG
		Serial.begin(115200);
		while(!Serial);
		Serial.println("OPL serial debug enabled");
	#endif

	#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_PICOMEM
// Initialise PicoMEM OPL ?

	#else

	#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		SPI.begin();
		SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
	#else
		wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED);
	#endif

	pinMode(pinLatch,   OUTPUT);
	pinMode(pinAddress, OUTPUT);
	pinMode(pinReset,   OUTPUT);

	digitalWrite(pinLatch,   HIGH);
	digitalWrite(pinReset,   HIGH);
	digitalWrite(pinAddress, LOW);

	#endif

	createShadowRegisters();
	reset();
}


/**
 * Initialize the YM3812. This function is deprecated and should be replaced with OPL2.begin().
 */
void OPL2::init() {
	begin();
}


/**
 * Create shadow registers to hold the values written to the OPL2 chip for later access. Only those registers that are
 * are valid on the YM3812 are created to be as memory friendly as possible for platforms with limited RAM such as the
 * Arduino Uno / Nano. Registers consume 120 bytes.
 */
void OPL2::createShadowRegisters() {
	chipRegisters = new byte[3];					//  3
	channelRegisters = new byte[3 * numChannels];	// 27
	operatorRegisters = new byte[10 * numChannels];	// 90
}


/**
 * Hard reset the OPL2 chip and initialize all registers to 0x00. This should be called before sending any data to the
 * chip.
 */
void OPL2::reset() {
	// Hard reset the OPL2.
	digitalWrite(pinReset, LOW);
	delay(1);
	digitalWrite(pinReset, HIGH);

	// Initialize chip registers.
	setChipRegister(0x00, 0x00);
	setChipRegister(0x08, 0x40);
	setChipRegister(0xBD, 0x00);

	// Initialize all channel and operator registers.
	for (byte i = 0; i < getNumChannels(); i ++) {
		setChannelRegister(0xA0, i, 0x00);
		setChannelRegister(0xB0, i, 0x00);
		setChannelRegister(0xC0, i, 0x00);

		for (byte j = OPERATOR1; j <= OPERATOR2; j ++) {
			setOperatorRegister(0x20, i, j, 0x00);
			setOperatorRegister(0x40, i, j, 0x3F);
			setOperatorRegister(0x60, i, j, 0x00);
			setOperatorRegister(0x80, i, j, 0x00);
			setOperatorRegister(0xE0, i, j, 0x00);
		}
	}
}


/**
 * Get the current value of a chip wide register from the shadow registers.
 *
 * @param reg - The 9-bit address of the register.
 * @return The value of the register from shadow registers.
 */
byte OPL2::getChipRegister(short reg) {
	return chipRegisters[getChipRegisterOffset(reg)];
}


/**
 * Write a given value to a chip wide register.
 *
 * @param reg - The 9-bit register to write to.
 * @param value - The value to write to the register.
 */
void OPL2::setChipRegister(short reg, byte value) {
	chipRegisters[getChipRegisterOffset(reg)] = value;
	write(reg & 0xFF, value);
}


/**
 * Get the internal register offset for a chip wide register.
 *
 * @param reg - The 9-bit register for which we want to know the internal offset.
 * @return The offset to the internal shadow register or 0 if an illegal chip register was requested.
 */
byte OPL2::getChipRegisterOffset(short reg) {
	switch (reg & 0xFF) {
		case 0x08:
			return 1;
		case 0xBD:
			return 2;
		case 0x01:
		default:
			return 0;
	}
}


/**
 * Get the value of a channel register.
 *
 * @param baseAddress - The base address of the register.
 * @param channel - The channel for which to get the register value [0, 8].
 * @return The current value of the from the shadow register.
 */
byte OPL2::getChannelRegister(byte baseRegister, byte channel) {
	return channelRegisters[getChannelRegisterOffset(baseRegister, channel)];
}


/**
 * Write a given value to a channel based register.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel to address [0, 8].
 * @param value - The value to write to the register.
 */
void OPL2::setChannelRegister(byte baseRegister, byte channel, byte value) {
	channelRegisters[getChannelRegisterOffset(baseRegister, channel)] = value;
	byte reg = baseRegister + (channel % CHANNELS_PER_BANK);
	write(reg, value);
}


/**
 * Get the internal offset of a channel register.
 *
 * @param baseRegister - The base register where we want to know the offset of.
 * @param channel - The channel [0, numChannels] for which we want to know the offset.
 * @return The internal offset of the channel register or 0 if the baseRegister is invalid.
 */
byte OPL2::getChannelRegisterOffset(byte baseRegister, byte channel) {
	channel = channel % getNumChannels();
	byte offset = channel * 3;

	switch (baseRegister) {
		case 0xB0:
			return offset + 1;
		case 0xC0:
			return offset + 2;
		case 0xA0:
		default:
			return offset;
	}
}


/**
 * Get the current value of an operator register of a channel from the shadow registers.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel of the operatpr [0, 17].
 * @param op - The operator [0, 1].
 * @return The operator register value from shadow registers.
 */
byte OPL2::getOperatorRegister(byte baseRegister, byte channel, byte operatorNum) {
	return operatorRegisters[getOperatorRegisterOffset(baseRegister, channel, operatorNum)];
}


/**
 * Write a given value to an operator register for a channel.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel of the operator [0, 8]
 * @param operatorNum - The operator to change [0, 1].
 * @param value - The value to write to the operator's register.
 */
void OPL2::setOperatorRegister(byte baseRegister, byte channel, byte operatorNum, byte value) {
	operatorRegisters[getOperatorRegisterOffset(baseRegister, channel, operatorNum)] = value;
	byte reg = baseRegister + getRegisterOffset(channel, operatorNum);
	write(reg, value);
}


/**
 * Get the internal offset of an operator register.
 *
 * @param baseRegister - The base register where we want to know the offset of.
 * @param channel - The channel [0, numChannels] to get the offset to.
 * @param operatorNum - The operator [0, 1] to get the offset to.
 * @return The internal offset of the operator register or 0 if the baseRegister is invalid.
 */
short OPL2::getOperatorRegisterOffset(byte baseRegister, byte channel, byte operatorNum) {
	channel = channel % getNumChannels();
	operatorNum = operatorNum & 0x01;
	short offset = (channel * 10) + (operatorNum * 5);

	switch (baseRegister) {
		case 0x40:
			return offset + 1;
		case 0x60:
			return offset + 2;
		case 0x80:
			return offset + 3;
		case 0xE0:
			return offset + 4;
		case 0x20:
		default:
			return offset;
	}
}


/**
 * Get the offset from a base register to a channel operator register.
 *
 * @param channel - The channel for which to get the offset [0, 8].
 * @param operatorNum - The operator for which to get the offset [0, 1].
 * @return The offset from the base register to the operator register.
 */
byte OPL2::getRegisterOffset(byte channel, byte operatorNum) {
	return registerOffsets[operatorNum % 2][channel % CHANNELS_PER_BANK];
}


/**
 * Write the given value to an OPL2 register. This does not update the internal shadow register!
 *
 * @param reg - The register to change.
 * @param value - The value to write to the register.
 */
void OPL2::write(byte reg, byte value) {
	#ifdef OPL_SERIAL_DEBUG
		Serial.print("reg: ");
		Serial.print(reg, HEX);
		Serial.print(", val: ");
		Serial.println(value, HEX);
	#endif

	// Write OPL2 address.
	digitalWrite(pinAddress, LOW);
	#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		SPI.transfer(reg);
	#else
		wiringPiSPIDataRW(SPI_CHANNEL, &reg, 1);
	#endif
	digitalWrite(pinLatch, LOW);
	delayMicroseconds(16);
	digitalWrite(pinLatch, HIGH);
	delayMicroseconds(16);

	// Write OPL2 data.
	digitalWrite(pinAddress, HIGH);
	#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		SPI.transfer(value);
	#else
		wiringPiSPIDataRW(SPI_CHANNEL, &value, 1);
	#endif
	digitalWrite(pinLatch, LOW);
	delayMicroseconds(4);
	digitalWrite(pinLatch, HIGH);
	delayMicroseconds(92);
}


/**
 * Return the number of channels for this OPL2.
 */
byte OPL2::getNumChannels() {
	return numChannels;
}


/**
 * Get the F-number for the given frequency for a given channel. When the F-number is calculated the current frequency
 * block of the channel is taken into account.
 */
short OPL2::getFrequencyFNumber(byte channel, float frequency) {
	float fInterval = getFrequencyStep(channel);
	return clampValue((short)(frequency / fInterval), (short)0, (short)1023);
}


/**
 * Get the F-Number for the given note. In this case the block is assumed to be the octave.
 */
short OPL2::getNoteFNumber(byte note) {
	return noteFNumbers[note % NUM_NOTES];
}

/**
 * Get the frequency step per F-number for the current block on the given channel.
 */
float OPL2::getFrequencyStep(byte channel) {
	return fIntervals[getBlock(channel)];
}


/**
 * Get the optimal frequency block for the given frequency.
 */
byte OPL2::getFrequencyBlock(float frequency) {
	for (byte i = 0; i < 8; i ++) {
		if (frequency < blockFrequencies[i]) {
			return i;
		}
	}
	return 7;
}


/**
 * Create and return a new empty instrument.
 */
Instrument OPL2::createInstrument() {
	Instrument instrument;

	for (byte op = OPERATOR1; op <= OPERATOR2; op ++) {
		instrument.operators[op].hasTremolo = false;
		instrument.operators[op].hasVibrato = false;
		instrument.operators[op].hasSustain = false;
		instrument.operators[op].hasEnvelopeScaling = false;
		instrument.operators[op].frequencyMultiplier = 0;
		instrument.operators[op].keyScaleLevel = 0;
		instrument.operators[op].outputLevel = 0;
		instrument.operators[op].attack = 0;
		instrument.operators[op].decay = 0;
		instrument.operators[op].sustain = 0;
		instrument.operators[op].release = 0;
		instrument.operators[op].waveForm = 0;
	}

	instrument.transpose = 0;
	instrument.feedback = 0;
	instrument.isAdditiveSynth = false;

	return instrument;
}


/**
 * Create an instrument and load it with instrument parameters from the given instrument data pointer.
 */
#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
	Instrument OPL2::loadInstrument(const unsigned char *instrumentData, bool fromProgmem) {
#else
	Instrument OPL2::loadInstrument(const unsigned char *instrumentData) {
#endif
	Instrument instrument = createInstrument();

	byte data[11];
	for (byte i = 0; i < 11; i ++) {
		#if OPL_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
			if (fromProgmem) {
				data[i] = pgm_read_byte_near(instrumentData + i);
			} else {
				data[i] = instrumentData[i];
			}
		#else
			data[i] = instrumentData[i];
		#endif
	}

	for (byte op = OPERATOR1; op <= OPERATOR2; op ++) {
		instrument.operators[op].hasTremolo = data[op * 5 + 1] & 0x80 ? true : false;
		instrument.operators[op].hasVibrato = data[op * 5 + 1] & 0x40 ? true : false;
		instrument.operators[op].hasSustain = data[op * 5 + 1] & 0x20 ? true : false;
		instrument.operators[op].hasEnvelopeScaling = data[op * 5 + 1] & 0x10 ? true : false;
		instrument.operators[op].frequencyMultiplier = (data[op * 5 + 1] & 0x0F);
		instrument.operators[op].keyScaleLevel = (data[op * 5 + 2] & 0xC0) >> 6;
		instrument.operators[op].outputLevel = data[op * 5 + 2] & 0x3F;
		instrument.operators[op].attack = (data[op * 5 + 3] & 0xF0) >> 4;
		instrument.operators[op].decay = data[op * 5 + 3] & 0x0F;
		instrument.operators[op].sustain = (data[op * 5 + 4] & 0xF0) >> 4;
		instrument.operators[op].release = data[op * 5 + 4] & 0x0F;
	}
	instrument.operators[0].waveForm = data[10] & 0x07;
	instrument.operators[1].waveForm = (data[10] & 0x70) >> 4;

	instrument.transpose = data[0];
	instrument.feedback = (data[5] & 0x0E) >> 1;
	instrument.isAdditiveSynth = data[5] & 0x01 ? true : false;

	return instrument;
}


/**
 * Create a new instrument from the given OPL2 channel.
 */
Instrument OPL2::getInstrument(byte channel) {
	Instrument instrument;

	for (byte op = OPERATOR1; op <= OPERATOR2; op ++) {
		instrument.operators[op].hasTremolo = getTremolo(channel, op);
		instrument.operators[op].hasVibrato = getVibrato(channel, op);
		instrument.operators[op].hasSustain = getMaintainSustain(channel, op);
		instrument.operators[op].hasEnvelopeScaling = getEnvelopeScaling(channel, op);
		instrument.operators[op].frequencyMultiplier = getMultiplier(channel, op);
		instrument.operators[op].keyScaleLevel = getScalingLevel(channel, op);
		instrument.operators[op].outputLevel = getVolume(channel, op);
		instrument.operators[op].attack = getAttack(channel, op);
		instrument.operators[op].decay = getDecay(channel, op);
		instrument.operators[op].sustain = getSustain(channel, op);
		instrument.operators[op].release = getRelease(channel, op);
		instrument.operators[op].waveForm = getWaveForm(channel, op);
	}

	instrument.transpose = 0;
	instrument.feedback = getFeedback(channel);
	instrument.isAdditiveSynth = getSynthMode(channel) == SYNTH_MODE_AM;

	return instrument;
}


/**
 * Set the given instrument to a channel. An optional volume may be provided to assign to proper output levels for the
 * operators.
 */
void OPL2::setInstrument(byte channel, Instrument instrument, float volume) {
	volume = clampValue(volume, (float)0.0, (float)1.0);

	setWaveFormSelect(true);
	for (byte op = OPERATOR1; op <= OPERATOR2; op ++) {
		byte outputLevel = 63 - (byte)((63.0 - (float)instrument.operators[op].outputLevel) * volume);

		setOperatorRegister(0x20, channel, op,
			(instrument.operators[op].hasTremolo ? 0x80 : 0x00) +
			(instrument.operators[op].hasVibrato ? 0x40 : 0x00) +
			(instrument.operators[op].hasSustain ? 0x20 : 0x00) +
			(instrument.operators[op].hasEnvelopeScaling ? 0x10 : 0x00) +
			(instrument.operators[op].frequencyMultiplier & 0x0F));
		setOperatorRegister(0x40, channel, op,
			((instrument.operators[op].keyScaleLevel & 0x03) << 6) +
			(outputLevel & 0x3F));
		setOperatorRegister(0x60, channel, op,
			((instrument.operators[op].attack & 0x0F) << 4) +
			(instrument.operators[op].decay & 0x0F));
		setOperatorRegister(0x80, channel, op,
			((instrument.operators[op].sustain & 0x0F) << 4) +
			(instrument.operators[op].release & 0x0F));
		setOperatorRegister(0xE0, channel, op,
			(instrument.operators[op].waveForm & 0x07));
	}

	byte value = getChannelRegister(0xC0, channel) & 0xF0;
	setChannelRegister(0xC0, channel,
		value +
		((instrument.feedback & 0x07) << 1) +
		(instrument.isAdditiveSynth ? 0x01 : 0x00));
}


/**
 * Set the given instrument as a drum type for percussive mode. Depending on the drumType the instrument parameters will
 * be loaded into the appropriate channel and operator(s). An optional volume may be provided to set the
 * proper output levels for the operator(s).
 *
 * @param instrument - The instrument to be set.
 * @param drumType - The type of drum instrument to set the parameters of.
 * @param volume - Optional volume parameter for the drum.
 */
void OPL2::setDrumInstrument(Instrument instrument, byte drumType, float volume) {
	drumType = clampValue(drumType, (byte)DRUM_BASS, (byte)DRUM_HI_HAT);
	volume = clampValue(volume, (float)0.0, (float)1.0);
	byte channel = drumChannels[drumType];

	setWaveFormSelect(true);
	for (byte op = OPERATOR1; op <= OPERATOR2; op ++) {
		byte outputLevel = 63 - (byte)((63.0 - (float)instrument.operators[op].outputLevel) * volume);
		byte registerOffset = drumRegisterOffsets[op][drumType];

		if (registerOffset != 0xFF) {
			setOperatorRegister(0x20, channel, op,
				(instrument.operators[op].hasTremolo ? 0x80 : 0x00) +
				(instrument.operators[op].hasVibrato ? 0x40 : 0x00) +
				(instrument.operators[op].hasSustain ? 0x20 : 0x00) +
				(instrument.operators[op].hasEnvelopeScaling ? 0x10 : 0x00) +
				(instrument.operators[op].frequencyMultiplier & 0x0F));
			setOperatorRegister(0x40, channel, op,
				((instrument.operators[op].keyScaleLevel & 0x03) << 6) +
				(outputLevel & 0x3F));
			setOperatorRegister(0x60, channel, op,
				((instrument.operators[op].attack & 0x0F) << 4) +
				(instrument.operators[op].decay & 0x0F));
			setOperatorRegister(0x80, channel, op,
				((instrument.operators[op].sustain & 0x0F) << 4) +
				(instrument.operators[op].release & 0x0F));
			setOperatorRegister(0xE0, channel, op,
				(instrument.operators[op].waveForm & 0x07));
		}
	}

	byte value = getChannelRegister(0xC0, channel) & 0xF0;
	setChannelRegister(0xC0, channel,
		value +
		((instrument.feedback & 0x07) << 1) +
		(instrument.isAdditiveSynth ? 0x01 : 0x00));
}


/**
 * Play a note of a certain octave on the given channel.
 */
void OPL2::playNote(byte channel, byte octave, byte note) {
	if (getKeyOn(channel)) {
		setKeyOn(channel, false);
	}
	setBlock(channel, clampValue(octave, (byte)0, (byte)NUM_OCTAVES));
	setFNumber(channel, noteFNumbers[note % 12]);
	setKeyOn(channel, true);
}


/**
 * Play a drum sound at a given note and frequency.
 * The OPL2 must be put into percusive mode first and the parameters of the drum sound must be set in the required
 * operator(s). Note that changing octave and note frequenct will influence both drum sounds if they occupy only a
 * single operator (Snare + Hi-hat and Tom + Cymbal).
 */
void OPL2::playDrum(byte drum, byte octave, byte note) {
	drum = drum % NUM_DRUM_SOUNDS;
	byte drumState = getDrums();

	setDrums(drumState & ~drumBits[drum]);
	byte drumChannel = drumChannels[drum];
	setBlock(drumChannel, clampValue(octave, (byte)0, (byte)NUM_OCTAVES));
	setFNumber(drumChannel, noteFNumbers[note % NUM_NOTES]);
	setDrums(drumState | drumBits[drum]);
}


/**
 * Is wave form selection currently enabled.
 */
bool OPL2::getWaveFormSelect() {
	return getChipRegister(0x01) & 0x20;
}


/**
 * Enable wave form selection for each operator.
 */
void OPL2::setWaveFormSelect(bool enable) {
	if (enable) {
		setChipRegister(0x01, getChipRegister(0x01) | 0x20);
	} else {
		setChipRegister(0x01, getChipRegister(0x01) & 0xDF);
	}
}


/**
 * Is amplitude modulation enabled for the given operator?
 */
bool OPL2::getTremolo(byte channel, byte operatorNum) {
	return getOperatorRegister(0x20, channel, operatorNum) & 0x80;
}


/**
 * Apply amplitude modulation when set to true. Modulation depth is controlled globaly by the AM-depth flag in the 0xBD
 * register.
 */
void OPL2::setTremolo(byte channel, byte operatorNum, bool enable) {
	byte value =  getOperatorRegister(0x20, channel, operatorNum) & 0x7F;
	setOperatorRegister(0x20, channel, operatorNum, value + (enable ? 0x80 : 0x00));
}


/**
 * Is vibrator enabled for the given channel?
 */
bool OPL2::getVibrato(byte channel, byte operatorNum) {
	return getOperatorRegister(0x20, channel, operatorNum) & 0x40;
}


/**
 * Apply vibrato when set to true. Vibrato depth is controlled globally by the VIB-depth flag in the 0xBD register.
 */
void OPL2::setVibrato(byte channel, byte operatorNum, bool enable) {
	byte value = getOperatorRegister(0x20, channel, operatorNum) & 0xBF;
	setOperatorRegister(0x20, channel, operatorNum, value + (enable ? 0x40 : 0x00));
}


/**
 * Is sustain being maintained for the given channel?
 */
bool OPL2::getMaintainSustain(byte channel, byte operatorNum) {
	return getOperatorRegister(0x20, channel, operatorNum) & 0x20;
}


/**
 * When set to true the sustain level of the voice is maintained until released. When false the sound begins to decay
 * immediately after hitting the sustain phase.
 */
void OPL2::setMaintainSustain(byte channel, byte operatorNum, bool enable) {
	byte value = getOperatorRegister(0x20, channel, operatorNum) & 0xDF;
	setOperatorRegister(0x20, channel, operatorNum, value + (enable ? 0x20 : 0x00));
}


/**
 * Is envelope scaling being applied to the given channel?
 */
bool OPL2::getEnvelopeScaling(byte channel, byte operatorNum) {
	return getOperatorRegister(0x20, channel, operatorNum) & 0x10;
}


/**
 * Enable or disable envelope scaling. When set to true higher notes will be shorter than lower ones.
 */
void OPL2::setEnvelopeScaling(byte channel, byte operatorNum, bool enable) {
	byte value = getOperatorRegister(0x20, channel, operatorNum) & 0xEF;
	setOperatorRegister(0x20, channel, operatorNum, value + (enable ? 0x10 : 0x00));
}


/**
 * Get the frequency multiplier for the given channel.
 */
byte OPL2::getMultiplier(byte channel, byte operatorNum) {
	return getOperatorRegister(0x20, channel, operatorNum) & 0x0F;
}


/**
 * Set frequency multiplier for the given channel. Note that a multiplier of 0 will apply a 0.5 multiplication.
 */
void OPL2::setMultiplier(byte channel, byte operatorNum, byte multiplier) {
	byte value = getOperatorRegister(0x20, channel, operatorNum) & 0xF0;
	setOperatorRegister(0x20, channel, operatorNum, value + (multiplier & 0x0F));
}


/**
 * Get the scaling level for the given channel.
 */
byte OPL2::getScalingLevel(byte channel, byte operatorNum) {
	return (getOperatorRegister(0x40, channel, operatorNum) & 0xC0) >> 6;
}


/**
 * Decrease output levels as frequency increases.
 * 00 - No change
 * 01 - 1.5 dB/oct
 * 10 - 3.0 dB/oct
 * 11 - 6.0 dB/oct
 */
void OPL2::setScalingLevel(byte channel, byte operatorNum, byte scaling) {
	byte value = getOperatorRegister(0x40, channel, operatorNum) & 0x3F;
	setOperatorRegister(0x40, channel, operatorNum, value + ((scaling & 0x03) << 6));
}


/**
 * Get the volume of the given channel operator. 0x00 is laudest, 0x3F is softest.
 */
byte OPL2::getVolume(byte channel, byte operatorNum) {
	return getOperatorRegister(0x40, channel, operatorNum) & 0x3F;
}


/**
 * Set the volume of the channel operator.
 * Note that the scale is inverted! 0x00 for loudest, 0x3F for softest.
 */
void OPL2::setVolume(byte channel, byte operatorNum, byte volume) {
	byte value = getOperatorRegister(0x40, channel, operatorNum) & 0xC0;
	setOperatorRegister(0x40, channel, operatorNum, value + (volume & 0x3F));
}


/**
 * Get the volume of the given channel.
 */
byte OPL2::getChannelVolume(byte channel) {
	return getVolume(channel, OPERATOR2);
}


/**
 * Set the volume for the given channel. Depending on the current synthesis mode this will affect both operators (AM) or
 * only operator 2 (FM).
 */
void OPL2::setChannelVolume(byte channel, byte volume) {
	if (getSynthMode(channel)) {
		setVolume(channel, OPERATOR1, volume);
	}
	setVolume(channel, OPERATOR2, volume);
}


/**
 * Get the attack rate of the given channel.
 */
byte OPL2::getAttack(byte channel, byte operatorNum) {
	return (getOperatorRegister(0x60, channel, operatorNum) & 0xF0) >> 4;
}


/**
 * Attack rate. 0x00 is slowest, 0x0F is fastest.
 */
void OPL2::setAttack(byte channel, byte operatorNum, byte attack) {
	byte value = getOperatorRegister(0x60, channel, operatorNum) & 0x0F;
	setOperatorRegister(0x60, channel, operatorNum, value + ((attack & 0x0F) << 4));
}


/**
 * Get the decay rate of the given channel.
 */
byte OPL2::getDecay(byte channel, byte operatorNum) {
	return getOperatorRegister(0x60, channel, operatorNum) & 0x0F;
}


/**
 * Decay rate. 0x00 is slowest, 0x0F is fastest.
 */
void OPL2::setDecay(byte channel, byte operatorNum, byte decay) {
	byte value = getOperatorRegister(0x60, channel, operatorNum) & 0xF0;
	setOperatorRegister(0x60, channel, operatorNum, value + (decay & 0x0F));
}


/**
 * Get the sustain level of the given channel. 0x00 is laudest, 0x0F is softest.
 */
byte OPL2::getSustain(byte channel, byte operatorNum) {
	return (getOperatorRegister(0x80, channel, operatorNum) & 0xF0) >> 4;
}


/**
 * Sustain level. 0x00 is laudest, 0x0F is softest.
 */
void OPL2::setSustain(byte channel, byte operatorNum, byte sustain) {
	byte value = getOperatorRegister(0x80, channel, operatorNum) & 0x0F;
	setOperatorRegister(0x80, channel, operatorNum, value + ((sustain & 0x0F) << 4));
}


/**
 * Get the release rate of the given channel.
 */
byte OPL2::getRelease(byte channel, byte operatorNum) {
	return getOperatorRegister(0x80, channel, operatorNum) & 0x0F;
}


/**
 * Release rate. 0x00 is flowest, 0x0F is fastest.
 */
void OPL2::setRelease(byte channel, byte operatorNum, byte release) {
	byte value = getOperatorRegister(0x80, channel, operatorNum) & 0xF0;
	setOperatorRegister(0x80, channel, operatorNum, value + (release & 0x0F));
}


/**
 * Get the frequenct F-number of the given channel.
 */
short OPL2::getFNumber(byte channel) {
	short value = (getChannelRegister(0xB0, channel) & 0x03) << 8;
	value += getChannelRegister(0xA0, channel);
	return value;
}


/**
 * Set frequency F-number [0, 1023] for the given channel.
 */
void OPL2::setFNumber(byte channel, short fNumber) {
	byte value = getChannelRegister(0xB0, channel) & 0xFC;
	setChannelRegister(0xB0, channel, value + ((fNumber & 0x0300) >> 8));
	setChannelRegister(0xA0, channel, fNumber & 0xFF);
}


/**
 * Get the frequency for the given channel.
 */
float OPL2::getFrequency(byte channel) {
	float fInterval = getFrequencyStep(channel);
	return getFNumber(channel) * fInterval;
}


/**
 * Set the frequenct of the given channel and if needed switch to a different block.
 */
void OPL2::setFrequency(byte channel, float frequency) {
	unsigned char block = getFrequencyBlock(frequency);
	if (getBlock(channel) != block) {
		setBlock(channel, block);
	}
	short fNumber = getFrequencyFNumber(channel, frequency);
	setFNumber(channel, fNumber);
}


/**
 * Get the frequency block of the given channel.
 */
byte OPL2::getBlock(byte channel) {
	return (getChannelRegister(0xB0, channel) & 0x1C) >> 2;
}


/**
 * Set frequency block. 0x00 is lowest, 0x07 is highest. This determines the frequency interval between notes.
 * 0 - 0.048 Hz, Range: 0.047 Hz ->   48.503 Hz
 * 1 - 0.095 Hz, Range: 0.094 Hz ->   97.006 Hz
 * 2 - 0.190 Hz, Range: 0.189 Hz ->  194.013 Hz
 * 3 - 0.379 Hz, Range: 0.379 Hz ->  388.026 Hz
 * 4 - 0.759 Hz, Range: 0.758 Hz ->  776.053 Hz
 * 5 - 1.517 Hz, Range: 1.517 Hz -> 1552.107 Hz
 * 6 - 3.034 Hz, Range: 3.034 Hz -> 3104.215 Hz
 * 7 - 6.069 Hz, Range: 6.068 Hz -> 6208.431 Hz
 */
void OPL2::setBlock(byte channel, byte block) {
	byte value = getChannelRegister(0xB0, channel) & 0xE3;
	setChannelRegister(0xB0, channel, value + ((block & 0x07) << 2));
}


/**
 * Get the octave split bit.
 */
bool OPL2::getNoteSelect() {
	return getChipRegister(0x08) & 0x40;
}


/**
 * Set the octave split bit. This sets how the chip interprets F-numbers to determine where an octave is split. For note
 * F-numbers used by the OPL2 library this bit should be set.
 *
 * @param enable - Sets the note select bit when true, otherwise reset it.
 */
void OPL2::setNoteSelect(bool enable) {
	setChipRegister(0x08, enable ? 0x40 : 0x00);
}


/**
 * Is the voice of the given channel currently enabled?
 */
bool OPL2::getKeyOn(byte channel) {
	return getChannelRegister(0xB0, channel) & 0x20;
}


/**
 * Enable voice on channel.
 */
void OPL2::setKeyOn(byte channel, bool keyOn) {
	byte value = getChannelRegister(0xB0, channel) & 0xDF;
	setChannelRegister(0xB0, channel, value + (keyOn ? 0x20 : 0x00));
}


/**
 * Get the feedback strength of the given channel.
 */
byte OPL2::getFeedback(byte channel) {
	return (getChannelRegister(0xC0, channel) & 0x0E) >> 1;
}


/**
 * Set feedback strength. 0x00 is no feedback, 0x07 is strongest.
 */
void OPL2::setFeedback(byte channel, byte feedback) {
	byte value = getChannelRegister(0xC0, channel) & 0xF1;
	setChannelRegister(0xC0, channel, value + ((feedback & 0x07) << 1));
}


/**
 * Get the synth model that is used for the given channel.
 */
byte OPL2::getSynthMode(byte channel) {
	return getChannelRegister(0xC0, channel) & 0x01;
}


/**
 * Set the synthesizer mode of the given channel.
 */
void OPL2::setSynthMode(byte channel, byte synthMode) {
	byte value = getChannelRegister(0xC0, channel) & 0xFE;
	setChannelRegister(0xC0, channel, value + (synthMode & 0x01));
}


/**
 * Is deeper amplitude modulation enabled?
 */
bool OPL2::getDeepTremolo() {
	return getChipRegister(0xBD) & 0x80;
}


/**
 * Set deeper aplitude modulation depth. When false modulation depth is 1.0 dB, when true modulation depth is 4.8 dB.
 */
void OPL2::setDeepTremolo(bool enable) {
	byte value = getChipRegister(0xBD) & 0x7F;
	setChipRegister(0xBD, value + (enable ? 0x80 : 0x00));
}


/**
 * Is deeper vibrato depth enabled?
 */
bool OPL2::getDeepVibrato() {
	return getChipRegister(0xBD) & 0x40;
}


/**
 * Set deeper vibrato depth. When false vibrato depth is 7/100 semi tone, when true vibrato depth is 14/100.
 */
void OPL2::setDeepVibrato(bool enable) {
	byte value = getChipRegister(0xBD) & 0xBF;
	setChipRegister(0xBD, value + (enable ? 0x40 : 0x00));
}


/**
 * Is percussion mode currently enabled?
 */
bool OPL2::getPercussion() {
	return getChipRegister(0xBD) & 0x20;
}


/**
 * Enable or disable percussion mode. When set to false there are 9 melodic voices, when true there are 6 melodic
 * voices and channels 6 through 8 are used for drum sounds. KeyOn for these channels must be off.
 */
void OPL2::setPercussion(bool enable) {
	byte value = getChipRegister(0xBD) & 0xDF;
	setChipRegister(0xBD, value + (enable ? 0x20 : 0x00));
}


/**
 * Return which drum sounds are enabled.
 */
byte OPL2::getDrums() {
	return getChipRegister(0xBD) & 0x1F;
}


/**
 * Set the OPL2 drum registers all at once.
 */
void OPL2::setDrums(byte drums) {
	byte value = getChipRegister(0xBD) & 0xE0;
	setChipRegister(0xBD, value);
	setChipRegister(0xBD, value + (drums & 0x1F));
}


/**
 * Enable or disable various drum sounds.
 * Note that keyOn for channels 6, 7 and 8 must be false in order to use rhythms.
 */
void OPL2::setDrums(bool bass, bool snare, bool tom, bool cymbal, bool hihat) {
	byte drums = 0;
	drums += bass   ? DRUM_BITS_BASS   : 0x00;
	drums += snare  ? DRUM_BITS_SNARE  : 0x00;
	drums += tom    ? DRUM_BITS_TOM    : 0x00;
	drums += cymbal ? DRUM_BITS_CYMBAL : 0x00;
	drums += hihat  ? DRUM_BITS_HI_HAT : 0x00;
	setDrums(drums);
}


/**
 * Get the wave form currently set for the given channel.
 */
byte OPL2::getWaveForm(byte channel, byte operatorNum) {
	return getOperatorRegister(0xE0, channel, operatorNum) & 0x07;
}


/**
 * Select the wave form to use.
 */
void OPL2::setWaveForm(byte channel, byte operatorNum, byte waveForm) {
	byte value = getOperatorRegister(0xE0, channel, operatorNum) & 0xF8;
	setOperatorRegister(0xE0, channel, operatorNum, value + (waveForm & 0x07));
}


/**
 * Clamp the given value to be between the given minimum and maximum.
 *
 * @param value - The value to clamp.
 * @param min - Minimum clamping value.
 * @param max - Maximum clamping vlue.
 * @return The value clamped between the given min and max.
 */
template <typename T>
T OPL2::clampValue(T value, T min, T max) {
	#if OPL_BOARD_TYPE == OP2L_BOARD_TYPE_ARDUINO
		return constrain(value, min, max);
	#else
		return std::max(min, std::min(value, max));
	#endif
}
