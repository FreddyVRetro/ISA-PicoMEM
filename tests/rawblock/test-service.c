#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/rawblock/service.c"
static unsigned reads,mode;
static uint32_t clock_us;
static void *pending_buffer;
static tuh_msc_complete_cb_t callback;
static uintptr_t callback_arg;
uint32_t time_us_32(void){clock_us+=100000;return clock_us;}
bool tuh_msc_mounted(uint8_t a){return a==dev;}
void tuh_vid_pid_get(uint8_t a,uint16_t *v,uint16_t *p){(void)a;*v=1;*p=2;}
uint32_t tuh_msc_get_block_count(uint8_t a,uint8_t l){(void)a;(void)l;return 100;}
uint32_t tuh_msc_get_block_size(uint8_t a,uint8_t l){(void)a;(void)l;return 512;}
bool tuh_msc_read10(uint8_t a,uint8_t l,void *b,uint32_t s,uint16_t n,tuh_msc_complete_cb_t c,uintptr_t arg)
{assert(a==1 && l==0 && s<100 && n==1);reads++;pending_buffer=b;callback=c;callback_arg=arg;return mode!=7;}
void tuh_task(void)
{
 msc_cbw_t cbw={.total_bytes=512,.tag=42,.command={0x28}};
 msc_csw_t csw={.signature=MSC_CSW_SIGNATURE,.tag=42};
 tuh_msc_complete_data_t c={&cbw,&csw,callback_arg,true};
 if(mode==6)return; /* missing completion -> timeout */
 if(mode==1)c.raw_read_complete=false;
 if(mode==2)csw.tag=43;
 if(mode==3)csw.data_residue=1;
 if(mode==4)csw.status=1;
 if(mode==5)csw.signature=0;
 memset(pending_buffer,0x6a,512);
 callback(1,&c);
}
static void issue(unsigned op,unsigned gen,unsigned tok,unsigned lba,unsigned count,uint8_t *r)
{
 static unsigned seq;uint8_t q[32]={0};unsigned i;
 memcpy(q,"PMB1",4);q[4]=1;q[5]=op;PMB_PUT32(q+8,++seq);
 PMB_PUT32(q+12,gen);PMB_PUT32(q+16,tok);PMB_PUT32(q+20,lba);PMB_PUT16(q+24,count);
 pm_raw_iow(5,PMB_BEGIN);assert(pm_raw_ior(5)==PMB_RX);
 for(i=0;i<32;i++)pm_raw_iow(7,q[i]);
 pm_raw_iow(5,PMB_SUBMIT);assert(pm_raw_ior(5)==PMB_BUSY);
 /* No client reset or data write may steal a pending mailbox. */
 pm_raw_iow(5,PMB_BEGIN);pm_raw_iow(5,PMB_ACK);pm_raw_iow(7,0xff);
 assert(pm_raw_ior(5)==PMB_BUSY);
 pm_raw_task();assert(pm_raw_ior(5)==PMB_DONE);
 for(i=0;i<544;i++)r[i]=pm_raw_ior(7);
 assert(pm_raw_ior(7)==0xff);
 pm_raw_iow(5,PMB_ACK);assert(pm_raw_ior(5)==PMB_IDLE);
}
int main(void)
{
 uint8_t r[544];unsigned gen,tok,i;
 assert(pm_raw_ior(6)==PMB_IDENT);
 for(mode=0;mode<=7;mode++) {
  memset(&media,0,sizeof(media));dev=0;transfer_pending=0;
  tuh_msc_mount_cb(1);gen=media.generation;
  tuh_msc_mount_cb(2);assert(dev==1); /* first medium stays selected */
  issue(PMB_CLAIM,gen,0,0,0,r);assert(r[6]==0);tok=media.token;
  issue(PMB_READ,gen,tok,3,1,r);
  assert(r[6]==(mode?PMB_IO:PMB_OK));
  for(i=32;i<544;i++)assert(r[i]==(mode?0:0x6a));
  if(mode){unsigned before=reads;issue(PMB_READ,gen,tok,3,1,r);assert(r[6]==PMB_FAULT);assert(reads==before);}
 }
 assert(usb_disk_read(r,0,1)==RES_NOTRDY);
 assert(usb_disk_write(r,0,1)==RES_WRPRT);
 assert(usb_disk_ioctl(0,r)==RES_NOTRDY);
 assert(usb_disk_status() & STA_NODISK);
 puts("PASS service: real mailbox, successful sector, malformed/short/failed CSW, timeout, rejected submission, no firmware FAT access");
}
