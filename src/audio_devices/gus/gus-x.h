#pragma once

#include "dosbox-x-compat.h"

extern void GUS_OnReset(void);
extern Bitu read_gus(Bitu port);
extern void write_gus(Bitu port, Bitu val);
extern uint32_t GUS_CallBack(Bitu len, int16_t* play_buffer);
extern uint8_t GUS_activeChannels(void);
extern uint32_t GUS_basefreq(void);
extern void GUS_SetIRQDMA(uint8_t irq_line, uint8_t dma_channel);
extern void GUS_Setup(void);
void GUS_Shutdown();
