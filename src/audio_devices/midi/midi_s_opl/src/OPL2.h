#ifndef TRUE
	#define TRUE 1
#endif


#ifndef FALSE
	#define FALSE 0
#endif


#ifndef NULL
	#define NULL 0
#endif


#ifndef OPL2_LIB_H_
	#define OPL2_LIB_H_

	#define OPL2_BOARD_TYPE_ARDUINO      0
	#define OPL2_BOARD_TYPE_RASPBERRY_PI 1
	#define OPL2_BOARD_TYPE_PICOMEM      2 

	// !!! IMPORTANT !!!
	// In order to correctly compile the library for your platform be sure to set the correct BOARD_TYPE below.
	#define OPL2_BOARD_TYPE OPL2_BOARD_TYPE_PICOMEM

	#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		#define PIN_LATCH 10
		#define PIN_ADDR   9
		#define PIN_RESET  8
	#else
		#define PIN_LATCH 3				// GPIO header pin 15
		#define PIN_ADDR  4				// GPIO header pin 16
		#define PIN_RESET 2				// GPIO header pin 13

		// SPI setup for WiringPi.
		#define SPI_SPEED   8000000
		#define SPI_CHANNEL 0
	#endif

	// Generic OPL2 definitions.
	#define OPL2_NUM_CHANNELS 9
	#define CHANNELS_PER_BANK 9

	// Operator definitions.
	#define OPERATOR1 0
	#define OPERATOR2 1
	#define MODULATOR 0
	#define CARRIER   1

	// Synthesis type definitions.
	#define SYNTH_MODE_FM 0
	#define SYNTH_MODE_AM 1

	// Drum sounds.
	#define DRUM_BASS   0
	#define DRUM_SNARE  1
	#define DRUM_TOM    2
	#define DRUM_CYMBAL 3
	#define DRUM_HI_HAT 4

	// Drum sound bit masks.
	#define DRUM_BITS_BASS   0x10
	#define DRUM_BITS_SNARE  0x08
	#define DRUM_BITS_TOM    0x04
	#define DRUM_BITS_CYMBAL 0x02
	#define DRUM_BITS_HI_HAT 0x01

	// Note to frequency mapping.
	#define NOTE_C   0
	#define NOTE_CS  1
	#define NOTE_D   2
	#define NOTE_DS  3
	#define NOTE_E   4
	#define NOTE_F   5
	#define NOTE_FS  6
	#define NOTE_G   7
	#define NOTE_GS  8
	#define NOTE_A   9
	#define NOTE_AS 10
	#define NOTE_B  11

	// Tune specific declarations.
	#define NUM_OCTAVES      7
	#define NUM_NOTES       12
	#define NUM_DRUM_SOUNDS  5

	// Instrument data sources (Arduino only).
	#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		#define INSTRUMENT_DATA_PROGMEM true
		#define INSTRUMENT_DATA_SRAM false
	#endif

	#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
		#include <Arduino.h>
	#else
		#include <stdint.h>
		#include <algorithm>
		typedef uint8_t byte;
		#define PROGMEM 
	#endif


	struct Operator {
		bool hasTremolo;
		bool hasVibrato;
		bool hasSustain;
		bool hasEnvelopeScaling;
		byte frequencyMultiplier;
		byte keyScaleLevel;
		byte outputLevel;
		byte attack;
		byte decay;
		byte sustain;
		byte release;
		byte waveForm;
	};


	struct Instrument {
		Operator operators[2];
		byte feedback;
		bool isAdditiveSynth;
		byte transpose;
	};


	class OPL2 {
		public:
			OPL2();
			OPL2(byte reset, byte address, byte latch);
			virtual void begin();
			virtual void reset();
			virtual void createShadowRegisters();
			void init();

			virtual byte getChipRegister(short reg);
			virtual byte getChannelRegister(byte baseRegister, byte channel);
			virtual byte getOperatorRegister(byte baseRegister, byte channel, byte op);
			virtual byte getRegisterOffset(byte channel, byte operatorNum);
			virtual void setChipRegister(short reg, byte value);
			virtual void setChannelRegister(byte baseRegister, byte channel, byte value);
			virtual void setOperatorRegister(byte baseRegister, byte channel, byte op, byte value);
			virtual byte getChipRegisterOffset(short reg);
			virtual byte getChannelRegisterOffset(byte baseRegister, byte channel);
			virtual short getOperatorRegisterOffset(byte baseRegister, byte channel, byte operatorNum);
			virtual void write(byte reg, byte data);

			virtual byte getNumChannels();

			float getFrequency(byte channel);
			void setFrequency(byte channel, float frequency);
			byte getFrequencyBlock(float frequency);
			short getFrequencyFNumber(byte channel, float frequency);
			short getNoteFNumber(byte note);
			float getFrequencyStep(byte channel);
			void playNote(byte channel, byte octave, byte note);
			void playDrum(byte drum, byte octave, byte note);

			Instrument createInstrument();
			#if OPL2_BOARD_TYPE == OPL2_BOARD_TYPE_ARDUINO
				Instrument loadInstrument(const unsigned char *instrument, bool fromProgmem = INSTRUMENT_DATA_PROGMEM);
			#else
				Instrument loadInstrument(const unsigned char *instrument);
			#endif
			Instrument getInstrument(byte channel);
			void setInstrument(byte channel, Instrument instrument, float volume = 1.0);
			void setDrumInstrument(Instrument instrument, byte drumType, float volume = 1.0);

			virtual bool getWaveFormSelect();
			bool getTremolo(byte channel, byte operatorNum);
			bool getVibrato(byte channel, byte operatorNum);
			bool getMaintainSustain(byte channel, byte operatorNum);
			bool getEnvelopeScaling(byte channel, byte operatorNum);
			byte getMultiplier(byte channel, byte operatorNum);
			byte getScalingLevel(byte channel, byte operatorNum);
			byte getVolume(byte channel, byte operatorNum);
			byte getChannelVolume(byte channel);
			byte getAttack(byte channel, byte operatorNum);
			byte getDecay(byte channel, byte operatorNum);
			byte getSustain(byte channel, byte operatorNum);
			byte getRelease(byte channel, byte operatorNum);
			short getFNumber(byte channel);
			byte getBlock(byte channel);
			bool getNoteSelect();
			bool getKeyOn(byte channel);
			byte getFeedback(byte channel);
			byte getSynthMode(byte channel);
			bool getDeepTremolo();
			bool getDeepVibrato();
			bool getPercussion();
			byte getDrums();
			byte getWaveForm(byte channel, byte operatorNum);

			virtual void setWaveFormSelect(bool enable);
			void setTremolo(byte channel, byte operatorNum, bool enable);
			void setVibrato(byte channel, byte operatorNum, bool enable);
			void setMaintainSustain(byte channel, byte operatorNum, bool enable);
			void setEnvelopeScaling(byte channel, byte operatorNum, bool enable);
			void setMultiplier(byte channel, byte operatorNum, byte multiplier);
			void setScalingLevel(byte channel, byte operatorNum, byte scaling);
			void setVolume(byte channel, byte operatorNum, byte volume);
			void setChannelVolume(byte channel, byte volume);
			void setAttack(byte channel, byte operatorNum, byte attack);
			void setDecay(byte channel, byte operatorNum, byte decay);
			void setSustain(byte channel, byte operatorNum, byte sustain);
			void setRelease(byte channel, byte operatorNum, byte release);
			void setFNumber(byte channel, short fNumber);
			void setBlock(byte channel, byte block);
			void setNoteSelect(bool enable);
			void setKeyOn(byte channel, bool keyOn);
			void setFeedback(byte channel, byte feedback);
			void setSynthMode(byte channel, byte synthMode);
			void setDeepTremolo(bool enable);
			void setDeepVibrato(bool enable);
			void setPercussion(bool enable);
			void setDrums(byte drums);
			void setDrums(bool bass, bool snare, bool tom, bool cymbal, bool hihat);
			void setWaveForm(byte channel, byte operatorNum, byte waveForm);

		protected:
			template <typename T>
			T clampValue(T value, T min, T max);

			byte pinReset   = PIN_RESET;
			byte pinAddress = PIN_ADDR;
			byte pinLatch   = PIN_LATCH;

			byte* chipRegisters;
			byte* channelRegisters;
			byte* operatorRegisters;

			byte numChannels = OPL2_NUM_CHANNELS;

			const float fIntervals[8] = {
				0.048, 0.095, 0.190, 0.379, 0.759, 1.517, 3.034, 6.069
			};
			const unsigned int noteFNumbers[12] = {
				0x156, 0x16B, 0x181, 0x198, 0x1B0, 0x1CA,
				0x1E5, 0x202, 0x220, 0x241, 0x263, 0x287
			};
			const float blockFrequencies[8] = {
				 48.503,   97.006,  194.013,  388.026,
				776.053, 1552.107, 3104.215, 6208.431
			};
			const byte registerOffsets[2][9] = {  
				{ 0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12 } ,   /*  initializers for operator 1 */
				{ 0x03, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15 }     /*  initializers for operator 2 */
			};
			const byte drumRegisterOffsets[2][5] = {
				{ 0x10, 0xFF, 0x12, 0xFF, 0x11 },
				{ 0x13, 0x14, 0xFF, 0x15, 0xFF }
			};
			const byte drumChannels[5] = {
				6, 7, 8, 8, 7
			};
			const byte drumBits[5] = {
				DRUM_BITS_BASS, DRUM_BITS_SNARE, DRUM_BITS_TOM, DRUM_BITS_CYMBAL, DRUM_BITS_HI_HAT
			};
	};
#endif

