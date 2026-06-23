/*
    Envelope Generator
    https://github.com/DLehenbauer/arduino-midi-sound-module
    
    Envelope generator used for amplitude, frequency, and wavetable offset modulation.
*/

#ifndef __ENVELOPE_H__
#define __ENVELOPE_H__

#include <stdint.h>
#include "instruments.h"

class Envelope {
  private:
    const EnvelopeStage* pFirstStage = nullptr;     // Pointer to first stage in 'Instruments::EnvelopeStages[]'
    uint8_t loopStart   = 0xFF;                     // Index of loop start
    uint8_t loopEnd     = 0xFF;                     // Index of loop end / release start
    uint8_t stageIndex  = 0xFF;                     // Current stage index
    int16_t value   = 0;                            // Current Q8.8 fixed-point value
    int16_t slope = 0;                              // Current Q8.8 slope (added to current value at each sample())
    int8_t  limit = -64;                            // Value limit at which envelope will advance to next stage
  
    // Updates 'slope' and 'limit' with the value of the current 'stageIndex'.
    void loadStage() volatile {
      EnvelopeStage stage;
      Instruments::getEnvelopeStage(pFirstStage + stageIndex, /* out: */ stage);
      slope = stage.slope;
      limit = stage.limit;
    }
  
  public:
    // Each call to 'sample()' advances the envelope generator to it's next state, and returns
    // a value in the range [0 .. 127].
    uint8_t sample() volatile {
      value += slope;                       // Increase/decrease the current value according to the slope.
      int8_t out = value >> 8;              // Convert Q8.8 fixed point value to uint8_t output

      const bool nextStage = (out < 0) ||   // Advance to next stage if the slope has overflowed
        (slope <= 0)                        // Otherwise, advance to next stage if...
          ? out <= limit                    //   decreasing value is below or equal to limit
          : out >= limit;                   //   or increasing value is above or equal to limit
    
      if (nextStage) {                      // If advancing to next stage...
        out = limit;                        //   clamp output to the limit
        value = limit << 8;                 //   clamp value to limit
        stageIndex++;                       //   advance to next envelope stage
        if (stageIndex == loopEnd) {        //   If next stage is end of loop...
          stageIndex = loopStart;           //     jump to start of loop
        }
        loadStage();                        //   Load the slope/limit for the new stage
      }
    
      return out;                           // Return the new value
    }

    // 'start()' is called by 'Synth::noteOn()' to intialize the envelope generators with the
    /// program from 'Instruments::EnvelopePrograms' at the given 'programIndex'.
    void start(uint8_t programIndex) volatile {
      EnvelopeProgram program;
      Instruments::getEnvelopeProgram(programIndex, program);
    
      pFirstStage = program.start;
      loopStart = program.loopStartAndEnd >> 4;
      loopEnd = program.loopStartAndEnd & 0x0F;
      value = program.initialValue << 8;
      stageIndex = 0;
    
      loadStage();
    }

    // 'stop()' is called by 'Synth::noteOff()' to advance the the envelope generator to the
    // 'loopEnd' stage, if it has not already passed it.
    // 
    // Note: The generator will not loop back to 'loopStart', but continue advancing until it
    //       reaches a stage than never completes, such as { slope: 0, limit: -64 }.
    void stop() volatile {
      if (stageIndex < loopEnd) {
        stageIndex = loopEnd;
        loadStage();
      }
    }

    // Allow Synth::getNextVoice() to inspect private state when choosing the next best voice.
    friend class Synth;
  
  #ifdef __EMSCRIPTEN__
    uint8_t sampleEm()            { return sample(); }
    void startEm(uint8_t program) { start(program); }
    void stopEm()                 { stop(); }
    uint8_t getStageIndex()       { return stageIndex; }
  #endif // __EMSCRIPTEN__
};

#endif //__ENVELOPE_H__