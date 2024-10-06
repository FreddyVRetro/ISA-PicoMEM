#pragma once

#include <stdint.h>

#include "pico/time.h"
#include "hardware/gpio.h"

#ifdef DOSBOX_STAGING
#include "dosboxcompat.h"
#else
#include "dosbox-x-compat.h"
#endif

// PicoGUS Timer code only
#define IRQ_PIN 21 // TODO don't spread around pin definitions like this

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (* PIC_EventHandler)(Bitu val);

typedef struct {
    PIC_EventHandler handler;
    Bitu value;
#ifdef USE_ALARM
    alarm_id_t alarm_id;
#else
    uint32_t deadline;
    bool active;
#endif
} PIC_TimerEvent;

extern PIC_TimerEvent timerEvents[3];

extern alarm_pool_t* alarm_pool;

int64_t PIC_HandleEvent(alarm_id_t id, void *user_data);

int64_t clear_irq(alarm_id_t id, void *user_data);

static __force_inline void PIC_ActivateIRQ(void) {
    // puts("activate irq");
    gpio_put(IRQ_PIN, 1); 
    // alarm_pool_add_alarm_in_us(alarm_pool, 500, clear_irq, 0, true);
}

static __force_inline void PIC_DeActivateIRQ(void) {
    gpio_put(IRQ_PIN, 0); 
}

// void PIC_AddEvent(PIC_EventHandler handler, uint32_t delay, Bitu val=0);
// delay in us
static __force_inline void PIC_AddEvent(PIC_EventHandler handler, uint32_t delay, Bitu val) {
    // printf("add event: %x %x %d\n", handler, val, delay);
    timerEvents[val].handler = handler;
    timerEvents[val].value = val;
#ifdef USE_ALARM
    // timerEvents[val].alarm_id = add_alarm_in_us(delay, PIC_HandleEvent, timerEvents + val, true);
    // alarm_pool_cancel_alarm(alarm_pool, timerEvents[val].alarm_id);
    timerEvents[val].alarm_id = alarm_pool_add_alarm_in_us(alarm_pool, delay, PIC_HandleEvent, timerEvents + val, true);
#else
    timerEvents[val].deadline = time_us_32() + delay;
    timerEvents[val].active = true;
#endif
    // gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

void PIC_RemoveEvents(PIC_EventHandler handler);

void PIC_Init(void);

#ifndef USE_ALARM
static __force_inline void PIC_HandleEvents() {
    for (int i = 0; i < 4; ++i) {
        if (timerEvents[i].active && timerEvents[i].deadline <= time_us_32()) {
            PIC_HandleEvent(0, &timerEvents[i]);
        }
    }
}
#endif

#ifdef __cplusplus
}
#endif
