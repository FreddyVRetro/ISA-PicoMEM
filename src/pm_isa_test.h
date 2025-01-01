



//***********************************************************************************************//
//*         CORE 1 Main : Wait for the ISA Bus events, emulate RAM and ROM                      *//
//***********************************************************************************************//
// The code is placed in the Scratch_x (4k Bank used for the Core 1 Stack) to avoid RAM Access conflict.

//void __time_critical_func(main_core1)(void)
//void __scratch_x("core1_ISA_code") (main_core1)(void)

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