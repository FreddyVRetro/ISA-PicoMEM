// RP2350 ISA Bus Code for PicoMEM 1.5 and 2.x Boards²


//#define DO_MEMR 0x3010u  // PIO instruction for (wait 0 gpio PIN_A16_MR) 
//#define DO_IOR  0x3012u  // PIO instruction for (wait 0 gpio PIN_A18_IR) 

#if PM_SYS_CLK==360
#define MUX_DELAY 5
#else
#define MUX_DELAY 4
#endif

#define DO_MEMR 2 
#define DO_IOR  1 

__force_inline void pm_do_ior(void)
{
 ISA_WasRead=true;
// 0x380e, // 16: wait   0 gpio, 14      side 2
 pio_sm_put(isa_pio, isa_bus_sm, 0x380e); // Do IOR (wait 0 gpio PIN_A18_IR)
} // Data is sent after in the core1

// Start of a Memory Read with Wait States added
__force_inline void pm_do_memr(uint32_t ISA_Data)
{
 //0x380c, // 18: wait   0 gpio, 12      side 2
 pio_sm_put(isa_pio, isa_bus_sm, 0x380c);             // 2nd Write : Do MEMR (wait 0 gpio PIN_A16_MR)
 gpioc_hi_out_clr(1<<PIN_IORDY-32);
 pio_sm_put(isa_pio, isa_bus_sm, ISA_Data);           // 3nd Write : Send the Data to the CPU (Read Cycle)
 asm volatile ("nop");                                // Force the compiler to not prepare the next instruction in advanced : One Cycle less
 // Added to wait until the PIO Send the Data
 ISA_Data = pio_sm_get_blocking(isa_pio, isa_bus_sm); // Wait that the data is sent to wait for the next ALE 
}


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

PM_INFO("Core1: pm_isa_rp2350_m2_v15\n");

uint32_t  ISA_Addr;
uint8_t   ISA_Data;

