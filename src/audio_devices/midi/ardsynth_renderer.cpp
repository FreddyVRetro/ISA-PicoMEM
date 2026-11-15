#include "ardsynth_renderer.h"
#include "ardsynth/midisynth.h"

static MidiSynth ardsynth;
static bool ardsynth_initialized = false;

void ardsynth_renderer_init()
{
  if (!ardsynth_initialized) {
    ardsynth.begin();
    ardsynth_initialized = true;
  }
}

void ardsynth_renderer_close()
{
  ardsynth_initialized = false;
}

void ardsynth_renderer_message(uint8_t status, uint8_t data1, uint8_t data2)
{
  if (!ardsynth_initialized) return;

  const uint8_t channel = status & 0x0F;
  switch (status & 0xF0) {
    case 0x80:
      ardsynth.midiNoteOff(channel, data1);
      break;
    case 0x90:
      if (data2 == 0) ardsynth.midiNoteOff(channel, data1);
      else ardsynth.midiNoteOn(channel, data1, data2);
      break;
    case 0xB0:
      ardsynth.midiControlChange(channel, data1, data2);
      break;
    case 0xC0:
      ardsynth.midiProgramChange(channel, data1);
      break;
    case 0xD0:
      break;
    case 0xE0: {
      const int16_t bend = static_cast<int16_t>((data1 | (data2 << 7)) - 8192);
      ardsynth.midiPitchBend(channel, bend);
      break;
    }
    default:
      break;
  }
}

void ardsynth_renderer_render(int16_t* buffer, uint16_t samples)
{
  if (!ardsynth_initialized) return;
  ardsynth.render(buffer, samples);
}

int ardsynth_renderer_active_voices()
{
  if (!ardsynth_initialized) return 0;
  int active = 0;
  for (uint8_t voice = 0; voice < MidiSynth::numVoices; voice++) {
    if (ardsynth.getAmp(voice) != 0) active++;
  }
  return active;
}
