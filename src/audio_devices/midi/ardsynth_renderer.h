#ifndef PICOMEM_ARDSYNTH_RENDERER_H
#define PICOMEM_ARDSYNTH_RENDERER_H

#include <stdint.h>

void ardsynth_renderer_init();
void ardsynth_renderer_close();
void ardsynth_renderer_message(uint8_t status, uint8_t data1, uint8_t data2);
void ardsynth_renderer_render(int16_t* buffer, uint16_t samples);
int ardsynth_renderer_active_voices();

#endif
