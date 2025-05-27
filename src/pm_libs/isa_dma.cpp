// Code to support the Hardware and emulated DMA
#include <stdio.h>
#include "pico/stdlib.h"
#include "../pm_debug.h"
#include "../pm_gvars.h"
#include "../pm_defines.h"
#include "../pm_pccmd.h"
#include "dev_memory.h"
#include "isa_dma.h"
#include "isa_irq.h"

// DMA transfer will be done through a buffer in the BIOS RAM (last 2Kb)

pm_dma_t isa_dma;

void isa_dma_init()
{
  isa_dma.inprogress=false;
  isa_dma.transfer_remain=0;  
}

bool isa_dma_inprogress()
{
 return false;
}

uint16_t dma_fifo_remain();

// Perform a DMA transfer with a pre defined length
// return the number of bytes transfered
// Data stored to a 2KB fifo. No transfer when full
uint16_t isa_dma_transfer_fifo()
{

// Commands to Start/Stop/Continue the transfer
  switch(isa_dma.cmd)
   {
    case DMA_CMD_START:  // New Transfer may stop the actual running transfer (But don't clean the FIFO)
         PM_INFO("DMA>START\n");
         isa_dma.copy_cnt=0;              // count the NB of transfer done (Debug)
         isa_dma.transfer_remain=isa_dma.transfer_size;
         isa_dma.cmd=DMA_CMD_NONE;
//         isa_dma.fifo_end=PM_DMAB_Size;  // set initial DMA FIFO End
         isa_dma.inprogress=isa_dma.channel;
         break;
    case DMA_CMD_CONTINUE:    // Continue the transfer when the previous one is completed.
         if (isa_dma.transfer_remain==0)
            {
              DBG_ON_1();
            // PM_INFO("DMA>CONTINUE\n");

             isa_dma.transfer_remain=isa_dma.transfer_size;
             isa_dma.cmd=DMA_CMD_NONE;
             isa_dma.inprogress=isa_dma.channel;
             PM_INFO("DC>%d",isa_dma.transfer_remain);         
            }
         break;
    case DMA_CMD_STOP:       // Stop/Pause the current transfer
         PM_INFO("DMA>STOP\n");
         isa_dma.cmd=DMA_CMD_NONE;
         isa_dma.inprogress=0;
         break; 
    case DMA_CMD_RESET:       // Stop/Pause the current transfer
         PM_INFO("DMA>RESET\n");
         isa_dma.cmd=DMA_CMD_NONE;
         isa_dma.inprogress=0;
         isa_dma.transfer_remain=0;
         isa_dma.fifo_head=0;
         isa_dma.fifo_tail=0;
         break;      
   }

// Need to transfer something ?
  if (!isa_dma.inprogress) return 0;

  uint8_t channel=isa_dma.inprogress;
  
// Update the DMA channel values if changed (By the PC)
  if (dmachregs[channel].changed)  // !! Risk if only one change is captured when arriving here, Nobody should do that
    {
      dmachregs[channel].changed=false;
      PM_INFO("DMA %d REG Changed >",channel);
 //     PM_INFO("Virtual DMA: Page:%X Offset: %X Size: %d\n",dmachregs[channel].page, dmachregs[channel].baseaddr, dmachregs[channel].basecnt);
      isa_dma.dma_address=((uint32_t) dmachregs[channel].page<<16)+dmachregs[channel].baseaddr;
      isa_dma.dma_c_adress=isa_dma.dma_address;
      isa_dma.dma_size=dmachregs[channel].basecnt+1;  // DMA register is the size -1
      isa_dma.dma_remain=isa_dma.dma_size;            // Bytes remaining in the currect DMA buffer
 //     PM_INFO("DMA Addr:%x Size:%d\n",isa_dma.dma_c_adress,isa_dma.dma_size);
     }

  uint32_t FIFO_Addr=((uint32_t) PM_BIOSADDRESS<<4)+16*1024+PM_DMAB_Offset; // FIFO is in the PC RAM

// Hoy many bytes need to be transfered, and is there enaugh space ?
  uint16_t to_transfer;
  uint16_t transfered;  
  uint16_t tr_offset;
  to_transfer= (isa_dma.transfer_remain<isa_dma.sizept) ? isa_dma.transfer_remain : isa_dma.sizept;
// PM_INFO("TC: %d (before move)  Tail: %d Head: %d\n",isa_dma.copy_cnt,isa_dma.fifo_tail,isa_dma.fifo_head);

 // Verify if there is enaugh space to copy "to_transfer" bytes
 if (isa_dma.fifo_tail>isa_dma.fifo_head)
     { // Tail in advance, Free space is tail-head
      if ((isa_dma.fifo_tail-isa_dma.fifo_head)<=to_transfer)
         {
 //         PM_INFO("DMA FIFO Full ! (tail>head) %d>%d\n",isa_dma.fifo_tail,isa_dma.fifo_head);  
// DBG_ON_1();       
          return 0;
         }
     } // Tail before head : Check if not enaugh space after the head, or before the tail
    else if (((PM_DMAB_Size-isa_dma.fifo_head)<to_transfer)&&(isa_dma.fifo_tail<=to_transfer)) // <= ??
     {
 //     PM_INFO("DMA FIFO Full ! (tail<=head) %d<%d\n",isa_dma.fifo_tail,isa_dma.fifo_head);
 // DBG_ON_1();   
      return 0;
     } 

// FIFO : Loop if not enaugh space to transfer at the end  
  if ((isa_dma.fifo_head+to_transfer)>PM_DMAB_Size)
    {
      // Go back to the FIFO begining, place the fifo end to the current head
      isa_dma.fifo_end=isa_dma.fifo_head; // Set new DMA Buffer end
      isa_dma.fifo_head=0;
//      PM_INFO("FIFO Go back\n");
    }

  tr_offset = isa_dma.fifo_head;
  transfered= to_transfer;

//  PM_INFO("TC: %d (after move)  Tail: %d Head: %d\n",isa_dma.copy_cnt,isa_dma.fifo_tail,isa_dma.fifo_head);
//  PM_INFO("TC: %d Rem:%d DAM Rem: %d Off: %d End: %d\n",isa_dma.copy_cnt,isa_dma.transfer_remain,isa_dma.dma_remain,tr_offset,isa_dma.fifo_end);

  if (!PC_Wait_End_Timeout(10000))
  { 
    PM_INFO("DMA> TIMEOUT, PCCR_PCSTATE!=0 (%x)\n",PCCR_PCSTATE);
    if (PCCR_PCSTATE==PCC_PCS_WAITCMD) PM_INFO("CMD in Progress");
    isa_irq_lower();
    return 0;
   }

// Ask the PC to Start the commands (Via an IRQ)
  isa_irq_raise(IRQ_StartPCCMD,0,true);  

  if (!PC_Wait_Ready_Timeout(20000))  // 20ms timeout
     { 
      PM_INFO("DMA> TIMEOUT, NO IRQ ?\n");
      isa_irq_lower();
      return 0;
     }

  if (to_transfer>isa_dma.dma_remain)
     {  // There is less data remaining in the DMA buffer that what is asked.
//      PM_INFO("TC: %d Copy %X>%X %d (DMA Loop)\n",isa_dma.copy_cnt, isa_dma.dma_c_adress, FIFO_Addr+tr_offset,isa_dma.dma_remain);
      PC_MemCopy(isa_dma.dma_c_adress, FIFO_Addr+tr_offset, isa_dma.dma_remain);
      // Update the FIFO transfer variables
      to_transfer-=isa_dma.dma_remain;
      tr_offset+=isa_dma.dma_remain;
      isa_dma.fifo_head+=isa_dma.dma_remain;

      // Loop to the begining of the DMA buffer (DMA Autoinit "simulation")  !! Must be in autoinit mode
      isa_dma.dma_c_adress=isa_dma.dma_address;
      isa_dma.dma_remain=isa_dma.dma_size;
     }
 
//  PM_INFO("TC: %d Copy %X>%X %d\n",isa_dma.copy_cnt,isa_dma.dma_c_adress, FIFO_Addr+tr_offset,to_transfer);
  PC_MemCopy(isa_dma.dma_c_adress, FIFO_Addr+tr_offset, to_transfer);

  // Update the DMA registers
  PC_SetDMARegs(channel, (uint16_t) (isa_dma.dma_c_adress+to_transfer), isa_dma.dma_remain-to_transfer);

  PC_End();     // Tell the PC Command loop to end  
  isa_irq_lower();

  
  if (!PC_Wait_End_Timeout(10000))
  { 
    PM_INFO("DMA> TIMEOUT, PCCR_PCSTATE!=0 (%x) BV_IRQStatus %x\n",PCCR_PCSTATE,BV_IRQStatus);
    isa_irq_lower();
    return 0;
  }

/**/  //> Move to SB interrupt code ?
   if (!isa_irq_Wait_End_Timeout(10000))
   { 
     PM_INFO("DMA> TIMEOUT, PIRQ still in progress BV_IRQStatus %x\n",BV_IRQStatus);
   }   

/*
  PM_INFO("\n");
  for (int i=0;i<32;i++)
      PM_INFO("%X ",PM_DP_RAM[PM_DMAB_Offset+i]);
*/
  // Update the FIFO variables
  isa_dma.transfer_remain-=to_transfer;
  tr_offset+=to_transfer;
  isa_dma.fifo_head+=to_transfer;
  // Update the DMA buffer variables
  isa_dma.dma_c_adress+=to_transfer;
  isa_dma.dma_remain-=to_transfer;

  isa_dma.copy_cnt++;  // Count the Nb of transfer done (For Debug)  

  if (isa_dma.transfer_remain==0) 
     {
      isa_dma.inprogress=0;
      PM_INFO("D>E");
     }
  //PM_INFO("R>%d",isa_dma.transfer_remain);

  if (isa_dma.fifo_head==isa_dma.fifo_tail) // Should never occur (For debug)
    {
      PM_INFO("DMA> ERROR ! HEAD=TAIL");
    }

  return transfered;
}

/*
bool isa_dma_transfer_completed
{
 return true;
}
 */