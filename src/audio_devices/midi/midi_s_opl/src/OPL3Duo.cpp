/**
 * ________ __________.____    ________      _____            .___.__         .____    ._____.
 * \_____  \\______   \    |   \_____  \    /  _  \  __ __  __| _/|__| ____   |    |   |__\_ |__
 *  /   |   \|     ___/    |    /  ____/   /  /_\  \|  |  \/ __ | |  |/  _ \  |    |   |  || __ \
 * /    |    \    |   |    |___/       \  /    |    \  |  / /_/ | |  (  <_> ) |    |___|  || \_\ \
 * \_______  /____|   |_______ \_______ \ \____|__  /____/\____ | |__|\____/  |_______ \__||___  /
 *         \/                 \/       \/         \/           \/                     \/       \/
 *   ___________         __                       .__                         _____
 *   \_   _____/__  ____/  |_  ____   ____   _____|__| ____   ____   ______ _/ ____\___________
 *    |    __)_\  \/  /\   __\/ __ \ /    \ /  ___/  |/  _ \ /    \ /  ___/ \   __\/  _ \_  __ \
 *    |        \>    <  |  | \  ___/|   |  \\___ \|  (  <_> )   |  \\___ \   |  | (  <_> )  | \/
 *   /_______  /__/\_ \ |__|  \___  >___|  /____  >__|\____/|___|  /____  >  |__|  \____/|__|
 *           \/      \/           \/     \/     \/               \/     \/
 *                ________ __________.____     ________   ________              ._.
 *                \_____  \\______   \    |    \_____  \  \______ \  __ __  ____| |
 *                 /   |   \|     ___/    |      _(__  <   |    |  \|  |  \/  _ \ |
 *                /    |    \    |   |    |___  /       \  |    `   \  |  (  <_> )|
 *                \_______  /____|   |_______ \/______  / /_______  /____/ \____/__
 *                        \/                 \/       \/          \/             \/
 *
 * Extensions to the OPL2 Audio Library for Arduino and compatibles to support the OPL3 Duo! board.
 * This file contains the extensions required for dual YMF262.
 * Code by Maarten janssen (maarten@cheerful.nl) 2020-10-12
 * WWW.CHEERFUL.NL
 */


#include "OPL3Duo.h"

// TODO: Change BOARD_TYPE to PLATFORM
#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
	#include <SPI.h>
	#include <Arduino.h>
#else
	#include <wiringPi.h>
	#include <wiringPiSPI.h>
#endif


/**
 * Create a new OPL3Duo instance with default pins.
 *
 * A2 = D6
 * A1 = D7
 * A0 = D8
 * /IC = D9
 * /WR = D10
 */
OPL3Duo::OPL3Duo() : OPL3() {
}


/**
 * Create a new OPL3Duo instance with custom pins.
 *
 * @param a2 - Pin number to use for A2.
 * @param a1 - Pin number to use for A1.
 * @param a0 - Pin number to use for A0.
 * @param latch - Pin number to use for LATCH.
 * @param reset - Pin number to use for RESET.
 */
OPL3Duo::OPL3Duo(byte a2, byte a1, byte a0, byte latch, byte reset) : OPL3(a1, a0, latch, reset) {
	pinUnit = a2;
}


/**
 * Initialize the OPL3Duo and reset the chips.
 */
void OPL3Duo::begin() {
	pinMode(pinUnit, OUTPUT);
	digitalWrite(pinUnit, LOW);
	OPL3::begin();
}


/**
 * Create shadow registers to hold the values written to the OPL3 chips for later access. Only those registers that are
 * are valid on the YMF262 are created to be as memory friendly as possible for platforms with limited RAM such as the
 * Arduino Uno / Nano. Registers consume 478 bytes.
 */
void OPL3Duo::createShadowRegisters() {
	chipRegisters = new byte[5 * 2];					//  10
	channelRegisters = new byte[3 * numChannels];		// 108
	operatorRegisters = new byte[10 * numChannels];		// 360
}


/**
 * Hard reset the OPL3 chip. All registers will be reset to 0x00, This should be done before sending any register data
 * to the chip.
 */
