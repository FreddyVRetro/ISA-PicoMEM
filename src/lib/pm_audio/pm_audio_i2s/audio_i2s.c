/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico.h"   // uint16_t def and so on

#include "../../../pm_debug.h"
#include "pm_audio.h"
#include "audio_i2s.h"
#include "audio_i2s.pio.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"


CU_REGISTER_DEBUG_PINS(audio_timing)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(audio_timing)


#if PICO_AUDIO_I2S_MONO_OUTPUT
#define i2s_dma_configure_size DMA_SIZE_16
#else
#define i2s_dma_configure_size DMA_SIZE_32
#endif

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_I2S_PIO)
#define DREQ_PIOx_TX0 __CONCAT(__CONCAT(DREQ_PIO, PICO_AUDIO_I2S_PIO), _TX0)

static void __isr __time_critical_func(audio_i2s_dma_irq_handler)();

struct {
    uint8_t *playing_buffer;
    uint32_t freq;
    uint8_t pio_sm;
    uint8_t dma_channel;
} shared_state;

static void audio_i2s_update_frequency(uint32_t sample_freq) {
    PM_INFO("Audio Init : Set PIO Freq: %d PIO: %d\n",sample_freq,audio_pio);

    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / sample_freq; // avoid arithmetic overflow
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(audio_pio, shared_state.pio_sm, divider >> 8u, divider & 0xffu);
    shared_state.freq = sample_freq;
}

bool audio_i2s_setup(const audio_i2s_config_t *config) {

    PM_INFO("Audio Init : audio_i2s_setup\n");

    uint func = GPIO_FUNC_PIOx;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    uint8_t sm = shared_state.pio_sm = config->pio_sm;      // Save the pio sm number
    //spi.sm = pio_claim_unused_sm(spi.pio, true);          // > To do, auto define state machine
    pio_sm_claim(audio_pio, sm);

    uint offset = pio_add_program(audio_pio, &audio_i2s_program);
    audio_i2s_program_init(audio_pio, sm, offset, config->data_pin, config->clock_pin_base);
    audio_i2s_update_frequency(PM_AUDIO_FREQUENCY);

    PM_INFO("I2S Claimed DMA :");

    uint8_t dma_channel; // = config->dma_channel;
    dma_channel=dma_claim_unused_channel(true);
    shared_state.dma_channel = dma_channel;     // Save the DMA Channel number
    shared_state.pio_sm = sm;                   // Save the pio sm number

    PM_INFO(" %d\n, PIO SM:%d",dma_channel,sm);

    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);

    channel_config_set_dreq(&dma_config,
                            DREQ_PIOx_TX0 + sm
    );
    channel_config_set_transfer_data_size(&dma_config, i2s_dma_configure_size);
    dma_channel_configure(dma_channel,
                          &dma_config,
                          &audio_pio->txf[sm],  // dest
                          NULL, // src
                          0, // count
                          false // trigger
    );

    /* Configure the DMA Interrupt */

    PM_INFO("DMA IRQ: %d\n",DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ);

    irq_add_shared_handler(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, audio_i2s_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);    
    dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel, 1);

// Force the DMA IRQ in high priority, if not correct, the sound will cut //
    irq_set_priority(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, PICO_HIGHEST_IRQ_PRIORITY);

    PM_INFO("Done\n");

    return true;
}

static inline void audio_start_dma_transfer() {
    
    uint8_t *ab = take_mixed_buffer();  // Return an Empty buffer if no more mixed buffer.

    dma_channel_config c = dma_get_channel_config(shared_state.dma_channel);
    channel_config_set_read_increment(&c, true);
    dma_channel_set_config(shared_state.dma_channel, &c, false);
    dma_channel_transfer_from_buffer_now(shared_state.dma_channel, ab, PM_SAMPLES_PER_BUFFER );  // Table, Nb de bytes
}

// irq handler for DMA
void __isr __time_critical_func(audio_i2s_dma_irq_handler)() {
#if PICO_AUDIO_I2S_NOOP
    assert(false);
#else
//    gpio_put(21,1);  
    uint dma_channel = shared_state.dma_channel;
    if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_channel)) {
        dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel);
 //       DEBUG_PINS_SET(audio_timing, 4);

        // free the buffer we just finished   *** To Check

        audio_start_dma_transfer();
       // printf("-PB:%x %x ",((uint32_t)pm_audio.playing_buffer)&0x0FFF,pm_audio.mixed_buffers);
//        DEBUG_PINS_CLR(audio_timing, 4);
    }
//    gpio_put(21,0);      
#endif
}

static bool audio_enabled;

//Enable / Disable the Audio output (Not the Mixing IRQ)
//Done via Stopping DMA IRQ and PIO State machine

void audio_i2s_set_enabled(bool enabled) {
  if (enabled != audio_enabled) {

//Enable/Disable IRQ > IRQ Disable will stop the Audio as DMA Will stop at the end of the buffer
        irq_set_enabled(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, enabled);

        if (enabled) {
            PM_INFO("Enabling PIO I2S audio (on core %d)\n", get_core_num());          
            audio_start_dma_transfer();
        } else {
            PM_INFO("Audio Init : DMA IRQ already enabled\n");            
            // if there was a buffer in flight, it will not be freed by DMA IRQ, let's do it manually 
            pm_audio.playing_buffer = pm_audio.poolend;
            }
        }
 pio_sm_set_enabled(audio_pio, shared_state.pio_sm, enabled);
 audio_enabled = enabled;

}


