#include "OPL2.h"

#ifndef OPL3_LIB_H_
	#define OPL3_LIB_H_

	#define OPL3_NUM_2OP_CHANNELS 18
	#define OPL3_NUM_4OP_CHANNELS 6
	#define CHANNELS_PER_BANK 9

	#define SYNTH_MODE_FM_FM 0
	#define SYNTH_MODE_FM_AM 1
	#define SYNTH_MODE_AM_FM 2
	#define SYNTH_MODE_AM_AM 3

	#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		#undef PIN_ADDR					// Undefine A0 and /IC pins from OPL2 and redefine for OPL3 Duo.
		#undef PIN_RESET

		#define PIN_BANK  7
		#define PIN_ADDR  8
		#define PIN_RESET 9
	#else
		#define PIN_BANK 5				// GPIO header pin 18
	#endif


	struct Instrument4OP {
		Instrument subInstrument[2];		// Definition of the 2 sub instruments for each channel.
											// Transpose of the instrument is found in transpose of sub instrument 0.
	};


	class OPL3: public OPL2 {
		public:
			OPL3();
			OPL3(byte a1, byte a0, byte latch, byte reset);
			virtual void begin();
			virtual void reset();
			virtual void createShadowRegisters();

			virtual void setChipRegister(short baseRegister, byte value);
			virtual void setChannelRegister(byte baseRegister, byte channel, byte value);
			virtual void setOperatorRegister(byte baseRegister, byte channel, byte operatorNum, byte value);
			virtual byte getChipRegisterOffset(short reg);
			virtual void write(byte bank, byte reg, byte value);

			virtual byte getNumChannels();
			virtual byte getNum4OPChannels();
			virtual byte get4OPControlChannel(byte channel4OP, byte index2OP = 0);

			Instrument4OP createInstrument4OP();
			#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
				Instrument4OP loadInstrument4OP(const unsigned char *instrument, bool fromProgmem = INSTRUMENT_DATA_PROGMEM);
			#else
				Instrument4OP loadInstrument4OP(const unsigned char *instrument);
			#endif
			Instrument4OP getInstrument4OP(byte channel4OP);
			void setInstrument4OP(byte channel4OP, Instrument4OP instrument, float volume = 1.0);

			virtual bool getWaveFormSelect();
			virtual void setWaveFormSelect(bool enable = false);

			virtual bool isOPL3Enabled();
			virtual void setOPL3Enabled(bool enable);
			virtual bool is4OPChannelEnabled(byte channel4OP);
			virtual void set4OPChannelEnabled(byte channel4OP, bool enable);
			virtual void setAll4OPChannelsEnabled(bool enable);

			bool isPannedLeft (byte channel);
			bool isPannedRight(byte channel);
			void setPanning(byte channel, bool left, bool right);
			byte get4OPSynthMode(byte channel4OP);
			void set4OPSynthMode(byte channel4OP, byte synthMode);
			byte get4OPChannelVolume(byte channel4OP);
			void set4OPChannelVolume(byte channel4OP, byte volume);


		protected:
			byte pinBank = PIN_BANK;

			byte numChannels = OPL3_NUM_2OP_CHANNELS;
			byte num4OPChannels = OPL3_NUM_4OP_CHANNELS;

			byte channelPairs4OP[6][2] = {
				{ 0,  3 }, {  1,  4 }, {  2,  5 },
				{ 9, 12 }, { 10, 13 }, { 11, 14 }
			};
	};
#endif
