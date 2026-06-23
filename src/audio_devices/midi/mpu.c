
// Sound Blaster MPU
// https://github.com/Cacodemon345/VSBHDASF/tree/main/src

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "CONFIG.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VMPU.H"
#include "PLATFORM.H"

#define MIDI_BUFF_SIZE 1024

extern struct globalvars gvars;

/* 0x330: data port
 * 0x331: read: status port
 *       write: command port
 * status port:
 * bit 6: 0=ready to write cmd or MIDI data; 1=interface busy
 * bit 7: 0=data ready to read; 1=no data at data port
 * command port:
 *  0xff: reset - triggers ACK (FE) to be read from data port
 *  0x3f: set to UART mode - triggers ACK (FE) to be read from data port
 */

static bool bReset = false;
static uint8_t bUART = 0;

static const int midi_lengths[8] = { 3, 3, 3, 3, 2, 2, 3, 1 };
static unsigned char midi_buffer[MIDI_BUFF_SIZE];
static unsigned char midi_temp_buffer[MIDI_BUFF_SIZE];
static unsigned int midi_ptr = 0;
static unsigned int midi_available_ptr = 0;
static unsigned int midi_message_cntr = 0;
static bool midi_check_status_byte = 0;
static bool midi_in_sysex = false;
static unsigned char midi_status_byte = 0x80;
static unsigned char midi_mpu_status = 0x80;
static const unsigned char gm_reset[6] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
static const unsigned char gs_reset[11] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };

extern tsf* tsfrenderer;

void VMPU_Process_Messages(void)
{
        unsigned char* temp_buffer = midi_buffer;
        unsigned int index = 0;
        while (index < midi_available_ptr) {
                switch (*temp_buffer & 0xF0) {
                        case 0xD0:
                                tsf_channel_set_pressure(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1] / 127.f);
                        case 0xA0:
                                {
                                        index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                        temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                        break;
                                }
                        case 0x80:
                                tsf_channel_note_off(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1]);
                                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                break;
                        case 0x90:
                                tsf_channel_note_on(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1], temp_buffer[2] / 127.0f);
                                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                break;
                        case 0xE0:
                                tsf_channel_set_pitchwheel(tsfrenderer, temp_buffer[0] & 0xf, (temp_buffer[1] & 0x7f) | ((temp_buffer[2] & 0x7f) << 7));
                                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                break;
                        case 0xC0:
                                tsf_channel_set_presetnumber(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1], (temp_buffer[0] & 0xf) == 0x9);
                                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                break;
                        case 0xB0:
                                tsf_channel_midi_control(tsfrenderer, temp_buffer[0] & 0xf, temp_buffer[1], temp_buffer[2]);
                                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                break;
                        case 0xF0: {
                                        if (*temp_buffer == 0xFF) {
                                                {
							tsf_reset(tsfrenderer);
                                                        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
                                                        tsf_set_volume(tsfrenderer, 1.0f);
                                                }
                                        }
                                        if (*temp_buffer == 0xF0) {
                                                unsigned char* sysexbuf = temp_buffer;
                                                unsigned int sysexlen = 0, i = 0;
                                                while (*temp_buffer != 0xF7) {
                                                        index++;
                                                        temp_buffer += 1;
                                                        sysexlen++;
                                                }

                                                index++;
                                                temp_buffer += 1;
                                                sysexlen++;

//						_dprintf("SYSEX MESSAGE: ");
//						for (i = 0; i < sysexlen; i++) {
//							_dprintf("0x%02X ", sysexbuf[i]);
//						}
//						_dprintf("\n");

                                                if (sysexbuf[1] == 0x41 && sysexbuf[3] == 0x42 && sysexbuf[4] == 0x12 && sysexlen >= 9) {
                                                        uint32_t addr = ((uint32_t)sysexbuf[5] << 16) + ((uint32_t)sysexbuf[6] << 8) + (uint32_t)sysexbuf[7];
                                                        if (addr == 0x400004 && tsfrenderer) {
                                                                tsf_set_volume(tsfrenderer, ((sysexbuf[8] > 127) ? 127 : sysexbuf[8]) / 127.f);
                                                        }
                                                }
                                                if (tsfrenderer && sysexbuf[1] == 0x7f && sysexbuf[2] == 0x7f && sysexbuf[3] == 0x04 && sysexbuf[4] == 0x01)
                                                {
//                                                        _dprintf("GM Master Vol 0x%02X\n", sysexbuf[6]);
                                                        tsf_set_volume(tsfrenderer, sysexbuf[6] / 127.f);
                                                }
                                                // TODO: Differentiate between GS and GM Resets.
                                                if (!memcmp(sysexbuf, gs_reset, sizeof(gs_reset)) || !memcmp(sysexbuf, gm_reset, sizeof(gm_reset))) {
							tsf_reset(tsfrenderer);
                                                        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
                                                        tsf_set_volume(tsfrenderer, 1.0f);
                                                }

                                        } else {
                                                index += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                                temp_buffer += midi_lengths[(temp_buffer[0] >> 4) - 0x8];
                                        }
                                        break;
                                }
                }
        }

        memcpy(midi_temp_buffer, midi_buffer + midi_available_ptr, midi_ptr - midi_available_ptr);
        memcpy(midi_buffer, midi_temp_buffer, midi_ptr - midi_available_ptr);
        midi_ptr -= midi_available_ptr;
        midi_available_ptr = 0;
}

