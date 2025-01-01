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

 //ISA_WasRead = false;

// 2 Values returned by the Assembly code
  static uint32_t Ctrl_m=0x0F0000;   // Ctrl signals at Bit 16 to 19 are active high (after invertion)
  static uint32_t addr_m=0x000F00;   //
  uint32_t IO_CTRL_MDIndex;          // Control for IO or Dev Type for MEM, No need for uint8_t

 // ** Wait until a Control signal is detected **
 // ** Then, jump to MEMR / MEMW / IO and decode the Memory Address and Device Index
 asm volatile (
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
     "lsl %[DEV_T],%[DEV_T],#12       \n\t" // ISA_CTRL=ISA_CTRL<<12   CAAxxxxx
     "lsr %[DEV_T],%[DEV_T],#28       \n\t" // ISA_CTRL=ISA_CTRL>>28   -> ISA_CTRL is now correct

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
      );

  ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm) ^ addr_m; // Read (A19-A0)<<12 xor with mask

  pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 1st Write : No Wait State
  pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)

  if (IO_CTRL_MDIndex>2) 
     {
      To_Display32_2=ISA_Addr;
      To_Display32=IO_CTRL_MDIndex;
     }

} // End For()
// Never go there, soooooo sad :(

}    // main_core1 End
