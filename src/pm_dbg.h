#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "isa_iomem.pio.h"  // For Pin definitions


#if USE_PSRAM_DMA
#include "psram_spi.h"
extern psram_spi_inst_t psram_spi;
#else
#include "psram_spi2.h"
extern pio_spi_inst_t psram_spi;
#endif


// Code to display value, when the board is not in the PC (Manual test of the signals)
void display_inputs(void) {
    uint32_t value;
    gpio_put(PIN_AS, SEL_ADL);        // Low part of the @ and I/O Mem Signals
    gpio_put(PIN_AD, SEL_ADR);
    while (true) {
        value = gpio_get_all();
        printf("IO : %x\n",value);
        printf("AD : %x\n",(value>>8)&0xFF);
#if PM_PICO_W        
        sleep_ms(1000);
#else  
//        gpio_put(LED_PIN, 1);
         sleep_ms(500);
//        gpio_put(LED_PIN, 0);
        sleep_ms(500);
#endif
    }
}


// Simple IOWrite test without PIO
void IOW_Test_Loop () {

while(true) {
  gpio_put(PIN_AD, SEL_ADR);
  gpio_put(PIN_AS, SEL_ADL); // The Switch delay is <10ns , we may not need a delay to stabilize
  while (gpio_get(PIN_A19_IW)==0){}  // If already 0, Wait for a 1
  while (gpio_get(PIN_A19_IW)==1){}  // Wait until IOR is 0
 // gpio_put(PIN_DEBUG, 1);

  uint16_t Addr_L=(gpio_get_all()>>8)&0x00FF;
  gpio_put(PIN_AS, SEL_ADH);
  uint16_t Addr=(gpio_get_all())&0xFF00+Addr_L;

  gpio_put(PIN_AD, SEL_DAT);
  uint8_t Data=(gpio_get_all()>>8);

  printf("@ %x D %x - ",Addr,Data);

 // gpio_put(PIN_DEBUG, 0);  
} // While true
}

/*
// Copy IOW to the LED
void IOW_CopyLED () {

  printf("PIN: %d LEVEL %d\n",PIN_AD, SEL_ADR);
  printf("PIN: %d LEVEL %d\n",PIN_AS, SEL_ADL);
  gpio_put(PIN_AD, SEL_ADR);
  gpio_put(PIN_AS, SEL_ADL); // The Switch delay is <10ns , we may not need a delay to stabilize

while(true) {
  if (gpio_get(PIN_A19_IW)==0)
     { 
      printf("0");
#if !PM_PICO_W      
      gpio_put(LED_PIN, 0);
#endif
      while (gpio_get(PIN_A19_IW)==0){}  // Wait until 1     
     } 
     else
     {
      printf("1 ");   
#if !PM_PICO_W     
      gpio_put(LED_PIN, 1);
#endif
      while (gpio_get(PIN_A19_IW)==1){}  // Wait until 1 
     }

} // While true
} // IOR_LED

// Copy the IOW Signal to the IRQ Output (For Test)
// Code OK / Tested
void IOW_Copy () {

  gpio_put(PIN_AD, SEL_ADR);
  gpio_put(PIN_AS, SEL_ADL); // The Switch delay is <10ns , we may not need a delay to stabilize

while(true) {
//    gpio_put(PIN_DEBUG, gpio_get(PIN_A19_IW));
} // While true

} // IOW_Copy
*/

/*
// Code OK
void measure_freqs(void) {

 uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
 uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
 uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
 uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
 uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
 uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
 uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
 uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);
 
 printf("pll_sys = %dkHz\n", f_pll_sys);
 printf("pll_usb = %dkHz\n", f_pll_usb);
 printf("rosc = %dkHz\n", f_rosc);
 printf("clk_sys = %dkHz\n", f_clk_sys);
 printf("clk_peri = %dkHz\n", f_clk_peri);
 printf("clk_usb = %dkHz\n", f_clk_usb);
 printf("clk_adc = %dkHz\n", f_clk_adc);
 printf("clk_rtc = %dkHz\n", f_clk_rtc);
 
 // Can't measure clk_ref / xosc as it is the ref
 }

*/

