/* Copyright (C) 2024 Freddy VETELE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. 
If not, see <https://www.gnu.org/licenses/>.
*/

/* dev_cdrom.cpp : PicoMEM CD ROM device emulation
 Emulate a Panasonic, with 3MKE3 interface, and Sound Blaster ports : 
 https://en.wikipedia.org/wiki/Panasonic_CD_interface

*/

#if USE_CDROM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "pm_audio.h"
#include "pm_defines.h"
#include "dev_picomem_io.h"   // SetPortType / GetPortType
#include "dev_cdrom_mke.h"    // dev_cdr_* API + dev_cdr_eltorito_t

extern "C" void MKE_WRITE(uint16_t address, uint8_t value);
extern "C" uint8_t MKE_READ(uint16_t address);
extern "C" void mke_init();

#define CDROM_NUM 1
#include "cdrom/cdrom.h"
#include "cdrom/cdrom_image_manager.h"
// The single drive instance ("extern cdrom_t cdrom" in cdrom.h) shared with
// mke.c -- the MKE state machine addresses it directly.
cdrom_t cdrom;
dev_cdrom_mke_t dev_cdrom_mke = {false,0,0x250}; // (BasePort 250)

#if USE_CD_AUDIO_FIFO
static audio_fifo_t*  cd_fifo;
#endif

