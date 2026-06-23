#include "sbdsp.h"
#include "sbmix.h"

mixer_t sbmix;


static INLINE uint8_t expand16to32(const uint8_t t) {
	/* 4-bit -> 5-bit expansion.
	 *
	 * 0 -> 0
	 * 1 -> 2
	 * 2 -> 4
	 * 3 -> 6
	 * ....
	 * 7 -> 14
	 * 8 -> 17
	 * 9 -> 19
	 * 10 -> 21
	 * 11 -> 23
	 * ....
	 * 15 -> 31 */
	return (t << 1) | (t >> 3);
}


SBmix_reset(void) {
	sbmix.filter_bypass=0; // Creative Documentation: filter_bypass bit is 0 by default
	sbmix.fm[0]=
	sbmix.fm[1]=
	sbmix.cda[0]=
	sbmix.cda[1]=
	sbmix.dac[0]=
	sbmix.dac[1]=31;
	sbmix.master[0]=
	sbmix.master[1]=31;
	sbmix.update=true;
}

/* Sound Blaster Pro 2 (CT1600) notes:
 *
 * - Registers 0x40-0xFF do nothing written and read back 0xFF.
 * - Registers 0x00-0x3F are almost exact mirrors of registers 0x00-0x0F, but not quite
 * - Registers 0x00-0x1F are exact mirrors of 0x00-0x0F
 * - Registers 0x20-0x3F are exact mirrors of 0x20-0x2F which are.... non functional shadow copies of 0x00-0x0F (???)
 * - Register 0x0E is mirrored at 0x0F, 0x1E, 0x1F. Reading 0x00, 0x01, 0x10, 0x11 also reads register 0x0E.
 * - Writing 0x00, 0x01, 0x10, 0x11 resets the mixer as expected.
 * - Register 0x0E is 0x11 on reset, which defaults to mono and lowpass filter enabled.
 * - See screenshot for mixer registers on hardware or mixer reset, file 'CT1600 Sound Blaster Pro 2 mixer register dump hardware and mixer reset state.png' */

 #define SETPROVOL(_WHICH_,_VAL_)                                    \
    _WHICH_[0]=   ((((_VAL_) & 0xf0) >> 3)|(type==SBT_16 ? 1:3));    \


SBmix_WriteAddr(uint8_t index)
{
 sbmix.index=index;
}

CBmiw_WriteData(uint8_t val)