/*
void testCMD()  // Test the commands
{

printf("Test Commands \n");

PM_Status=10;
PM_Command=CMD_Reset;
PM_ExecCMD();
if (PM_Status==0) {printf("Ok\n");}

PM_BasePort=0x44;
PM_Command=CMD_GetBasePort;
PM_ExecCMD();
if (PM_CmdDataL==PM_BasePort) {printf("Ok\n");}

printf("PM_Memory [0..64]\n");
for (uint32_t i=0 ; i<64 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\n");
printf("PM_Memory [65536-32..65536+32]\n");
for (uint32_t i=65536-32 ; i<65536+32 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\nPM_Memory[65535] %x\n",PM_Memory[65535]);

PM_Memory[0]=0xAA;
printf("PM_Memory[0] %x, ",PM_Memory[0]);

PM_CmdDataL=0xAA;
PM_Command=CMD_SetMEM;
PM_ExecCMD();

printf("PM_Memory [0..64]\n");
for (uint32_t i=0 ; i<64 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\n");
printf("PM_Memory [65536-32..65536+32]\n");
for (uint32_t i=65536-32 ; i<65536+32 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\nPM_Memory[65535] %x\n",PM_Memory[65535]);
printf("PM_Memory[65536] %x\n",PM_Memory[65536]);

PM_CmdDataL=0x55;
PM_Command=CMD_SetMEM;
PM_ExecCMD();
printf("PM_Memory [0..64]\n");
for (uint32_t i=0 ; i<64 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\n");
printf("PM_Memory [65536-32..65536+32]\n");
for (uint32_t i=65536-32 ; i<65536+32 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\nPM_Memory[65535] %x\n",PM_Memory[65535]);
printf("PM_Memory[65536] %x\n",PM_Memory[65536]);

err_blink(2,0);

}

void CMD_WaitEnd()
{
do { } while (PM_Status==STAT_CMDINPROGRESS); 
}

void testCMDMultiCore()  // Test the commands
{

printf("Test Commands \n");

PM_Status=10;
PM_Command=CMD_Reset;
PM_Status=STAT_CMDINPROGRESS;
do {
    printf("%x",PM_Status);
    sleep_ms(100);
 } while (PM_Status==STAT_CMDINPROGRESS);
//PM_ExecCMD();
if (PM_Status==0) {printf("Ok\n");}

PM_BasePort=0x44;
PM_Command=CMD_GetBasePort;
PM_Status=STAT_CMDINPROGRESS;
CMD_WaitEnd();
//PM_ExecCMD();
if (PM_CmdDataL==PM_BasePort) {printf("Ok\n");}

printf("PM_Memory [0..64]\n");
for (uint32_t i=0 ; i<64 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\n");
printf("PM_Memory [65536-32..65536+32]\n");
for (uint32_t i=65536-32 ; i<65536+32 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\nPM_Memory[65535] %x\n",PM_Memory[65535]);

PM_Memory[0]=0xAA;
printf("PM_Memory[0] %x, ",PM_Memory[0]);

PM_CmdDataL=0xAA;
PM_Command=CMD_SetMEM;
PM_Status=STAT_CMDINPROGRESS;
CMD_WaitEnd();
//PM_ExecCMD();

printf("PM_Memory [0..64]\n");
for (uint32_t i=0 ; i<64 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\n");
printf("PM_Memory [65536-32..65536+32]\n");
for (uint32_t i=65536-32 ; i<65536+32 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\nPM_Memory[65535] %x\n",PM_Memory[65535]);
printf("PM_Memory[65536] %x\n",PM_Memory[65536]);

PM_CmdDataL=0x55;
PM_Command=CMD_SetMEM;
PM_Status=STAT_CMDINPROGRESS;
CMD_WaitEnd();
//PM_ExecCMD();
printf("PM_Memory [0..64]\n");
for (uint32_t i=0 ; i<64 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\n");
printf("PM_Memory [65536-32..65536+32]\n");
for (uint32_t i=65536-32 ; i<65536+32 ; ++i) printf("%x, ",PM_Memory[i]);
printf("\nPM_Memory[65535] %x\n",PM_Memory[65535]);
printf("PM_Memory[65536] %x\n",PM_Memory[65536]);

err_blink(2,0);

}
*/

