/* SPDX-License-Identifier: GPL-2.0-or-later
 * Exclusive raw USB build: USB is never registered with FatFs.
 * SD, BIOS image I/O and their shared command memory are independent.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "class/msc/msc_host.h"
#include "ff.h"
#include "diskio.h"
#include "core.h"
#include "service.h"
static struct pmb_media media;
static volatile uint8_t state=PMB_IDLE;
static volatile unsigned cursor;
static uint8_t request[PMB_HEADER], reply[PMB_FRAME];
static uint8_t dev;
static volatile uint8_t transfer_pending, transfer_ok;
static uint32_t transfer_generation, transfer_tag;
static uint8_t sector[PMB_SECTOR] __attribute__((aligned(4)));

uint8_t pm_raw_ior(unsigned port)
{
 if (port==PMB_IDENT_PORT) return PMB_IDENT;
 if (port==PMB_STATUS_PORT) return state;
 if (port==PMB_DATA_PORT && state==PMB_DONE && cursor<PMB_FRAME) {
  __dmb(); return reply[cursor++];
 }
 return 0xff;
}
void pm_raw_iow(unsigned port,uint8_t value)
{
 if (port==PMB_DATA_PORT && state==PMB_RX && cursor<PMB_HEADER) {
  request[cursor++]=value; return;
 }
 if (port!=PMB_STATUS_PORT) return;
 if (value==PMB_BEGIN && state==PMB_IDLE) { cursor=0; state=PMB_RX; }
 else if (value==PMB_SUBMIT && state==PMB_RX && cursor==PMB_HEADER) {
  __dmb(); state=PMB_BUSY;
 } else if (value==PMB_ACK && state==PMB_DONE) {
  cursor=0; __dmb(); state=PMB_IDLE;
 }
}
static bool complete(uint8_t addr, tuh_msc_complete_data_t const *c)
{
 if (!transfer_pending || addr!=dev || media.generation!=transfer_generation ||
     c->user_arg!=transfer_tag) return false;
 transfer_ok = c->raw_read_complete && c->cbw && c->csw && c->cbw->total_bytes==PMB_SECTOR &&
     c->cbw->command[0]==SCSI_CMD_READ_10 && c->csw->signature==MSC_CSW_SIGNATURE &&
     c->csw->tag==c->cbw->tag && c->csw->data_residue==0 && c->csw->status==0;
 transfer_pending=0;
 return true;
}
static int read_sector(uint32_t lba,uint8_t *out)
{
 uint32_t deadline=time_us_32()+2000000u;
 transfer_generation=media.generation;
 if (++transfer_tag==0) ++transfer_tag;
 transfer_ok=0; transfer_pending=1;
 memset(sector,0,sizeof(sector));
 if (!dev || !tuh_msc_mounted(dev) ||
     !tuh_msc_read10(dev,0,sector,lba,1,complete,transfer_tag)) {
  transfer_pending=0; return 0;
 }
 while (transfer_pending) {
  tuh_task();
  if ((int32_t)(time_us_32()-deadline)>=0) {
   /* No reuse of sector buffer after an uncertain transfer; fault is sticky. */
   transfer_pending=0; return 0;
  }
 }
 if (!transfer_ok || transfer_generation!=media.generation) return 0;
 memcpy(out,sector,sizeof(sector)); return 1;
}
void pm_raw_task(void)
{
 if (state!=PMB_BUSY) return;
 __dmb();
 pmb_execute(&media,request,reply,read_sector);
 cursor=0; __dmb(); state=PMB_DONE;
}
void tuh_msc_mount_cb(uint8_t addr)
{
 uint16_t vid=0,pid=0;
 if (dev || transfer_pending || media.fault) return;
 tuh_vid_pid_get(addr,&vid,&pid);
 dev=addr;
 pmb_attach(&media,tuh_msc_get_block_count(addr,0),
            tuh_msc_get_block_size(addr,0),vid,pid);
}
void tuh_msc_umount_cb(uint8_t addr)
{
 if (addr!=dev) return;
 pmb_detach(&media);dev=0;
 if (transfer_pending) { transfer_ok=0; transfer_pending=0; media.fault=1; }
}
/* Defense in depth: no firmware file or BIOS-image path can reach raw USB. */
DSTATUS usb_disk_status(void) { return STA_NOINIT|STA_NODISK|STA_PROTECT; }
DSTATUS usb_disk_initialize(void) { return usb_disk_status(); }
DRESULT usb_disk_read(BYTE *b,LBA_t s,UINT n) { (void)b;(void)s;(void)n;return RES_NOTRDY; }
DRESULT usb_disk_write(const BYTE *b,LBA_t s,UINT n) { (void)b;(void)s;(void)n;return RES_WRPRT; }
DRESULT usb_disk_ioctl(BYTE c,void *b) { (void)c;(void)b;return RES_NOTRDY; }
