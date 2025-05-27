// RP2350 ISA Bus Code

//***********************************************************************************************//
//*         CORE 1 Main : Wait for the ISA Bus events, emulate RAM and ROM                      *//
//***********************************************************************************************//
// rp2040 optimisations : 
// Memory read Address read from PIO SM is done with a fixed delay (NOP) to gain time.
// RP2040 Can't really be overclocked more than 300MHz so pio_sm_get_blocking is not needed.
// The code is placed in the Scratch_x (4k Bank used for the Core 1 Stack) to avoid RAM Access conflict.

//void __time_critical_func(main_core1)(void)
void __scratch_x("core1_ISA_code") (main_core1)(void)
{

PM_INFO("Core1: pm_isa_rp2350_v2_dev\n");

// PSRAM 4 Bytes cache
uint32_t  PSRC_Addr;
uint8_t   PSRC_Data[4];

uint32_t  ISA_Addr;
uint8_t   ISA_Data;

// Send the value to use to return the fake Address (Address where the is a ROM or no IO)
// pio_sm_put(isa_pio, isa_bus_sm, 0x0FFFFF0F0);  // If 0x0FFFFF0FF, Floppy access (DMA) Crash

for (;;) {
#if TIMING_DEBUG   
//  gpio_put(PIN_IRQ,0);  // IRQ Up
#endif

 ISA_WasRead = false;

// 2 Values returned by the Assembly code
  static uint32_t Ctrl_m=0x0F0000;   // Ctrl signals at Bit 16 to 19 are active high (after invertion)
  static uint32_t addr_m=0x000F00;   //
  uint32_t IO_CTRL_MDIndex;          // Control for IO or Dev Type for MEM, No need for uint8_t

 // ** Wait until a Control signal is detected **
 // ** Then, jump to MEMR / MEMW / IO and decode the Memory Address and Device Index
 asm volatile goto (
     "DetectCtrlEndLoop:             \n\t"  // 1) Wait until there is no more Control signal (We detect an edge)
     "ldr %[DEV_T],[%[IOB],%[IOO]]   \n\t"  // Load the GPIO Values 
     "tst %[DEV_T],%[CM]             \n\t"  // Apply the mask to keep the ISA Control Signals Only
     "bne DetectCtrlEndLoop          \n\t"  // Loop if Control Signal detected (!=0)

     "DetectCtrlLoop:                \n\t"  // 2) Wait until a Control Signal is present
     "ldr %[DEV_T],[%[IOB],%[IOO]]   \n\t"  // Load the GPIO Values
     "tst %[DEV_T],%[CM]             \n\t"  // Apply the mask to keep the ISA Control Signals Only
     "beq DetectCtrlLoop             \n\t"  // Loop if no Control signal detected
     "str %[DEV_T],[%[PIOB],%[PIOT]] \n\t"  // pio->txf[0] = CTRL_AL8; Start the Cycle

// %[DEV_T] contains A12 to A19, then MR, MW, IOR, IOW from the Bit 8 
     "lsl %[DEV_T],%[DEV_T],#12      \n\t" // ISA_CTRL=ISA_CTRL<<12   CAAxxxxx
     "lsr %[DEV_T],%[DEV_T],#28      \n\t" // ISA_CTRL=ISA_CTRL>>28   -> ISA_CTRL is now correct
//     "sbfx %[DEV_T],%[DEV_T],#16,#8  \n\t" // Get 8bit from the bit 16

     "cmp %[DEV_T],#1                \n\t"  // Is it a MEM Read ?
     "beq  ISA_Do_MEMR               \n\t"  // Jump must be <200 bytes

     "cmp %[DEV_T],#2                \n\t"  // Is it a MEM Read ?
     "beq  ISA_Do_MEMW               \n\t"  // Jump must be <200 bytes

//** > ISA IOR / IOW   **

     "b %l[ISA_Do_IO]                \n\t"  // If %[TMP]!=0 Do IO    Jump must be <200 bytes

     "ISA_Do_MEMW:                   \n\t"   // Minimum 4 NOP (20/05)  5 at 300MHz on the PM 1.0
     "b %l[ISA_Do_MEMW]              \n\t"   //

     "ISA_Do_MEMR:                   \n\t"   // Minimum 4 NOP (20/05)  5 at 300MHz on the PM 1.0
/*
     "nop                            \n\t"   // Added to wait until the PIO Send the Address Value  
     "nop                            \n\t"

// Directly read the RX pipe : Be carefull about the timing
     "ldr %[ADDR],[%[PIOB],%[PIOR]]  \n\t"   // ISA_Addr = pio->rxf[sm]; Read ISA_Addr from the PIO (Address with Mask) 
*/

// Define Outputs
      :[DEV_T]"+l" (IO_CTRL_MDIndex),      // IO_CTRL_MDIndex is for MEM, use it as ISA_CTRL for I/O and used as Temp register
       [ADDR]"+l" (ISA_Addr)
// Define Inputs
     :[CM]"l" (Ctrl_m),
      [MIT]"l" (MEM_Index_T),          // l for low register     
      [IOB]"l" (SIO_BASE),             // IO Registers Base Address (l for low register)
      [IOO]"i" (SIO_GPIO_IN_OFFSET),   // Integer, GPIO In Offset (#4)
      [PIOB]"l"(isa_pio),
      [PIOT]"i" (PIO_TXF0_OFFSET),
      [PIOR]"i" (PIO_RXF0_OFFSET)
     :"cc"                              // Tell the compiler that regs are changed
     : ISA_Do_IO,
       ISA_Do_MEMW
      );

  ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm);

  IO_CTRL_MDIndex=MEM_Index_T[ISA_Addr>>13];
  pio_sm_put(isa_pio, isa_bus_sm, IO_CTRL_MDIndex>>3);  // 1st Write : No Wait State
  ISA_Addr=ISA_Addr^addr_m;

   if ((IO_CTRL_MDIndex>0)&&(IO_CTRL_MDIndex<MI_R_WS_END))    // 0<Index<MI_R_WS_END : "Fast RAM"
        { // ** Read from the Pico Memory for "Fast" RAM or ROM
        pm_do_memr(RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]);
        continue;  // Go back to the main loop begining
        }
   else if (IO_CTRL_MDIndex>=16)                              // Index >=16 : PSRAM (RAM or EMS)
        { // ** Read from QSPI PSRAM
        pm_do_memr(RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]);
        continue;  // Go back to the main loop begining
        }
 
   pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   continue;  // Go back to the main loop begining

