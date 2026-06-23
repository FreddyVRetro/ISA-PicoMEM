#pragma once

#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"


//https://github.com/STMicroelectronics/stm32ai-datalogger


__force_inline void DBG_PIN_INIT()
{
#if DBG_PIN_AUDIO
 gpio_init(1);
 gpio_set_dir(1, GPIO_OUT);
 gpio_put(1, 0);
 gpio_init(42);
 gpio_set_dir(42, GPIO_OUT);
 gpio_put(42, 0);
 gpio_init(43);
 gpio_set_dir(43, GPIO_OUT);
 gpio_put(43, 0);

 gpio_init(44);
 gpio_set_dir(44, GPIO_OUT);
 gpio_put(44, 0); 
 gpio_init(45);
 gpio_set_dir(45, GPIO_OUT);
 gpio_put(45, 0);

 gpio_init(46);
 gpio_set_dir(46, GPIO_OUT);
 gpio_put(46, 0);
 gpio_init(47);
 gpio_set_dir(47, GPIO_OUT);
 gpio_put(47, 0);
#endif
}

__force_inline void DBG_ON_1()  // PIN 1, ROUGE
{
#if DBG_PIN_AUDIO    
 gpio_put(44,1);
#endif 
}

__force_inline void DBG_OFF_1()
{
#if DBG_PIN_AUDIO
 gpio_put(44,0);
#endif    
}


__force_inline void DBG_ON_2()  // PIN 2, Marron
{
#if DBG_PIN_AUDIO    
 gpio_put(45,1);
#endif 
}

__force_inline void DBG_OFF_2()
{
#if DBG_PIN_AUDIO
 gpio_put(45,0);
#endif    
}

__force_inline void DBG_ON_IRQ_SB()
{
#if DBG_PIN_AUDIO    
 gpio_put(42,1);
#endif 
}

__force_inline void DBG_OFF_IRQ_SB()
{
#if DBG_PIN_AUDIO
 gpio_put(42,0);
#endif    
}

__force_inline void DBG_ON_INT_SB()   // Bleue ?
{
#if DBG_PIN_AUDIO
 gpio_put(43,1);
#endif 
}

__force_inline void DBG_OFF_INT_SB()
{
#if DBG_PIN_AUDIO    
 gpio_put(43,0);
#endif 
}

__force_inline void DBG_ON_INT_DMA()   // Verte ?
{
#if DBG_PIN_AUDIO
 gpio_put(46,1);
#endif 
}

__force_inline void DBG_OFF_INT_DMA()
{
#if DBG_PIN_AUDIO    
 gpio_put(46,0);
#endif 
}

__force_inline void DBG_ON_SB_UPD()
{
#if DBG_PIN_AUDIO    
 gpio_put(47,1);
#endif 
}

__force_inline void  DBG_OFF_SB_UPD()
{
#if DBG_PIN_AUDIO    
 gpio_put(47,0);
#endif 
}

__force_inline void DBG_ON_MIX()
{
#if DBG_PIN_AUDIO    
 gpio_put(1,1);
#endif 
}

__force_inline void  DBG_OFF_MIX()
{
#if DBG_PIN_AUDIO    
 gpio_put(1,0);
#endif 
}

#if PM_PRINTF
#define pmprintf printf
#else 
static void fakeprintf(const char *format, ...) {}  // Static means not public
#define pmprintf fakeprintf
#endif

//#if PM_PRINTF
#define PM_ERROR   pmprintf //"PM ERROR"
#define PM_WARNING pmprintf //"PM WARNING"
#define PM_INFO    pmprintf //"PM INFO"
#define PM_INFO2   pmprintf //"PM INFO2"
//#endif