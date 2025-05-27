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

#if DISP32
//To_Display32=0xAA55;
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
     "ldr %[MIT],[%[IOB],%[IOO]]     \n\t"  // Load the GPIO Values 
     "tst %[MIT],%[CM]               \n\t"  // Apply the mask to keep the ISA Control Signals Only
     "bne DetectCtrlEndLoop          \n\t"  // Loop if Control Signal detected (!=0)

     "DetectCtrlLoop:                \n\t"  // 2) Wait until a Control Signal is present
     "ldr %[MIT],[%[IOB],%[IOO]]     \n\t"  // Load the GPIO Values
     "tst %[MIT],%[CM]               \n\t"  // Apply the mask to keep the ISA Control Signals Only    
     "beq DetectCtrlLoop             \n\t"  // Loop if no Control signal detected
     "str %[MIT],[%[PIOB],%[PIOT]]   \n\t"  // pio->txf[0] = CTRL_AL8; Start the Cycle 
                                            // Then, PIO will returns AL12 or FFFFh if a DMA Cycle is detected   
// %[DEV_T] contains A12 to A19, then MR, MW, IOR, IOW from the Bit 8 
     "lsl %[DEV_T],%[MIT],#12        \n\t" // ISA_CTRL=ISA_CTRL<<12   CAAxxxxx
     "lsr %[DEV_T],%[DEV_T],#28      \n\t" // ISA_CTRL=ISA_CTRL>>28   -> ISA_CTRL is now correct

     "cmp %[DEV_T],#1                \n\t"  // Is it a MEM Read ?
     "beq  ISA_Do_MEMR               \n\t"  // Jump must be <200 bytes

     "cmp %[DEV_T],#2                \n\t"  // Is it a MEM Read ?
     "beq  ISA_Do_MEMW               \n\t"  // Jump must be <200 bytes

//** > ISA IOR / IOW   **

     "b %l[ISA_Do_IO]                \n\t"  // If %[TMP]!=0 Do IO    Jump must be <200 bytes

//** > ISA Memory Write Address read and IOCHRDY **
// !! This portion of the code is "Hard Coded" to be cycle accurate with the State Machine !
// If the Memory access does not work anymore, check this part of the code and add "nop".
     "ISA_Do_MEMW:                      \n\t"

     "nop            \n\t"    // Added to wait until the PIO Send the Address Value
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value

// Directly read the RX pipe : Be carefull about the timing
     "ldr %[ADDR],[%[PIOB],%[PIOR]]      \n\t"   // ISA_Addr<<12 = pio->rxf[sm]; Read ISA_Addr from the PIO (Address with Mask)
     "lsr %[DEV_T], %[ADDR], #25         \n\t"   // ISA_Addr>>20+4 (16kb Block) >>20+5 for 8Kb Block
     "ldrb %[DEV_T], [%[MIT], %[DEV_T]]  \n\t"   // IO_CTRL_MDIndex = MEM_Index_T[(ISAM_AH12>>x)] (TMP=MTA)
     "lsr %[MIT], %[DEV_T], #3           \n\t"   // TMP = IO_CTRL_MDIndex>>3 (0-7 : No Wait State)
     "str %[MIT], [%[PIOB], %[PIOT]]     \n\t"   // pio->txf[sm] = TMP;  > Command the IOCHRDY Move if !=0
// complete the ISA_Addr Address calculation for Memory Read
     "lsr %[ADDR], %[ADDR], #12          \n\t"   // ISA_Addr=ISA_Addr>>12
     "eor %[ADDR], %[CM]                 \n\t"   // ISA_Addr+ISA_Addr^Ctrl_m  (0x0F0000)
     "b %l[ISA_Do_MEMW]                  \n\t"   //

//** > ISA Memory Read Address read and IOCHRDY **
     "ISA_Do_MEMR:                      \n\t"   // Minimum 4 NOP (20/05)  5 at 300MHz on the PM 1.0
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value  
#if DO_FAST_RAM    // Fast RAM : Can tell to not move IOCHRDY Directly
     "mov %[ADDR], #0                     \n\t"   // TMP = 0
     "str %[ADDR], [%[PIOB], %[PIOT]]     \n\t"   // pio->txf[sm] = TMP;  > Command the IOCHRDY Move if !=0
#else
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value 
     "nop            \n\t"    // Added to wait until the PIO Send the Address Value 
#endif

// Directly read the RX pipe : Be carefull about the timing
     "ldr %[ADDR],[%[PIOB],%[PIOR]]      \n\t"   // ISA_Addr<<12 = pio->rxf[sm]; Read ISA_Addr from the PIO (Address with Mask) 
     "lsr %[DEV_T], %[ADDR], #25         \n\t"   // ISA_Addr>>20+4 (16kb Block) >>20+5 for 8Kb Block   > Ok for test, will always be 0 
     "ldrb %[DEV_T], [%[MIT], %[DEV_T]]  \n\t"   // IO_CTRL_MDIndex = MEM_Index_T[(ISA_Addr>>25)]
#if DO_FAST_RAM    // Fast RAM : Only 0 or a no PSRAM Memory
#else
     "lsr %[MIT], %[DEV_T], #3           \n\t"   // TMP = IO_CTRL_MDIndex>>3 (3, 0-7 : No Wait State) (3, 0-7)
     "str %[MIT], [%[PIOB], %[PIOT]]     \n\t"   // pio->txf[sm] = TMP;  > Command the IOCHRDY Move if !=0