/*
  if ((ISA_Addr>>16)==0x0D)
   {
    To_Display32_2=IO_CTRL_MDIndex;
    ISA_Addr = ISA_Addr ^ addr_m;
    To_Display32=ISA_Addr;
   }

  continue;  // Go back to the main loop begining
*/

// ********** ISA_Cycle=Memory Write (0010b) **********
ISA_Do_MEMW:

   ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm);  // Read the Address

   IO_CTRL_MDIndex = MEM_Index_T[ISA_Addr>>13];
   pio_sm_put(isa_pio, isa_bus_sm, IO_CTRL_MDIndex>>3);  // 1st Write : Add Wait state if needed
   ISA_Addr=ISA_Addr^addr_m;

   pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)

// ** Write to the PicoMEM Internal RAM
   if ((IO_CTRL_MDIndex>0)&&(IO_CTRL_MDIndex<MI_W_NOWS_END))  // Memory in the Pico Memory or ROM
         {
          uint32_t ISAMW_Data;
          ISAMW_Data=((uint32_t)gpio_get_all()>>PIN_AD0);                
          RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]=(uint8_t) ISAMW_Data;
         }
   else if (IO_CTRL_MDIndex>=16)                      // Index >=16 : PSRAM (RAM or EMS)
        {   //!! Be carefull when optimizing / Overclocking 
          uint32_t ISAMW_Data; 
//          asm volatile ("nop");
//          asm volatile ("nop");
          ISAMW_Data=((uint32_t)gpio_get_all()>>PIN_AD0);                
          RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]=(uint8_t) ISAMW_Data;
        }
   else {}  // To add : RAM Capture with PSRAM (With pimoroni)

continue;  // Go back to the main loop begining

//*************************************************
//***         The ISA Cycle is IO               ***
//*************************************************
ISA_Do_IO:  // Goto, from the assembly code
  
  uint32_t ISA_IO_Addr;
  uint32_t IO_Device;

  ISA_IO_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm) & 0x3FF; // No need for Mask (Only the low 8Bit are used)

  IO_Device = PORT_Table[ISA_Addr>>3];
  pio_sm_put(isa_pio, isa_bus_sm, IO_Device); // First pio put : No Wait State if 0

  asm volatile ("nop");
