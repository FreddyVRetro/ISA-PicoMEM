#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/rawblock/core.h"
static struct pmb_media m;
static unsigned calls;
static int fail,detach;
static int read_sector(uint32_t lba,uint8_t *p)
{
 unsigned i;calls++;
 for(i=0;i<512;i++)p[i]=(uint8_t)(lba+i);
 if(detach)pmb_detach(&m);
 return !fail;
}
static uint8_t q[32],r[544];
static unsigned sequence;
static void command(unsigned op,uint32_t gen,uint32_t token,uint32_t lba,unsigned count)
{
 memset(q,0,sizeof(q));memcpy(q,"PMB1",4);q[4]=1;q[5]=op;
 PMB_PUT32(q+8,++sequence);PMB_PUT32(q+12,gen);PMB_PUT32(q+16,token);
 PMB_PUT32(q+20,lba);PMB_PUT16(q+24,count);
}
static void run(unsigned expected)
{
 pmb_execute(&m,q,r,read_sector);
 assert(!memcmp(r,"PMB1",4));assert(r[6]==expected);
 assert(PMB_GET32(r+8)==sequence);
 if(expected)for(unsigned i=32;i<544;i++)assert(r[i]==0);
}
int main(void)
{
 uint32_t gen,tok;
 command(PMB_INFO,0,0,0,0);run(PMB_OK);assert(!PMB_GET32(r+20));
 command(PMB_CLAIM,1,0,0,0);run(PMB_NO_MEDIA);
 pmb_attach(&m,0xffffffffu,512,0x0951,0x1602);gen=m.generation;
 command(PMB_CLAIM,gen+1,0,0,0);run(PMB_STALE);
 command(PMB_READ,gen,0,0,1);run(PMB_NOT_OWNER);
 command(PMB_CLAIM,gen,0,0,0);run(PMB_OK);tok=m.token;assert(tok);
 command(PMB_CLAIM,gen,0,0,0);run(PMB_NOT_OWNER);
 command(PMB_READ,gen,tok,0xfffffffeu,1);run(PMB_OK);assert(calls==1);
 for(unsigned i=0;i<512;i++)assert(r[32+i]==(uint8_t)(0xfffffffeu+i));
 command(PMB_READ,gen,tok,0xffffffffu,1);run(PMB_RANGE);
 command(PMB_READ,gen,tok,0,2);run(PMB_RANGE);
 command(PMB_READ,gen,tok,0,0);run(PMB_RANGE);
 for(unsigned op=5;op<256;op++){command(op,gen,tok,0,0);run(PMB_UNSUPPORTED);}
 command(PMB_READ,gen,tok,0,1);q[26]=1;run(PMB_BAD_REQUEST);
 command(PMB_READ,gen,tok,0,1);q[4]=2;run(PMB_BAD_REQUEST);
 command(PMB_READ,gen,tok,0,1);q[0]=0;run(PMB_BAD_REQUEST);
 command(PMB_RELEASE,gen,tok,0,0);run(PMB_OK);assert(!m.token);
 command(PMB_READ,gen,tok,0,1);run(PMB_NOT_OWNER);assert(calls==1);
 pmb_detach(&m);pmb_attach(&m,8,4096,1,2);
 command(PMB_CLAIM,m.generation,0,0,0);run(PMB_UNSUPPORTED);
 pmb_detach(&m);pmb_attach(&m,8,512,1,2);gen=m.generation;
 command(PMB_CLAIM,gen,0,0,0);run(PMB_OK);tok=m.token;
 fail=1;command(PMB_READ,gen,tok,0,1);run(PMB_IO);assert(m.fault);
 fail=0;command(PMB_READ,gen,tok,0,1);run(PMB_FAULT);assert(calls==2);
 pmb_detach(&m);pmb_attach(&m,8,512,1,2);
 command(PMB_CLAIM,m.generation,0,0,0);run(PMB_FAULT);
 memset(&m,0,sizeof(m));pmb_attach(&m,8,512,1,2);gen=m.generation;
 command(PMB_CLAIM,gen,0,0,0);run(PMB_OK);tok=m.token;
 detach=1;command(PMB_READ,gen,tok,1,1);run(PMB_IO);assert(m.fault);
 assert(calls==3);
 puts("PASS core: ownership, stale media, high LBA, malformed requests, write rejection, failed/removed reads, sticky fault");
}