for (;;) {
  gpioc_hi_out_clr(1<<PIN_IORDY-32);

  ISA_WasRead = false;
#if CORE1_ALIVETEST
  Core1_AliveCnt++; 
#endif
// Minimum Delay for the multiplexer
// <=320MHz : 4  
  pio_sm_put(isa_pio, isa_bus_sm, MUX_DELAY);   // Send the multiplexer loop delay

// 2 Values returned by the Assembly code
  static uint32_t Ctrl_m=0xF0000000; // Ctrl signals at Bit 28 to 31 are active high (after invertion)
  static uint32_t addr_m=0x000F00;   // Mask to revert the 4Bit for the address
  uint32_t IO_CTRL_MDIndex;          // Control for IO or Dev Type for MEM, No need for uint8_t
  uint32_t MEM_Index;          // Memory type index

 // ** Wait until a Control signal is detected **
 // ** Then, jump to MEMR / MEMW / IO and decode the Memory Address and Device Index
 asm volatile goto (
     "DetectCtrlEndLoop:             \n\t"  // 1) Wait until there is no more Control signal (We detect an edge)
     "mrc p0, #0, %[ADDR], c0, c8    \n\t"  // %[ADDR]=gpio_get_all()
     "tst %[ADDR],%[CM]              \n\t"  // Apply the mask to keep the ISA Control Signals Only
     "bne DetectCtrlEndLoop          \n\t"  // Loop if Control Signal detected (!=0)

     "DetectCtrlLoop:                \n\t"  // 2) Wait until a Control Signal is present
     "mrc p0, #0, %[ADDR], c0, c8    \n\t"  // %[ADDR]=gpio_get_all()
     "tst %[ADDR],%[CM]              \n\t"  // Apply the mask to keep the ISA Control Signals Only    
     "beq DetectCtrlLoop             \n\t"  // Loop if no Control signal detected
     "str %[ADDR],[%[PIOB],%[PIOT]]  \n\t"  // pio->txf[0] = CTRL_AL8; Start the Cycle 

// %[CTRL_I] contains A12 to A19, then MR, MW, IOR, IOW from the Bit 8 PM15 : CAA-----
     "lsr %[CTRL_I],%[ADDR],#28      \n\t"  // ISA_CTRL=GPIO_ALL>>28   -> ISA_CTRL is now correct

     "cmp %[CTRL_I],#3               \n\t"    // Is it a MEM Read or Write ?
     "bgt  %l[ISA_Do_IO]             \n\t"    // Jump to IO Code

     	// Start of the Memory R/W > Move IOCHRDY as fast as possible
     "ubfx %[ADDR],%[ADDR], #21, #7   \n\t"   //  Extract the High part of the Address
     "ldrb %[MEMI],[%[MIT],%[ADDR]]   \n\t"   //  Load the Memory Index
     "cmp	 %[MEMI], #15	           \n\t"   //  Do we access Slow RAM ? (Need IOCHRDY)
     "bls RAM_No_IOCHRDY              \n\t"
     "mcr p0, #2, r8, c0, c1          \n\t"   // gpioc_hi_out_set(1<<PIN_IORDY-32);
     "RAM_No_IOCHRDY:                 \n\t"

//    "lsrs  %[ADDR] ,%[MEMI], #3         \n\t"  // pio_sm_put(isa_pio, isa_bus_sm, MEM_Index>>3);
//    "str   %[ADDR] ,[%[PIOB],%[PIOT]]   \n\t"   // Index <<3 Sent to the PIO to move or not IOCHRDY


// Define Outputs
     :[CTRL_I]"+l" (IO_CTRL_MDIndex),      // IO_CTRL_MDIndex is for MEM, use it as ISA_CTRL for I/O and used as Temp register
      [ADDR]"+l" (ISA_Addr),
      [MEMI]"+l" (MEM_Index)
// Define Inputs
     :[CM]"l"   (Ctrl_m),
      [MIT]"l"  (MEM_Index_T),          // l for low register     
      [IOB]"l"  (SIO_BASE),             // GPIO Registers Base Address (l for low register)
      [IOO]"i"  (SIO_GPIO_IN_OFFSET),   // GPIO IN Offset (#4)
      [PIOB]"l" (isa_pio),              // Base adress of the PIO used for ISA
      [PIOT]"i" (PIO_TXF0_OFFSET),
      [PIOR]"i" (PIO_RXF0_OFFSET)
     :"cc"                              // Tell the compiler that regs are changed
     : ISA_Do_IO
//       ISA_Do_MEMW
      );

//#define MEM_Index ISA_Addr  // The Memory index is returned in the ISA_Addr variable/register

ISA_Do_MEM:

//  uint32_t MEM_Index;          // Memory type index
//  MEM_Index=MEM_Index_T[(ISA_Addr<<(32-PIN_A8_MR))>>(32-7)];  // Get the Memory Type Index from A19-A13 (7 bit, 128 values)
//  pio_sm_put(isa_pio, isa_bus_sm, MEM_Index>>3); // 1st Write : No Wait State if 0
//  if (MEM_Index>15) gpioc_hi_out_set(1<<PIN_IORDY-32);

  if (IO_CTRL_MDIndex==1)
   {
// ********** ISA_Cycle=Memory Read (0001b) **********
   if (MEM_Index)    // Read if any emulated Memory is present
        {   //!! Be carefull when optimizing / Overclocking 
#if PM_SYS_CLK>340
          asm volatile ("nop");
#endif            
        pm_do_memr(RAM_Offset_Table[MEM_Index][pio_sm_get(isa_pio, isa_bus_sm)^addr_m]);
        continue;  // Go back to the main loop begining
        }

   pio_sm_get_blocking(isa_pio, isa_bus_sm);  // Dummy Read and discard the address
   pio_sm_put(isa_pio, isa_bus_sm, 0x00u);    // 2nd Write : Directly Loop (Write Cycle or nothing)
   continue;  // Go back to the main loop begining
   }
   else
   {
// ********** ISA_Cycle=Memory Write (0010b) **********
#if PM_SYS_CLK>340
 asm volatile ("nop");
#endif  
   ISA_Addr = pio_sm_get(isa_pio, isa_bus_sm);  // Read the Address
   pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)

// ** Write to the PicoMEM Internal RAM
   if ((MEM_Index>0)&&(MEM_Index<MI_W_NOWS_END))  // Memory in the Pico Memory or ROM
         {  //!! Be carefull when optimizing / Overclocking 
 #if PM_SYS_CLK>340
          asm volatile ("nop");
 #endif
          RAM_Offset_Table[MEM_Index][ISA_Addr^addr_m]=gpio_get_all()>>PIN_AD0;
         }
   else if (MEM_Index>=16)                      // Index >=16 : PSRAM (RAM or EMS)
        {   //!! Be carefull when optimizing / Overclocking 
#if PM_SYS_CLK>340
          asm volatile ("nop");
#endif    
          RAM_Offset_Table[MEM_Index][ISA_Addr^addr_m]=gpio_get_all()>>PIN_AD0;
        }
   else {}  // To add : RAM Capture with PSRAM (With pimoroni)

   continue;  // Go back to the main loop begining
   }