void OPL3Duo::reset() {
	// Hard reset both OPL3 chips.
	for (byte unit = 0; unit < 2; unit ++) {
		digitalWrite(pinUnit, unit == 1);
		digitalWrite(pinReset, LOW);
		delay(1);
		digitalWrite(pinReset, HIGH);
	}

	// Initialize chip registers on both synth units.
	for (byte i = 0; i < 2; i ++) {
		setChipRegister(i, 0x01, 0x00);
		setChipRegister(i, 0x08, 0x40);
		setChipRegister(i, 0xBD, 0x00);
		setChipRegister(i, 0x104, 0x00);
	}

	// Enable OPL3 mode for both chips so we can access all operators.
	setChipRegister(0, 0x105, 0x01);
	setChipRegister(1, 0x105, 0x01);

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

	// Disable OPL3 mode for both chips.
	setChipRegister(0, 0x105, 0x00);
	setChipRegister(1, 0x105, 0x00);

	digitalWrite(pinUnit, LOW);
}


/**
 * Get the current value of a chip wide register from the shadow registers.
 * 
 * @param synthUnit - The chip to address [0, 1]
 * @param reg - The 9-bit address of the register.
 * @return The value of the register from shadow registers.
 */
byte OPL3Duo::getChipRegister(byte synthUnit, short reg) {
	synthUnit = synthUnit & 0x01;
	return chipRegisters[(synthUnit * 5) + getChipRegisterOffset(reg)];
}


/**
 * Write a given value to a chip wide register.
 *
 * @param synthUnit - The chip to address [0, 1]
 * @param reg - The 9-bit register to write to.
 * @param value - The value to write to the register.
 */
void OPL3Duo::setChipRegister(byte synthUnit, short reg, byte value) {
	synthUnit = synthUnit & 0x01;
	chipRegisters[(synthUnit * 5) + getChipRegisterOffset(reg)] = value;

	byte bank = (synthUnit << 1) | ((reg >> 8) & 0x01);
	write(bank, reg & 0xFF, value);
}


/**
 * Write a given value to a channel based register.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel to address [0, 17]
 * @param value - The value to write to the register.
 */
void OPL3Duo::setChannelRegister(byte baseRegister, byte channel, byte value) {
	channelRegisters[getChannelRegisterOffset(baseRegister, channel)] = value;

	byte bank = (channel / CHANNELS_PER_BANK) & 0x03;
	byte reg = baseRegister + (channel % CHANNELS_PER_BANK);
	write(bank, reg, value);
}


/**
 * Write a given value to an operator register for a channel.
 *
 * @param baseRegister - The base address of the register.
 * @param channel - The channel of the operator [0, 17]
 * @param op - The operator to change [0, 1].
 * @param value - The value to write to the operator's register.
 */
void OPL3Duo::setOperatorRegister(byte baseRegister, byte channel, byte operatorNum, byte value) {
	operatorRegisters[getOperatorRegisterOffset(baseRegister, channel, operatorNum)] = value;

	byte bank = (channel / CHANNELS_PER_BANK) & 0x03;
	byte reg = baseRegister + getRegisterOffset(channel % CHANNELS_PER_BANK, operatorNum);
	write(bank, reg, value);
}


/**
 * Write a given value to a register of the OPL3 chip.
 *
 * @param bank - The bank + unit (A1 + A2) of the register [0, 3].
 * @param reg - The register to be changed.
 * @param value - The value to write to the register.
 */
void OPL3Duo::write(byte bank, byte reg, byte value) {
	digitalWrite(pinUnit, (bank & 0x02) ? HIGH : LOW);
	OPL3::write(bank, reg, value);
}


/**
 * Get the number of 2OP channels for this implementation.
 *
 * @return The number of 2OP channels.
 */
byte OPL3Duo::getNumChannels() {
	return numChannels;
}


/**
 * Get the number of 4OP channels for this implementation.
 *
 * @return The number of 4OP channels.
 */
byte OPL3Duo::getNum4OPChannels() {
	return num4OPChannels;
}


/**
 * Get the 2-OP channel that is associated with the given 4 operator channel.
 *
 * @param channel4OP - The 4 operator channel [0, 11] for wich we want to get the associated OPL channel.
 * @param index2OP - Then 2 operator channel index [0, 1], defaults to 0 for control channel.
 * @return The OPL3 channel number that controls the 4 operator channel.
 */
byte OPL3Duo::get4OPControlChannel(byte channel4OP, byte index2OP) {
	return channelPairs4OP[channel4OP % getNum4OPChannels()][index2OP % 2];
}


/**
 * Returns whether OPL3 mode is currently enabled on both synth units.
 *
 * @return True if OPL3 mode is enabled.
 */