switch (sbmix.index) {
		case 0x00:      /* Reset */
			SBmix_reset();
			PM_INFO("Mixer reset value %x",val);
			break;
		case 0x02:      /* Master Volume (SB2 Only) */
			SETPROVOL(sbmix.master,(val&0xf)|(val<<4));
			sbmix.update=true;
			break;
		case 0x04:      /* DAC Volume (SBPRO) */
			SETPROVOL(sbmix.dac,val);
			sbmix.update=true;
			break;
		case 0x06:      /* FM output selection, Somewhat obsolete with dual OPL SBpro + FM volume (SB2 Only) */
			//volume controls both channels
			SETPROVOL(sbmix.fm,(val&0xf)|(val<<4));
			sbmix.update=true;
			if(val&0x60) LOG(LOG_SB,LOG_WARN)("Turned FM one channel off. not implemented %X",val);
			//TODO Change FM Mode if only 1 fm channel is selected
			break;
		case 0x08:      /* CDA Volume (SB2 Only) */
			SETPROVOL(sbmix.cda,(val&0xf)|(val<<4));
			sbmix.update=true;
			break;
		case 0x0a:      /* Mic Level (SBPRO) or DAC Volume (SB2): 2-bit, 3-bit on SB16 */
			if (type==SBT_2) {
				sbmix.dac[0]=sbmix.dac[1]=((val & 0x6) << 2)|3;
				sbmix.update=true;
			} else {
				sbmix.mic=((val & 0x7) << 2)|(type==SBT_16?1:3);
			}
			break;
		case 0x0e:      /* Output/Stereo Select */
			/* Real CT1600 notes:
			 *
			 * This register only allows changing bits 1 and 5. Each nibble can be 1 or 3, therefore on readback it will always be 0x11, 0x13, 0x31, or 0x11.
			 * This register is also mirrored at 0x0F, 0x1E, 0x1F. On read this register also appears over 0x00, 0x01, 0x10, 0x11, though writing that register
			 * resets the mixer as expected. */
			/* only allow writing stereo bit if Sound Blaster Pro OR if a SB16 and user says to allow it */
			if ((type == SBT_PRO1 || type == SBT_PRO2) || (type == SBT_16 && !sbpro_stereo_bit_strict_mode))
				sbmix.stereo=(val & 0x2) > 0;

			sbmix.sbpro_stereo=(val & 0x2) > 0;
			sbmix.filter_bypass=(val & 0x20) > 0; /* filter is ON by default, set the bit to turn OFF the filter */
			DSP_ChangeStereo(sbmix.stereo);
			updateSoundBlasterFilter(freq);

			/* help the user out if they leave sbtype=sb16 and then wonder why their DOS game is producing scratchy monural audio. */
			if (type == SBT_16 && sbpro_stereo_bit_strict_mode && (val&0x2) != 0)
				LOG(LOG_SB,LOG_WARN)("Mixer stereo/mono bit is set. This is Sound Blaster Pro style Stereo which is not supported by sbtype=sb16, consider using sbtype=sbpro2 instead.");
			break;
/*		case 0x14:      /* Audio 1 Play Volume (ESS 688)
			if (ess_type != ESS_NONE) {
				sbmix.dac[0]=expand16to32((val>>4)&0xF);
				sbmix.dac[1]=expand16to32(val&0xF);
				sbmix.update=true;
			}
			break; */
		case 0x22:      /* Master Volume (SBPRO) */
			SETPROVOL(sbmix.master,val);
			sbmix.update=true;
			break;
		case 0x26:      /* FM Volume (SBPRO) */
			SETPROVOL(sbmix.fm,val);
			sbmix.update=true;
			break;
		case 0x28:      /* CD Audio Volume (SBPRO) */
			SETPROVOL(sbmix.cda,val);
			sbmix.update=true;
			break;
		case 0x2e:      /* Line-in Volume (SBPRO) */
			SETPROVOL(sbmix.lin,val);
			break;
			//case 0x20:        /* Master Volume Left (SBPRO) ? */
		case 0x30:      /* Master Volume Left (SB16) */
			if (type==SBT_16) {
				sbmix.master[0]=val>>3;
				sbmix.update=true;
			}
			break;
			//case 0x21:        /* Master Volume Right (SBPRO) ? */
		case 0x31:      /* Master Volume Right (SB16) */
			if (type==SBT_16) {
				sbmix.master[1]=val>>3;
				sbmix.update=true;
			}
			break;
		case 0x32:      /* DAC Volume Left (SB16) */
			/* Master Volume (ESS 688) */
			if (type==SBT_16) {
				sbmix.dac[0]=val>>3;
				sbmix.update=true;
			}
/*            
			else if (ess_type != ESS_NONE) {
				sbmix.master[0]=expand16to32((val>>4)&0xF);
				sbmix.master[1]=expand16to32(val&0xF);
				sbmix.update=true;
			} */
			break;
		case 0x33:      /* DAC Volume Right (SB16) */
			if (type==SBT_16) {
				sbmix.dac[1]=val>>3;
				sbmix.update=true;
			}
			break;
		case 0x34:      /* FM Volume Left (SB16) */
			if (type==SBT_16) {
				sbmix.fm[0]=val>>3;
				sbmix.update=true;
			}
			break;
		case 0x35:      /* FM Volume Right (SB16) */
			if (type==SBT_16) {
				sbmix.fm[1]=val>>3;
				sbmix.update=true;
			}
			break;
		case 0x36:      /* CD Volume Left (SB16) */
			/* FM Volume (ESS 688) */
			if (type==SBT_16) {
				sbmix.cda[0]=val>>3;
				sbmix.update=true;
			}
			else if (ess_type != ESS_NONE) {
				sbmix.fm[0]=expand16to32((val>>4)&0xF);
				sbmix.fm[1]=expand16to32(val&0xF);
				sbmix.update=true;
			}
			break;
		case 0x37:      /* CD Volume Right (SB16) */
			if (type==SBT_16) {
				sbmix.cda[1]=val>>3;
				sbmix.update=true;
			}
			break;
		case 0x38:      /* Line-in Volume Left (SB16) */
			/* AuxA (CD) Volume Register (ESS 688) */
			if (type==SBT_16)
				sbmix.lin[0]=val>>3;
			else if (ess_type != ESS_NONE) {
				sbmix.cda[0]=expand16to32((val>>4)&0xF);
				sbmix.cda[1]=expand16to32(val&0xF);
				sbmix.update=true;
			}
			break;
		case 0x39:      /* Line-in Volume Right (SB16) */
			if (type==SBT_16) sbmix.lin[1]=val>>3;
			break;
		case 0x3a:
			if (type==SBT_16) sbmix.mic=val>>3;
			break;
/*            
		case 0x3e:      // Line Volume (ESS 688)
			if (ess_type != ESS_NONE) {
				sbmix.lin[0]=expand16to32((val>>4)&0xF);
				sbmix.lin[1]=expand16to32(val&0xF);
			}
			break;
		case 0x80:      // IRQ Select
			if (type==SBT_16) { // ViBRA PnP cards do not allow reconfiguration by this byte
				hw.irq=0xff;
				if (IS_PC98_ARCH) {
					if (val & 0x1) hw.irq=3;
					else if (val & 0x2) hw.irq=10;
					else if (val & 0x4) hw.irq=12;
					else if (val & 0x8) hw.irq=5;

					// NTS: Real hardware stores only the low 4 bits. The upper 4 bits will always read back 1111.
					//      The value read back will always be Fxh where x contains the 4 bits checked here.
				}
				else {
					if (val & 0x1) hw.irq=2;
					else if (val & 0x2) hw.irq=5;
					else if (val & 0x4) hw.irq=7;
					else if (val & 0x8) hw.irq=10;
				}
			}
			break;
		case 0x81:      // DMA Select
			if (type==SBT_16) { // ViBRA PnP cards do not allow reconfiguration by this byte
				hw.dma8=0xff;
				hw.dma16=0xff;
				if (IS_PC98_ARCH) {
					pc98_mixctl_reg = (unsigned char)val ^ 0x14;

					if (val & 0x1) hw.dma8=0;
					else if (val & 0x2) hw.dma8=3;

					// NTS: On real hardware, only bits 0 and 1 are writeable. bits 2 and 4 seem to act oddly in response to
					//      bytes written:
					//
					//      write 0x00          read 0x14
					//      write 0x01          read 0x15
					//      write 0x02          read 0x16
					//      write 0x03          read 0x17
					//      write 0x04          read 0x10
					//      write 0x05          read 0x11
					//      write 0x06          read 0x12
					//      write 0x07          read 0x13
					//      write 0x08          read 0x1C
					//      write 0x09          read 0x1D
					//      write 0x0A          read 0x1E
					//      write 0x0B          read 0x1F
					//      write 0x0C          read 0x18
					//      write 0x0D          read 0x19
					//      write 0x0E          read 0x1A
					//      write 0x0F          read 0x1B
					//      write 0x10          read 0x04
					//      write 0x11          read 0x05
					//      write 0x12          read 0x06
					//      write 0x13          read 0x07
					//      write 0x14          read 0x00
					//      write 0x15          read 0x01
					//      write 0x16          read 0x02
					//      write 0x17          read 0x03
					//      write 0x18          read 0x0C
					//      write 0x19          read 0x0D
					//      write 0x1A          read 0x0E
					//      write 0x1B          read 0x0F
					//      write 0x1C          read 0x08
					//      write 0x1D          read 0x09
					//      write 0x1E          read 0x0A
					//      write 0x1F          read 0x0B
					//
					//      This pattern repeats for any 5 bit value in bits [4:0] i.e. 0x20 will read back 0x34.
				}
				else {
					if (val & 0x1) hw.dma8=0;
					else if (val & 0x2) hw.dma8=1;
					else if (val & 0x8) hw.dma8=3;
					if (val & 0x20) hw.dma16=5;
					else if (val & 0x40) hw.dma16=6;
					else if (val & 0x80) hw.dma16=7;
				}
				LOG(LOG_SB,LOG_NORMAL)("Mixer select dma8:%x dma16:%x",hw.dma8,hw.dma16);
			}
			break;
*/            
		default:

			if( ((type == SBT_PRO1 || type == SBT_PRO2) && sbmix.index==0x0c) || /* Input control on SBPro */
					(type == SBT_16 && sbmix.index >= 0x3b && sbmix.index <= 0x47)) /* New SB16 registers */
				sbmix.unhandled[sbmix.index] = val;
			LOG(LOG_SB,LOG_WARN)("MIXER:Write %X to unhandled index %X",val,sbmix.index);
	}
}