#endif
// complete the ISA_Addr Address calculation for Memory Read
     "lsr %[ADDR], %[ADDR], #12          \n\t"   // ISA_Addr=ISA_Addr>>12
     "eor %[ADDR], %[CM]                 \n\t"   // ISA_Addr+ISA_Addr^Ctrl_m (0x0F0000)

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

//*************************************************
//***         The ISA Cycle is MEM              ***
//*************************************************

#if DISP32
//To_Display32=ISA_Addr;
#endif

IO_CTRL_MDIndex=0;  // Test / Debug

// ********** ISA_Cycle= Memory Read  (0001b) **********

// ** Read from the Pico Memory or ROM
#if DO_FAST_RAM    // Fast RAM : Only 0 or a no PSRAM Memory
   if (IO_CTRL_MDIndex!=0) 
#else
   if ((IO_CTRL_MDIndex>0)&&(IO_CTRL_MDIndex<MI_R_WS_END))    // 0<Index<MI_R_WS_END : "Fast RAM"  
#endif     
        {
        pm_do_memr(RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]);
        continue;  // Go back to the main loop begining
        }
#if DO_FAST_RAM   // Fast RAM : Only 0 or a no PSRAM Memory
        else
       {
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }
#else
   if (IO_CTRL_MDIndex==MEM_I_NULL) {
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }

// ** Read from the PSRAM  (Emulated RAM or EMS)
   if (IO_CTRL_MDIndex==MEM_EMS) {  // EMS
        ISA_Addr=EMS_Base[(ISA_Addr>>14)&0x03]+(ISA_Addr & 0x3FFF);  // Compute the base PSRAM Address              
       }
//#if PM_PRINTF
//          if (!ePSRAM_Enabled) printf("!Fatal EMS Read %x ",ISA_Addr);             
//#endif 
#if USE_PSRAM
#if USE_PSRAM_DMA
       pm_do_memr(psram_read8(&psram_spi, ISA_Addr));
#else
       pm_do_memr(psram_read8(&psram_spi, ISA_Addr));
#endif
#endif                           
       continue;  // Go back to the main loop begining
#endif


// ********** ISA_Cycle=Memory Write (0010b) **********
ISA_Do_MEMW:

#if DISP32
//To_Display32=ISA_Addr;
#endif

   IO_CTRL_MDIndex=0;  // Test / Debug

   uint32_t ISAMW_Data;
   ISAMW_Data=((uint32_t)gpio_get_all()>>PIN_AD0);   

// ** Write to the PicoMEM Internal RAM
#if DO_FAST_RAM    // Fast RAM : Only 0 or a no PSRAM Memory
   if (IO_CTRL_MDIndex!=0) 
#else
   if ((IO_CTRL_MDIndex>0)&&(IO_CTRL_MDIndex<MI_W_NOWS_END))  // Memory in the Pico Memory or ROM
#endif
             {
              RAM_Offset_Table[IO_CTRL_MDIndex][ISA_Addr]=(uint8_t) ISAMW_Data;
              pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
              continue;  // Go back to the main loop begining              
             }
#if DO_FAST_RAM   // Fast RAM : Only 0 or a no PSRAM Memory
        else
       {
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }
#else
   if (IO_CTRL_MDIndex==MEM_I_NULL) {
          pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
          continue;  // Go back to the main loop begining
         }

// ** Write to the PSRAM (Emulated RAM or EMS)
   if (IO_CTRL_MDIndex==MEM_EMS) {  // EMS
         ISA_Addr=EMS_Base[(ISA_Addr>>14)&0x03u]+(ISA_Addr & 0x3FFFu);  // Compute the base PSRAM Address           
        }
#if USE_PSRAM_DMA
      psram_write8_async(&psram_spi, ISA_Addr, (uint8_t) ISAMW_Data);
#else
      psram_write8(&psram_spi, ISA_Addr, (uint8_t) ISAMW_Data);
#endif        
   
   pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   continue;  // Go back to the main loop begining
#endif
 //***    End of Memory Cycle         ***

//*************************************************
//***         The ISA Cycle is IO               ***
//*************************************************
ISA_Do_IO:  // Goto, from the assembly code

  uint32_t ISA_IO_Addr;
  uint32_t IO_Device;

  ISA_Addr = (pio_sm_get_blocking(isa_pio, isa_bus_sm) ^ addr_m) & 0x3FF;

  IO_Device = PORT_Table[ISA_Addr>>3]; 
  IO_Device =0;
  pio_sm_put(isa_pio, isa_bus_sm, IO_Device); // First pio put : No Wait State if 0

  asm volatile ("nop");

//  IO_Device =0;

#if DISP32
To_Display32=ISA_Addr;
#endif

  switch(IO_CTRL_MDIndex)
      {
// ********** PicoMEM IOW **********        
       case CTRL_IOW : // ISA_Cycle=IOW  (0111b)
       uint32_t ISAIOW_Data;
       ISAIOW_Data=((uint32_t)gpio_get_all()>>PIN_AD0)&0xFF;
       switch(IO_Device)
        {
        case DEV_PM :
           dev_pmio_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
        case DEV_LTEMS :  // LTEMS : Write to the Bank Registers
           dev_ems_iow(ISA_IO_Addr,ISAIOW_Data);
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

#if USE_RTC
        case DEV_RTC :
           dev_rtc_iow(ISA_IO_Addr,ISAIOW_Data);
           break;   
#endif

// *** Add Other device IOW here ***         

        }          // switch(IO_Device) (Write)
        break;     // End ISA_Cycle=IOW

// ********** PicoMEM IOR ********** 
       case CTRL_IOR : // ISA_Cycle=IOR (1011b)
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

#if USE_RTC
        case DEV_RTC:
           dev_rtc_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break; 
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