bool OPL3Duo::isOPL3Enabled(byte synthUnit) {
	return getChipRegister(synthUnit & 0x01, 0x105) & 0x01;
}


/**
 * Returns whether OPL3 mode is currently enabled on both synth units.
 *
 * @return True if OPL3 mode is enabled on both chips.
 */
bool OPL3Duo::isOPL3Enabled() {
	return (getChipRegister(0, 0x105) & 0x01) == 0x01 &&
		(getChipRegister(1, 0x105) & 0x01) == 0x01;
}


/**
 * Enable or disable OPL3 mode on both synth units. This function must be called in order to use any of the OPL3
 * functions. It will also set panning for all channels to enable both left and right speakers when OPL3 mode is
 * enabled.
 *
 * @param enable - When set to true enables OPL3 mode.
 */
void OPL3Duo::setOPL3Enabled(bool enable) {
	setChipRegister(0, 0x105, enable ? 0x01 : 0x00);
	setChipRegister(1, 0x105, enable ? 0x01 : 0x00);

	// For ease of use enable both the left and the right speaker on all channels when going into OPL3 mode.
	for (byte i = 0; i < getNumChannels(); i ++) {
		setPanning(i, enable, enable);
	}
}


/**
 * Enable or disable OPL3 mode for a single synth unit only. This function must be called in order to use any of the
 * OPL3 functions. It will also set panning for all channels to enable both left and right speakers when OPL3 mode is
 * enabled.
 *
 * @param synthUnit - Synth unit [0, 1] for which to change OPL3 mode.
 * @param enable - When set to true enables OPL3 mode.
 */
void OPL3Duo::setOPL3Enabled(byte synthUnit, bool enable) {
	synthUnit = synthUnit & 0x01;
	setChipRegister(synthUnit, 0x105, enable ? 0x01 : 0x00);

	// For ease of use enable both the left and the right speaker on all channels when going into OPL3 mode.
	byte firstChannel = synthUnit == 0 ? 0 : getNumChannels() / 2;
	byte lastChannel  = synthUnit == 0 ? getNumChannels() / 2 : getNumChannels();
	for (byte i = firstChannel; i < lastChannel; i ++) {
		setPanning(i, enable, enable);
	}
}


/**
 * Is the given 4-OP channel enabled?
 *
 * @param channel4OP -The 4-OP cahnnel [0, 11] for which we want to know if 4-operator mode is enabled.
 * @return True if the given 4-OP channel is in 4-operator mode.
 */
bool OPL3Duo::is4OPChannelEnabled(byte channel4OP) {
	channel4OP = channel4OP % getNum4OPChannels();
	byte synthUnit = channel4OP >= NUM_4OP_CHANNELS_PER_UNIT ? 1 : 0;
	byte channelMask = 0x01 << (channel4OP % NUM_4OP_CHANNELS_PER_UNIT);
	return getChipRegister(synthUnit, 0x0104) & channelMask;
}


/**
 * Enable or disable 4-operator mode on the given 4-OP channel.
 *
 * @param channel4OP - The 4-OP channel [0, 11] for which to enable or disbale 4-operator mode.
 * @param enable - Enables or disable 4 operator mode.
 */
void OPL3Duo::set4OPChannelEnabled(byte channel4OP, bool enable) {
	channel4OP = channel4OP % getNum4OPChannels();
	byte synthUnit = channel4OP >= NUM_4OP_CHANNELS_PER_UNIT ? 1 : 0;
	byte channelMask = 0x01 << (channel4OP % NUM_4OP_CHANNELS_PER_UNIT);
	byte value = getChipRegister(synthUnit, 0x0104) & ~channelMask;
	setChipRegister(synthUnit, 0x0104, value + (enable ? channelMask : 0));
}


/**
 * Enable or disable 4-op mode on all channels of both synth units.
 *
 * @param enable - When set to true enables 4-op mode on all channels.
 */
void OPL3Duo::setAll4OPChannelsEnabled(bool enable) {
	setAll4OPChannelsEnabled(0, enable);
	setAll4OPChannelsEnabled(1, enable);
}


/**
 * Enable or disable 4-op mode on all channels of the given synth unit.
 *
 * @param synthUnit - Synth unit [0, 1] for which to change OPL3 mode.
 * @param enable - When set to true enables 4-op mode on all channels.
 */
void OPL3Duo::setAll4OPChannelsEnabled(byte synthUnit, bool enable) {
	setChipRegister(synthUnit, 0x0104, enable ? 0x3F : 0x00);
}