extern "C" {

uint8_t dev_cdr_install(uint16_t baseport)
{
 PM_INFO("[CDR] Install MKE CDR device (%x)\n",baseport);  
 if (!dev_cdrom_mke.active)   // Don't re enable if active
  {
   dev_cdrom_mke.baseport = baseport;

  if (GetPortType(dev_cdrom_mke.baseport)!=DEV_NULL)
     {
      PM_ERROR("[CDR] Port already used (%d)\n",GetPortType(dev_cdrom_mke.baseport));
      dev_cdrom_mke.active=false;
      return CMDERR_PORTUSED;
     }

   cdrom_global_init();   // clear the instance + error-string hookup
   mke_init();            // FIFO allocation + MKE state machine reset
   
#if USE_CD_AUDIO_FIFO
    cd_fifo = cdrom_audio_fifo_peek(&cdrom);  // Get pointer to the CD audio FIFO for the mixer
#endif

   SetPortType(dev_cdrom_mke.baseport,DEV_CDR,1);
   dev_cdrom_mke.active=true;
   PM_INFO("[CDR] dev_cdr_install: Success\n");
 } else {
  PM_INFO("[CDR] dev_cdr_install: Already active, port %x\n", dev_cdrom_mke.baseport); }
  return 0;
}

void dev_cdr_remove()
{
 if (dev_cdrom_mke.active)   // Don't stop if not active
  {
   dev_cdrom_mke.active=false;
   PM_INFO("Remove CD ROM (%X)\n",dev_cdrom_mke.baseport);

   DelPortType(DEV_CDR);
  }
}

// Return true if CMS is installed
bool dev_cdr_installed()
{
  return (GetPortType(dev_cdrom_mke.baseport)==DEV_CDR);
}

// Started in the Main Command Wait Loop
void dev_cdr_update()
{ // Perform file open, read ...
  if (dev_cdrom_mke.active) cdrom_tasks(&cdrom);
}

// Mount (or eject) the image named in PM_Config->CDR. The config stores the
// basename without extension (the BIOS/screenpad lists strip it); re-try
// .iso then .cue. The actual file open runs async in cdrom_tasks().
// Called after config apply and from CMD_CD_Mount (hot swap).
void dev_cdr_mount_from_config()
{
  char fname[14+4+1];
  if (!dev_cdrom_mke.active) {
    PM_INFO("[CDR] Mount skipped: dev_cdrom_mke.active=0\n");
    return;
  }
  // Point the image manager at the browsed sub-folder (CDRPath, relative to
  // 0:/CDROM) before listing/loading. Empty CDRPath resets to the root.
  cdman_set_subdir(PM_Config->CDRPath);
  if (PM_Config->CDR.name[0]==0)
    {
     PM_INFO("[CDR] Eject\n");
     cdman_unload_image(&cdrom);
     return;
    }
  snprintf(fname,sizeof(fname),"%s.iso",PM_Config->CDR.name);
  if (cdman_load_image_named(&cdrom,fname)!=0)
    {
     snprintf(fname,sizeof(fname),"%s.cue",PM_Config->CDR.name);
     if (cdman_load_image_named(&cdrom,fname)!=0)
       {
        PM_ERROR("[CDR] Image not found in 0:/CDROM: %s\n",PM_Config->CDR.name);
        return;
       }
    }
  PM_INFO("[CDR] Mount %s\n",fname);
}

#if USE_CD_AUDIO
// --- Red Book (CD-DA) audio pull path ---------------------------------
// Mix CD audio into one interleaved stereo int16 mixer buffer.
// Source: cdrom_audio_callback_simple() -- 44.1 kHz frames, reads audio
// sectors from the SD inline. Called from pm_audio_domix() on Core 0, so
// the SD access is serialized with the rest of the disk IO; one 2352-byte
// sector every ~13 ms of playback fits between I2S buffer deadlines.
// Upsampling 44100 -> out_rate by linear interpolation, 16.16 phase.
// Volume is already applied inside the callback (cd_audio_volume).
int16_t  cd_stage[SAMPLES_PER_SECTOR];  // one sector = 588 frames
uint32_t cd_stage_level = 0;            // int16 samples in stage
uint32_t cd_stage_rd    = 0;            // read index (int16 units)
uint32_t cd_phase       = 0;            // 16.16 pos between frames
int16_t  cd_curL, cd_curR, cd_nxtL, cd_nxtR;
bool     cd_primed      = false;


static bool cd_fetch_frame(int16_t *l, int16_t *r,bool UseSD)
{
  #if USE_CD_AUDIO_FIFO
    if (cd_fifo->state == FIFO_STATE_STOPPED || cd_fifo->write_idx == cd_fifo->read_idx) {
        return false;   // underrun / track end: rest of buffer stays as-is
    }
    sample_pair cd = cd_fifo->buffer[cd_fifo->read_idx & AUDIO_FIFO_BITS];
    cd_fifo->read_idx++;
    if (cd_fifo->write_idx == cd_fifo->read_idx) cd_fifo->state = FIFO_STATE_STOPPED;
    *l = cd.data16[0]/2;  // CD ROM is too loud, /2 for the moment
    *r = cd.data16[1]/2;
    return true;
  #else
    if (cd_stage_rd >= cd_stage_level) {
        cd_stage_rd    = 0;
        if (UseSD)
         {
          cd_stage_level = cdrom_audio_callback_simple(&cdrom, cd_stage,
                                                     SAMPLES_PER_SECTOR, false);
          if (cd_stage_level < 2) { cd_stage_level = 0; return false;}         
         } else { cd_stage_level =0;  return false; }  // track end / stop: rest of buffer stays as-is
       }
    *l = cd_stage[cd_stage_rd++]/2;  // CD ROM is too loud, /2 for the moment
    *r = cd_stage[cd_stage_rd++]/2;
    return true;
  #endif
}

const uint32_t step = (uint32_t)(((uint64_t)44100 << 16) / PM_AUDIO_FREQUENCY);

void dev_cdr_mixaudio(int16_t *buffer, uint32_t frames,bool UseSD)
{
  /*
  int mix_gate = (dev_cdrom_mke.active ? 1 : 0) |
           (cdrom.cd_status == CD_STATUS_PLAYING ? 2 : 0);

  if (mix_gate != 3) {
        cd_primed = false;
        cd_stage_level = cd_stage_rd = cd_phase = 0;
        return;
    }
*///   Mettre a jour cd_primed !!

    if (!cd_primed) {
        if (!cd_fetch_frame(&cd_curL, &cd_curR,UseSD)) return;
        if (!cd_fetch_frame(&cd_nxtL, &cd_nxtR,UseSD)) { cd_nxtL = cd_curL; cd_nxtR = cd_curR; }
        cd_phase  = 0;
        cd_primed = true;
    }
    for (uint32_t i = 0; i < frames; i++) {
        while (cd_phase >= 0x10000u) {
            cd_phase -= 0x10000u;
            cd_curL = cd_nxtL; cd_curR = cd_nxtR;
            if (!cd_fetch_frame(&cd_nxtL, &cd_nxtR,UseSD)) {
                cd_primed = false;   // track end / stop: rest of buffer stays as-is
                return;
            }
        }
        int32_t l = cd_curL + (((cd_nxtL - cd_curL) * (int32_t)cd_phase) >> 16);
        int32_t r = cd_curR + (((cd_nxtR - cd_curR) * (int32_t)cd_phase) >> 16);
        l += buffer[2*i];
        r += buffer[2*i+1];
        if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
        if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
        buffer[2*i]   = (int16_t)l;
        buffer[2*i+1] = (int16_t)r;
        cd_phase += step;
    }
}
#endif // USE_CD_AUDIO

// --- INT 13h EDD / El Torito support ------------------------------------
// Backend for the firmware-side INT 13h CD drive (bios_disk.cpp INT13_CD,
// drive 0xE0). Stateless reads only: never touches the MKE FIFOs or
// seek_pos, so it coexists with CDMKE / audio playback on the same disc.

bool dev_cdr_read2048(uint32_t lba, uint8_t *buf)
{
    if (!dev_cdrom_mke.active || !cdrom.disk_loaded || !cdrom.ops) return false;
    if (lba >= cdrom.cdrom_capacity) return false;
    return cdrom.ops->read_sector(&cdrom, CD_READ_DATA, buf, lba) != 0;
}

uint32_t dev_cdr_total_sectors(void)
{
    if (!dev_cdrom_mke.active || !cdrom.disk_loaded) return 0;
    return cdrom.cdrom_capacity;
}

// El Torito: ISO sector 17 = Boot Record Volume Descriptor ("CD001",
// "EL TORITO SPECIFICATION", catalog LBA at 0x47); catalog sector =
// validation entry (0x01 ... 0x55 0xAA) + default entry (0x88 bootable).
uint8_t dev_cdr_eltorito(dev_cdr_eltorito_t *et, uint8_t *scratch)
{
    if (!dev_cdrom_mke.active || !cdrom.disk_loaded) return 1;   // no media

    if (!dev_cdr_read2048(17, scratch)) return 1;
    if (scratch[0] != 0 || memcmp(&scratch[1], "CD001", 5) != 0 ||
        memcmp(&scratch[7], "EL TORITO SPECIFICATION", 23) != 0)
        return 2;                                          // not bootable
    uint32_t cat_lba = (uint32_t)scratch[0x47] | ((uint32_t)scratch[0x48] << 8) |
                       ((uint32_t)scratch[0x49] << 16) | ((uint32_t)scratch[0x4A] << 24);

    if (!dev_cdr_read2048(cat_lba, scratch)) return 2;
    // Validation entry
    if (scratch[0] != 0x01 || scratch[0x1E] != 0x55 || scratch[0x1F] != 0xAA)
        return 2;
    // Initial/default entry
    const uint8_t *e = &scratch[0x20];
    if (e[0] != 0x88) return 2;                            // not bootable
    et->media_type   = e[1] & 0x0F;
    et->load_seg     = (uint16_t)e[2] | ((uint16_t)e[3] << 8);
    if (et->load_seg == 0) et->load_seg = 0x7C0;
    et->sector_count = (uint16_t)e[6] | ((uint16_t)e[7] << 8);
    et->image_lba    = (uint32_t)e[8] | ((uint32_t)e[9] << 8) |
                       ((uint32_t)e[10] << 16) | ((uint32_t)e[11] << 24);
    et->catalog_lba  = cat_lba;
    return 0;
}

bool dev_cdr_ior(uint32_t Addr,uint8_t *Data )
{
  *Data=MKE_READ(Addr);
  return true;
}

void dev_cdr_iow(uint32_t Addr,uint8_t Data)
{
 MKE_WRITE(Addr, Data);
}

} // extern "C"

#endif