//*************************************************
//***         The ISA Cycle is IO               ***
//*************************************************
ISA_Do_IO:  // Goto, from the assembly code

  uint32_t ISA_IO_Addr;
  uint32_t IO_Device;

  ISA_IO_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm) & 0x3FF; // No need for Mask (Only the low 8Bit are used)

  IO_Device = IO_Index_T[ISA_IO_Addr>>3];
//  pio_sm_put(isa_pio, isa_bus_sm, IO_Device); // First pio put : No Wait State if 0
  if (IO_Device) gpioc_hi_out_set(1<<PIN_IORDY-32);

  asm volatile ("nop");
  ISA_IO_Addr=ISA_IO_Addr ^ 0x300;

  switch(IO_CTRL_MDIndex)
      {
// ********** PicoMEM IOW **********        
       case CTRL_IOW : // ISA_Cycle=IOW  (1000b)
       uint32_t ISAIOW_Data;
       #if PM_SYS_CLK>340
         asm volatile ("nop");
       #endif       
       ISAIOW_Data=((uint32_t)gpio_get_all()>>PIN_AD0)&0xFF;
//       if (IO_Device) PM_INFO("%d",IO_Device);
       switch(IO_Device)
        {
        case DEV_PM :
           dev_pmio_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
        case DEV_LTEMS :  // LTEMS : Write to the Bank Registers
           dev_ems_iow(ISA_IO_Addr,ISAIOW_Data);
	       break;  //DEV_LTEMS
#if PM_WIFI
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
#if DEV_IRQ
         case DEV_IRQ :
           dev_irq_iow(ISA_IO_Addr,ISAIOW_Data);
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
        case DEV_CVX :
          dev_cvx_iow(ISA_IO_Addr,ISAIOW_Data);
         break;
#if USE_SBDSP                           
        case DEV_SBDSP :
           dev_sbdsp_iow(ISA_IO_Addr,ISAIOW_Data);
          break;     
        case DEV_DMA :
           dev_dma_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif
#if USE_GUS
        case DEV_GUS :
           dev_gus_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif
#if USE_MPU
        case DEV_MPU :
           dev_mpu_iow(ISA_IO_Addr,ISAIOW_Data);
          break;
#endif 
#endif

#if USE_RTC
        case DEV_RTC :
           dev_rtc_iow(ISA_IO_Addr,ISAIOW_Data);
           break;   
#endif
#if USE_CDROM
        case DEV_CDR : // MKE CD
           dev_cdr_iow(ISA_IO_Addr,ISAIOW_Data);
           break; 
#endif

// *** Add Other device IOW here ***         

        }          // switch(IO_Device) (Write)
        break;     // End ISA_Cycle=IOW

// ********** PicoMEM IOR ********** 
       case CTRL_IOR : // ISA_Cycle=IOR (0100b) 
//       if (IO_Device) PM_INFO("%d",IO_Device);
       switch(IO_Device)
        {
        case DEV_PM :
           dev_pmio_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();
          break; //DEV_PM    
        case DEV_LTEMS :
           dev_ems_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();
          break;  //DEV_LTEMS  
#if PM_WIFI
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
           if (dev_adlib_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();          
          break;
        case DEV_CMS:
           if (dev_cms_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();
          break;
        case DEV_MMB:
          if (dev_mmb_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();         
          break;
        case DEV_CVX:
           if (dev_cvx_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();        
          break; 
#if USE_SBDSP                 
        case DEV_SBDSP:
           if (dev_sbdsp_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();        
          break;
#endif
#if USE_GUS
        case DEV_GUS:
           if (dev_gus_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();        
          break; 
#endif
#if USE_MPU                    
        case DEV_MPU:
           if (dev_mpu_ior(ISA_IO_Addr,&ISA_Data)) pm_do_ior();           
          break;           
#endif 
#endif          
#if USE_RTC
        case DEV_RTC:
           dev_rtc_ior(ISA_IO_Addr,&ISA_Data);
           pm_do_ior();           
          break; 
#endif
#if USE_CDROM
        case DEV_CDR : // MKE CD
           dev_cdr_ior(ISA_IO_Addr,&ISA_Data);
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
if (ISA_WasRead) {
    gpioc_hi_out_clr(1<<PIN_IORDY-32);
    pio_sm_put(isa_pio, isa_bus_sm, (uint32_t) ISA_Data);     // 3nd Write : Send the Data to the CPU (Read Cycle)
    ISA_Data = pio_sm_get_blocking(isa_pio, isa_bus_sm);      // Wait that the data is sent to wait for the next ALE  
   }
   else {
    pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
   }

} // End For()
// Never go there, soooooo sad :(

}    // main_core1 End
