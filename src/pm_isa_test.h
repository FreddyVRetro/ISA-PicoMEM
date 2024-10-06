



//***********************************************************************************************//
//*         CORE 1 Main : Wait for the ISA Bus events, emulate RAM and ROM                      *//
//***********************************************************************************************//
// The code is placed in the Scratch_x (4k Bank used for the Core 1 Stack) to avoid RAM Access conflict.

//void __time_critical_func(main_core1)(void)
//void __scratch_x("core1_ISA_code") (main_core1)(void)

// Test code, doing IO Only

void __time_critical_func(test_IOW_core1)(void)
{

// PSRAM 4 Bytes cache
uint32_t  PSRC_Addr;
uint8_t   PSRC_Data[4];

uint32_t  ISA_Addr;
uint8_t   ISA_Data;

// Send the value to use to return the fake Address (Address where the is a ROM or no IO)
 pio_sm_put(isa_pio, isa_bus_sm, 0x0FFFFF0F0);  // If 0x0FFFFF0FF, Floppy access (DMA) Crash

//for (;;) {}

for (;;) {
#if TIMING_DEBUG   
//  gpio_put(PIN_IRQ,0);  // IRQ Up
#endif

 ISA_WasRead = false;

// 2 Values returned by the Assembly code
  static uint32_t Ctrl_m=0x0F0000;
  uint32_t IO_CTRL_MDIndex;          // Control for IO, Dev Type for MEM, No need for uint8_t

 // ** Wait until a Control signal is detected **
 // ** Then, jump to MEMR / MEMW / IO and decode the Memory Address and Device Index
 //asm volatile goto (
asm volatile (
     "DetectCtrlEndLoop2:             \n\t"  // 1) Wait until there is no more Control signal (We detect an edge)
     "ldr %[DEV_T],[%[IOB],%[IOO]]   \n\t"  // Load the GPIO Values 
     "tst %[DEV_T],%[CM]             \n\t"  // Apply the mask to keep the ISA Control Signals Only
     "bne DetectCtrlEndLoop2          \n\t"  // Loop if Control Signal detected (!=0)

     "DetectCtrlLoop2:                \n\t"   // 2) Wait until a Control Signal is present
     "ldr %[DEV_T],[%[IOB],%[IOO]]   \n\t"   // Load the GPIO Values
     "tst %[DEV_T],%[CM]             \n\t"   // Apply the mask to keep the ISA Control Signals Only    
     "beq DetectCtrlLoop2             \n\t"   // Loop if no Control signal detected
     "str %[DEV_T],[%[PIOB],%[PIOT]] \n\t"   // pio->txf[0] = CTRL_AL8; Start the Cycle 
                                             // Then, PIO will returns AL12 or FFFFh if a DMA Cycle is detected
//     "tst %[DEV_T],2"
//     "beq NoDMA                   \n\t"   // Loop if no Control signal detected
//     "NoDMA:                      \n\t"

     "lsl %[DEV_T],%[DEV_T],#12       \n\t"  // ISA_CTRL=ISA_CTRL<<12   CAAxxxxx
     "lsr %[DEV_T],%[DEV_T],#28       \n\t"  // ISA_CTRL=ISA_CTRL>>28   -> ISA_CTRL is now correct

// Define Outputs
      :[DEV_T]"+l" (IO_CTRL_MDIndex),  // IO_CTRL_MDIndex is for MEM, use it as ISA_CTRL for I/O and used as Temp register
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
//     : ISA_Do_IO,
//       ISA_Do_MEMW
      );

//************************************************
//***         The ISA Cycle is IO              ***
//************************************************

   if (IO_CTRL_MDIndex<4) {  // If MEMR or MEMW, Complete the PIO SM cycle and Loop directly
        ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm);  // Read the Address
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 1st Write : No Wait state
        pio_sm_put(isa_pio, isa_bus_sm, 0x00u);  // 2nd Write : Directly Loop (Write Cycle or nothing)
        continue;  // Go back to the main loop begining
       }

  uint32_t ISA_IO_Addr;
  uint32_t IO_Device;

  ISA_Addr = pio_sm_get_blocking(isa_pio, isa_bus_sm); // Read (A19-A0)<<12
// Initial : 11
//  ISA_IO_Addr = (ISA_Addr>>12) & 0x3FF;
//  IO_Device = PORT_Table[ISA_IO_Addr>>3];

  IO_Device = PORT_Table[(ISA_Addr<<9)>>24];  // <<9 to keep the Bit 13 as well, for DMA Detection
  pio_sm_put(isa_pio, isa_bus_sm, IO_Device); // First pio put : No Wait State if 0

  asm volatile ("nop");

  ISA_IO_Addr = (ISA_Addr<<10)>>22;           // Same as (ISA_Addr>>12) & 0x3FF

/*  ISA_Addr=(ISA_Addr^0xF0000000)>>12;
  if (ISA_Addr>=0x3FF) 
     {
      //To_Display32=ISA_Addr;
      IO_Device=0;
     }
*/

//if (IO_Device) To_Display32=ISA_Addr;
//if (IO_Device!=4) IO_Device=0;
//IO_Device=0;
//To_Display32=ISA_Addr;

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
           EMS_Bank[(uint8_t) ISA_IO_Addr & 0x03]=ISAIOW_Data;   // Write the Bank Register
	         EMS_Base[(uint8_t) ISA_IO_Addr & 0x03]=((uint32_t) ISAIOW_Data*16*1024)+1024*1024; // EMS Bank base Address : 1Mb + Bank number *16Kb (in SPI RAM)
	      break;  //DEV_LTEMS
#ifdef RASPBERRYPI_PICO_W
#if USE_NE2000
        case DEV_NE2000:
           PM_NE2000_Write((ISA_IO_Addr-PM_Config->ne2000Port) & 0x1F,ISAIOW_Data);
          break;
#endif
#endif
#if DEV_POST_Enable==1
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
           ISA_Data=EMS_Bank[(uint8_t) ISA_IO_Addr & 0x03];  // Read the Bank Register
           pm_do_ior();
          break;  //DEV_LTEMS  
#if PM_PICO_W
#if USE_NE2000             
        case DEV_NE2000:        
           ISA_Data = PM_NE2000_Read((ISA_IO_Addr-PM_Config->ne2000Port) & 0x1F);
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

}    // test_IOW_core1 End


