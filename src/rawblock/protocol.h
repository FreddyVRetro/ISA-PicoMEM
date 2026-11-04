/* SPDX-License-Identifier: GPL-2.0-or-later
 * Experimental raw USB block ABI 1. Not an upstream allocation.
 * Byte encoding avoids host packing/endianness and 16-bit int assumptions.
 */
#ifndef PM_BLOCK_PROTOCOL_H
#define PM_BLOCK_PROTOCOL_H
#define PMB_VERSION 1
#define PMB_HEADER 32
#define PMB_SECTOR 512
#define PMB_FRAME (PMB_HEADER + PMB_SECTOR)
#define PMB_STATUS_PORT 5
#define PMB_IDENT_PORT 6
#define PMB_DATA_PORT 7
#define PMB_IDENT 0xB7
#define PMB_IDLE 0xB0
#define PMB_RX 0xB1
#define PMB_BUSY 0xB2
#define PMB_DONE 0xB3
#define PMB_BEGIN 0xA5
#define PMB_SUBMIT 0x5A
#define PMB_ACK 0xC3
#define PMB_INFO 1
#define PMB_CLAIM 2
#define PMB_READ 3
#define PMB_RELEASE 4
#define PMB_OK 0
#define PMB_BAD_REQUEST 1
#define PMB_NO_MEDIA 2
#define PMB_STALE 3
#define PMB_NOT_OWNER 4
#define PMB_RANGE 5
#define PMB_IO 6
#define PMB_FAULT 7
#define PMB_UNSUPPORTED 8
#define PMB_FLAGS 3 /* read-only and exclusive raw mode; no FatFs USB access */
#define PMB_GET16(p) ((unsigned)(p)[0] | ((unsigned)(p)[1] << 8))
#define PMB_GET32(p) ((unsigned long)PMB_GET16(p) | ((unsigned long)PMB_GET16((p)+2) << 16))
#define PMB_PUT16(p,v) do { unsigned pmb_v16=(v); (p)[0]=(unsigned char)pmb_v16; (p)[1]=(unsigned char)(pmb_v16>>8); } while (0)
#define PMB_PUT32(p,v) do { unsigned long pmb_v32=(v); PMB_PUT16(p,pmb_v32); PMB_PUT16((p)+2,pmb_v32>>16); } while (0)
#endif