/*
  To_Display32_2=IO_Device;
  To_Display32=ISA_IO_Addr^0x300;
*/
  /*if (ISA_IO_Addr!=0x320)
   {
    To_Display32_2=IO_Device;
    To_Display32=ISA_IO_Addr^0x300;
   } */

  switch(IO_CTRL_MDIndex)
      {
// ********** PicoMEM IOW **********        
       case CTRL_IOW : // ISA_Cycle=IOW  (1000b)
       uint32_t ISAIOW_Data;
       ISAIOW_Data=((uint32_t)gpio_get_all()>>PIN_AD0)&0xFF;
       switch(IO_Device)
        {
        case DEV_PM :
           dev_pmio_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
        case DEV_LTEMS :  // LTEMS : Write to the Bank Registers
           dev_ems_iow(ISA_IO_Addr,ISAIOW_Data);
        //    EMS_Bank[(uint8_t) ISA_IO_Addr & 0x03]=ISAIOW_Data;   // Write the Bank Register
	     //    EMS_Base[(uint8_t) ISA_IO_Addr & 0x03]=((uint32_t) ISAIOW_Data*16*1024)+1024*1024; // EMS Bank base Address : 1Mb + Bank number *16Kb (in SPI RAM)
	       break;  //DEV_LTEMS
#ifdef RASPBERRYPI_PICO_W
#if USE_NE2000
        case DEV_NE2000:
           dev_ne2000_iow((ISA_IO_Addr-PM_Config->ne2000Port) & 0x1F,ISAIOW_Data);
          break;
#endif
#endif
#if DEV_POST_Enable
	      case DEV_POST :
           dev_post_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif
#if USE_USBHOST
        case DEV_JOY :
           dev_joystick_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif          
#if USE_AUDIO
        case DEV_ADLIB :
           dev_adlib_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
        case DEV_CMS :
           dev_cms_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
        case DEV_TANDY :
           dev_tdy_iow(ISA_IO_Addr,ISAIOW_Data);
          break;   
        case DEV_MMB :
          dev_mmb_iow(ISA_IO_Addr,ISAIOW_Data);
         break; 
#if USE_SBDSP                             
        case DEV_SBDSP :
           dev_sbdsp_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif          
        case DEV_DMA :
           dev_dma_iow(ISA_IO_Addr,ISAIOW_Data);
          break;           
#endif

// *** Add Other device IOW here ***         

        }          // switch(IO_Device) (Write)
        break;     // End ISA_Cycle=IOW

// ********** PicoMEM IOR ********** 
       case CTRL_IOR : // ISA_Cycle=IOR (0100b)
       switch(IO_Device)
        {
        case DEV_PM :
           dev_pmio_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();
          break; //DEV_PM    
        case DEV_LTEMS :
           dev_ems_ior(ISA_IO_Addr,&ISA_Data);
           //ISA_Data=EMS_Bank[(uint8_t) ISA_IO_Addr & 0x03];  // Read the Bank Register
           pm_do_ior();
          break;  //DEV_LTEMS  
#if PM_PICO_W
#if USE_NE2000
        case DEV_NE2000:
           dev_ne2000_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();
          break;          
#endif          
#endif
#if USE_USBHOST
        case DEV_JOY:
           dev_joystick_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break;
#endif
#if USE_AUDIO
        case DEV_ADLIB:
           dev_adlib_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break;
        case DEV_CMS:
           dev_cms_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break;
        case DEV_MMB:
          dev_mmb_ior(ISA_IO_Addr,&ISA_Data);
          pm_do_ior();           
         break;      
#if USE_SBDSP                 
        case DEV_SBDSP:
           dev_sbdsp_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break;
#endif            
#endif          

   // *** Add Other device IOR here ***
        }         // switch(IO_Device) (Read)
        break;    // End ISA_Cycle=IOR
// *** Place no more than 4 Case  ***
       default: break;
     } // End switch(ISA_Cycle) ** In the IO Block

  //***    End of I/O Cycle     ***

// ** Now Need to send back the value Read, or stop the IOCHRDY **
if (ISA_WasRead)
   {
    pio_sm_put(isa_pio, isa_bus_sm, 0x00ffff00u | ISA_Data);  // 3nd Write : Send the Data to the CPU (Read Cycle)
    ISA_Data = pio_sm_get_blocking(isa_pio, isa_bus_sm);      // Wait that the data is sent to wait for the next ALE  
   }
   else
   {
    pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   }

} // End For()
// Never go there, soooooo sad :(

}    // main_core1 End
