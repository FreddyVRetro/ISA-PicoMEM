/* Exercise the actual pinned TinyUSB MSC state machine, not a reimplementation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/lib/tinyusb/src/class/msc/msc_host.c"
static unsigned callbacks;static bool evidence;static unsigned char data[512];
bool usbh_edpt_xfer_with_callback(uint8_t d,uint8_t ep,uint8_t *b,uint16_t n,tuh_xfer_cb_t cb,uintptr_t u)
{(void)d;(void)ep;(void)b;(void)n;(void)cb;(void)u;return true;}
bool usbh_edpt_claim(uint8_t d,uint8_t ep){(void)d;(void)ep;return true;}
bool usbh_edpt_release(uint8_t d,uint8_t ep){(void)d;(void)ep;return true;}
static bool complete_test(uint8_t d,const tuh_msc_complete_data_t *c)
{assert(d==1);callbacks++;evidence=c->raw_read_complete;return true;}
static void setup(void)
{
 msch_interface_t *m;msch_epbuf_t *b;
 msch_init();m=get_itf(1);b=get_epbuf(1);
 m->configured=m->mounted=true;m->ep_in=0x81;m->ep_out=2;
 m->stage=MSC_STAGE_DATA;m->complete_cb=complete_test;m->buffer=data;
 memset(b,0,sizeof(*b));b->cbw.command[0]=SCSI_CMD_READ_10;
 b->cbw.total_bytes=512;b->cbw.dir=0x80;
 callbacks=0;evidence=false;
}
int main(void)
{
 unsigned n,e,tests=0;
 for(n=0;n<=513;n++) {
  setup();assert(msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,n));
  assert(msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,13));
  assert(callbacks==1 && evidence==(n==512));tests++;
 }
 for(n=0;n<=14;n++) {
  setup();assert(msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,512));
  assert(msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,n));
  assert(callbacks==1 && evidence==(n==13));tests++;
 }
 for(e=XFER_RESULT_FAILED;e<=XFER_RESULT_INVALID;e++) {
  setup();msch_xfer_cb(1,0x81,(xfer_result_t)e,512);
  msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,13);assert(!evidence);tests++;
  setup();msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,512);
  msch_xfer_cb(1,0x81,(xfer_result_t)e,13);assert(!evidence);tests++;
 }
 setup();msch_xfer_cb(1,2,XFER_RESULT_SUCCESS,512);
 msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,13);assert(!evidence);tests++;
 setup();msch_xfer_cb(1,0x81,XFER_RESULT_SUCCESS,512);
 msch_xfer_cb(1,2,XFER_RESULT_SUCCESS,13);assert(!evidence);tests++;
 printf("PASS actual TinyUSB MSC: %u DATA/status length, endpoint and result cases\n",tests);
}