static void VMPU_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
        dbgprintf(("VMPU_Write(%X)=%X\n", port, value ));
        if ( port == 0x331 ) {
        	if ( value == 0x3f ) {
                	midi_mpu_status &= ~0x80;
                }
                if ( value == 0xff ) {
                        VMPU_Process_Messages();
                        bReset = true;
                        midi_ptr = 0;
                        midi_available_ptr = 0;
                        midi_message_cntr = 0;
                        midi_check_status_byte = false;
                        midi_mpu_status &= ~0x80;
                }
        } else {
                if (!bReset) {
                        {
                                if (midi_ptr >= (MIDI_BUFF_SIZE-4))
                                        return;

                                if (midi_in_sysex) {                                
                                        midi_buffer[midi_ptr++] = value;
                                        if (value == 0xF7) {
                                                midi_available_ptr = midi_ptr;
                                                midi_message_cntr = 0;
                                                midi_check_status_byte = false;
                                                midi_in_sysex = false;
                                        }
                                        return;
                                }

                                if ((value & 0xF0) < 0x80 && midi_check_status_byte) {
                                        midi_buffer[midi_ptr++] = midi_status_byte;
                                        midi_check_status_byte = false;
				}
				
                                if ((value & 0xF0) >= 0x80) {
                                        midi_status_byte = value;
                                        midi_check_status_byte = false;
                                }
        
                                midi_buffer[midi_ptr++] = value;
                                midi_message_cntr++;

                                if (midi_message_cntr >= midi_lengths[(midi_buffer[midi_available_ptr] >> 4) - 0x8]) {
                                        if (value == 0xF0) {
                                                midi_message_cntr = 0;
                                                midi_in_sysex = true;
                                                return;
                                        }
                                        midi_available_ptr = midi_ptr;
                                        midi_message_cntr = 0;
                                        midi_check_status_byte = true;
                                }
                        } else PM_INFO("RST");
                }
        }
    return;
}

static uint8_t VMPU_Read(uint16_t port)
///////////////////////////////////////
{
        dbgprintf(("VMPU_Read(%X)\n", port ));
        if ( port == 0x330 ) {
                midi_mpu_status |= 0x80;
                if ( bReset ) {
                        bReset = false;
                        return 0xfe;
                }
                return 0xfe; // Always return Active Sensing.
        } else {
                return midi_mpu_status | (midi_ptr >= (MIDI_BUFF_SIZE-4) ? 0x40 : 0);
        }
}

/* SB-MIDI data written with DSP cmd 0x38 */

void VMPU_SBMidi_RawWrite( uint8_t value )
//////////////////////////////////////////
{
}

uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VMPU_Write(port, val), val) : ( val &= ~0xff, val |= VMPU_Read(port) );
}
#endif