// Do test with the simplified PIO Code
void PM_DoISA_Test()
{
  uint isa_bus_offset = pio_add_program(isa_pio, &pm_isa_test_program);
  //printf("ISA_Offset: %d SM: %d\n",isa_bus_offset,isa_bus_sm); 
  if (pio_claim_unused_sm(isa_pio, true)!=isa_bus_sm) printf("PIO SM For ISA Error");
  pm_isa_test_program_init(isa_pio, isa_bus_sm, isa_bus_offset);

  uint32_t C_AL;
  uint32_t T_AH;
  uint32_t C_D;

 printf(" Start PicoMEM SM/Switch test /n");

  C_AL = pio_sm_get_blocking(isa_pio, isa_bus_sm);
  T_AH = pio_sm_get_blocking(isa_pio, isa_bus_sm);
  C_D  = pio_sm_get_blocking(isa_pio, isa_bus_sm);

  uint8_t T_IORW;
  uint32_t T_Addr;
  uint32_t T_Data;

  do
  {

  for (int i=0;i<20;i++)
   {
    for (int j=0;j<10;j++)   // Read 10 times to empty the FIFO
        {
         C_AL = pio_sm_get_blocking(isa_pio, isa_bus_sm);
         T_AH   = pio_sm_get_blocking(isa_pio, isa_bus_sm);
         C_D  = pio_sm_get_blocking(isa_pio, isa_bus_sm);
        }
    for (int j=0;j<100;j++)  // Now try to detect IOR/IOW/MEMR/MEMW
        {
         C_AL = pio_sm_get_blocking(isa_pio, isa_bus_sm);
         T_AH   = pio_sm_get_blocking(isa_pio, isa_bus_sm);
         C_D  = pio_sm_get_blocking(isa_pio, isa_bus_sm);
         T_IORW=C_AL>>28;
         if ((T_IORW)!=0xFF) break;
        }
     
     T_Addr=((C_AL>>20)&0xFF)+(T_AH>>12);
     T_Data=((C_D>>20)&0xFF);

    bool T_IsCycle;

     switch(T_IORW)
      {       
       case CTRL_MR_  : printf(" MR ");
                        T_IsCycle=true;
                        break;
       case CTRL_MW_  : printf(" MW ");
                        T_IsCycle=true;       
                        break;
       case CTRL_IOR_ : printf(" IR ");
                        T_IsCycle=true;              
                        break;
       case CTRL_IOW_ : printf(" IW ");
                        T_IsCycle=true;
                        break;
       case 0x0F      : printf(" XX ");
                        T_IsCycle=false;       
                        break;
       default        : printf(" ERR ");
                        T_IsCycle=false;       
      }

     if (T_IsCycle) printf("(Addr: %X, Data:%X) ",T_Addr,T_Data);

     printf(" 1-%X 2-%X 3-%X C: %X: ",C_AL>>20,T_AH>>20,C_D>>20,T_IORW);
   }

  } while(1);

}