void PSRAM_Test()
{
 printf("PSRAM Test\n");

#if USE_PSRAM

 for (int i=0;i<5;i++)
    { 
     psram_write8(&psram_spi, 0,0xAA);
     printf("AA-%x ",psram_read8(&psram_spi, 0));
     psram_write8(&psram_spi, 0,0x55);
     printf("55-%x ",psram_read8(&psram_spi, 0));
    }
 printf("\n>Write Data\n");
//    puts("Writing PSRAM...");
 for (uint32_t addr = 0; addr < (1024 * 1024); ++addr) {
     psram_write8(&psram_spi, addr, (uint8_t) (addr & 0xFF));
    }
 
 printf(">Read Data (8/16/32)\n");
 uint32_t ErrNb=0;
//    puts("Reading PSRAM...");
 uint32_t psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < (1024 * 1024); ++addr) {
     uint8_t result = psram_read8(&psram_spi, addr);
     if (static_cast<uint8_t>((addr & 0xFF)) != result) 
         {
         printf("PSRAM failure at address %x (%x != %x)\n", addr, addr & 0xFF, result);
         if (ErrNb++==10) break;
         }
 }
 uint32_t psram_elapsed = time_us_32() - psram_begin;
 float psram_speed = 1000000.0 * 1024.0 * 1024 / psram_elapsed;
 printf("8 bit : PSRAM read 1MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

 psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < (1024 * 1024); addr += 2) {
     uint16_t result = psram_read16(&psram_spi, addr);
     if (static_cast<uint16_t>(
             (((addr + 1) & 0xFF) << 8) |
             (addr & 0XFF)) != result
        ) 
        {
         printf("PSRAM failure at address %x (%x != %x)\n", addr, addr & 0xFF, result & 0xFF);
         if (ErrNb++==20) break;
         //err_blink(5,0);
        }
    }
 psram_elapsed = (time_us_32() - psram_begin);
 psram_speed = 1000000.0 * 1024 * 1024 / psram_elapsed;
 printf("16 bit: PSRAM read 1MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

 psram_begin = time_us_32();
 for (uint32_t addr = 0; addr < (1024 * 1024); addr += 4) {
     uint32_t result = psram_read32(&psram_spi, addr);
     if (static_cast<uint32_t>(
             (((addr + 3) 
             & 0xFF) << 24) |
             (((addr + 2) & 0xFF) << 16) |
             (((addr + 1) & 0xFF) << 8)  |
             (addr & 0XFF)) != result
     ) {
         printf("PSRAM failure at address %x (%x != %x)\n", addr, addr & 0xFF, result & 0xFF);
         if (ErrNb++==30) break;
        }
 }
 psram_elapsed = (time_us_32() - psram_begin);
 psram_speed = 1000000.0 * 1024 * 1024 / psram_elapsed;
 printf("32 bit: PSRAM read 1MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

if (ErrNb==0) {printf("PSRAM Test OK :) \n");}
   else {printf("PSRAM Test FAIL :( \n");}
#endif

/*

printf("PSRAM Block Read/Write\n");
for (int i=0;i<256;i++) PM_DP_RAM[i]=(uint8_t) i;
for (int i=0;i<256;i++) printf("%d,",PM_DP_RAM[i]);
printf("\nPSRAM Block Write\n");
psram_writebuffer(&psram_spi,0,(uint8_t*)&PM_DP_RAM[0],256);
for (int i=0;i<256;i++) printf("%d,",PM_DP_RAM[i]);
for (int i=0;i<256;i++) PM_DP_RAM[i]=0;
printf("\nPSRAM Block Read\n");
psram_readbuffer(&psram_spi,0,(uint8_t*) &PM_DP_RAM[0],256);
for (int i=0;i<256;i++) printf("%d,",PM_DP_RAM[i]);

uint32_t* PM_DP_RAM32=(uint32_t*)PM_DP_RAM;
for (int i=0;i<256;i++) PM_DP_RAM[i]=(uint8_t) i;
printf("\nPSRAM Write 32b\n");
for (int i=0;i<64;i++) psram_write32(&psram_spi,i<<2,PM_DP_RAM32[i]);
for (int i=0;i<256;i++) PM_DP_RAM[i]=0;
printf("\nPSRAM Read 32b\n");
for (int i=0;i<64;i++) PM_DP_RAM32[i]=psram_read32(&psram_spi,i<<2);
for (int i=0;i<256;i++) printf("%d,",PM_DP_RAM[i]);
*/

}

#ifdef __cplusplus
}
#endif