/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <string.h>
#include "core.h"
static void generation_next(struct pmb_media *m)
{
 if (++m->generation == 0) ++m->generation;
 m->token=0;
}
void pmb_attach(struct pmb_media *m, uint32_t capacity, uint16_t size,
                uint16_t vid, uint16_t pid)
{
 generation_next(m); m->capacity=capacity; m->sector_size=size;
 m->vid=vid; m->pid=pid; m->present=1;
 /* Fault is deliberately not cleared by detach/reattach or a new claim. */
}
void pmb_detach(struct pmb_media *m)
{
 generation_next(m); m->present=0; m->capacity=0;
}
void pmb_execute(struct pmb_media *m, const uint8_t *q, uint8_t *r, pmb_reader read)
{
 unsigned i, status=PMB_OK, op=q[5];
 uint32_t seq=PMB_GET32(q+8), gen=PMB_GET32(q+12);
 uint32_t token=PMB_GET32(q+16), lba=PMB_GET32(q+20);
 memset(r,0,PMB_FRAME);
 if (memcmp(q,"PMB1",4) || q[4]!=PMB_VERSION || q[6] || q[7] || !seq)
  status=PMB_BAD_REQUEST;
 for (i=26;i<PMB_HEADER;i++) if (q[i]) status=PMB_BAD_REQUEST;
 if (op!=PMB_READ && (lba || PMB_GET16(q+24))) status=PMB_BAD_REQUEST;
 if (!status && op==PMB_INFO) {
  if (gen || token) status=PMB_BAD_REQUEST;
 } else if (!status) {
  if (op<PMB_CLAIM || op>PMB_RELEASE) status=PMB_UNSUPPORTED;
  else if (m->fault) status=PMB_FAULT;
  else if (!m->present) status=PMB_NO_MEDIA;
  else if (!gen || gen!=m->generation) status=PMB_STALE;
  else if (!m->capacity || m->sector_size!=PMB_SECTOR) status=PMB_UNSUPPORTED;
  else if (op==PMB_CLAIM) {
   if (token || m->token) status=PMB_NOT_OWNER;
   else m->token=seq;
  } else if (!token || token!=m->token) status=PMB_NOT_OWNER;
  else if (op==PMB_RELEASE) m->token=0;
  else if (PMB_GET16(q+24)!=1 || lba>=m->capacity) status=PMB_RANGE;
  else {
   int ok=read(lba,r+PMB_HEADER);
   if (!ok || !m->present || m->generation!=gen || m->token!=token) {
    memset(r+PMB_HEADER,0,PMB_SECTOR);
    m->fault=1; status=PMB_IO;
   }
  }
 }
 memcpy(r,"PMB1",4); r[4]=PMB_VERSION; r[5]=op; r[6]=status;
 r[7]=PMB_FLAGS;
 PMB_PUT32(r+8,seq); PMB_PUT32(r+12,m->generation);
 PMB_PUT32(r+16,m->token); PMB_PUT32(r+20,m->capacity);
 PMB_PUT16(r+24,m->sector_size);
 PMB_PUT16(r+26,(!status && op==PMB_READ)?1:0);
 PMB_PUT16(r+28,m->vid); PMB_PUT16(r+30,m->pid);
}
