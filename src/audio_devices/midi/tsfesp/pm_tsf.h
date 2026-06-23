TSFDEF int pm_tsf_get_active_voices(tsf* f)
{
    struct tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
    int voicesPlaying = 0;

        for (; v != vEnd; v++)
                if (v->playingPreset != -1) voicesPlaying++;

    return voicesPlaying;
}

void pm_tsf_voice_render_short(tsf* f, struct tsf_voice* v, int32_t* outputBuffer)
{
	TSF_CONST struct tsf_region* region = v->region;
    TSF_CONST short* input = f->shortSamples;
	int32_t* outL = outputBuffer;
	//short* outR = (f->outputmode == TSF_STEREO_UNWEAVED ? outL + numSamples : TSF_NULL);

	// Cache some values, to give them at least some chance of ending up in registers.
	TSF_BOOL updateModEnv = (region->modEnvToPitch || region->modEnvToFilterFc);
	TSF_BOOL updateModLFO = (v->modlfo.deltaF16P16 && (region->modLfoToPitch || region->modLfoToFilterFc || region->modLfoToVolume));
	TSF_BOOL updateVibLFO = (v->viblfo.deltaF16P16 &&  (region->vibLfoToPitch + v->modWheel + v->channelPressure));
//	TSF_BOOL updateModLFO = (v->modlfo.deltaF16P16 && (region->modLfoToPitch || region->modLfoToFilterFc || region->modLfoToVolume));
//	TSF_BOOL updateVibLFO = (v->viblfo.delta && region->vibLfoToPitch);
	TSF_BOOL isLooping    = (v->loopStart < v->loopEnd);

    fixed24p8 tmpLoopStartF24P8 = v->loopStart << 8;
//  fixed24p8 tmpLoopEndF24P8 = v->loopEnd << 8;
//	unsigned int tmpLoopStart = v->loopStart, tmpLoopEnd = v->loopEnd;
    fixed24p8 tmpSampleEndF24P8 = region->end << 8;
    fixed24p8 tmpLoopEndF24P8 = (v->loopEnd + 1) << 8;
//	double tmpSampleEndDbl = (double)region->end, tmpLoopEndDbl = (double)tmpLoopEnd + 1.0;
    fixed24p8 tmpSourceSamplePositionF24P8 = v->sourceSamplePositionF24P8;
//	double tmpSourceSamplePosition = v->sourceSamplePosition;
	//struct tsf_voice_lowpass tmpLowpass = v->lowpass;

//	TSF_BOOL dynamicLowpass = (region->modLfoToFilterFc || region->modEnvToFilterFc);
	float tmpSampleRate = f->outSampleRate; //, tmpInitialFilterFc, tmpModLfoToFilterFc, tmpModEnvToFilterFc;

	TSF_BOOL dynamicPitchRatio = (region->modLfoToPitch || region->modEnvToPitch || (region->vibLfoToPitch + v->modWheel + v->channelPressure));

    fixed16p16 pitchRatioF16P16;
//	double pitchRatio;
	float tmpModLfoToPitchD16, tmpVibLfoToPitchD16, tmpModEnvToPitchD16;

	TSF_BOOL dynamicGain = (region->modLfoToVolume != 0);
	float noteGain = 0, tmpModLfoToVolumeD16;

//	if (dynamicLowpass) tmpInitialFilterFc = (float)region->initialFilterFc, tmpModLfoToFilterFc = (float)region->modLfoToFilterFc, tmpModEnvToFilterFc = (float)region->modEnvToFilterFc;
//	else tmpInitialFilterFc = 0, tmpModLfoToFilterFc = 0, tmpModEnvToFilterFc = 0;

	if (dynamicPitchRatio) pitchRatioF16P16 = 0, tmpModLfoToPitchD16 = (float)region->modLfoToPitch * (1.0f / 65536.0f), tmpVibLfoToPitchD16 = ((float)region->vibLfoToPitch + v->modWheel + v->channelPressure) * (1.0f / 65536.0f), tmpModEnvToPitchD16 = (float)region->modEnvToPitch * (1.0f / 65536.0f);
	else pitchRatioF16P16 = 65536.0f * tsf_timecents2Secsd(v->pitchInputTimecents) * v->pitchOutputFactor, tmpModLfoToPitchD16 = 0, tmpVibLfoToPitchD16 = 0, tmpModEnvToPitchD16 = 0;

	if (dynamicGain) tmpModLfoToVolumeD16 = (float)region->modLfoToVolume * 0.1f * (1.0f / 65536.0f);
	else noteGain = tsf_decibelsToGain(v->noteGainDB), tmpModLfoToVolumeD16 = 0;

    int samples= PM_SAMPLES_PER_BUFFER;
   
    fixed16p16 gainMonoF16P16, gainLeftF16P16, gainRightF16P16;

	if (dynamicPitchRatio)
			pitchRatioF16P16 = 65536.0f * tsf_timecents2Secsd(v->pitchInputTimecents + (v->modlfo.levelF16P16 * tmpModLfoToPitchD16 + v->viblfo.levelF16P16 * tmpVibLfoToPitchD16 + v->modenv.level * tmpModEnvToPitchD16)) * v->pitchOutputFactor;

	if (dynamicGain)
			noteGain = tsf_decibelsToGain(v->noteGainDB + (v->modlfo.levelF16P16 * tmpModLfoToVolumeD16));

//	gainMono = noteGain * v->ampenv.level;
    gainMonoF16P16 = (noteGain * v->ampenv.level) * 65536.0f;

    // Update EG.
	tsf_voice_envelope_process(&v->ampenv, samples, tmpSampleRate);
	if (updateModEnv) tsf_voice_envelope_process(&v->modenv, samples, tmpSampleRate);

	// Update LFOs.
	if (updateModLFO) tsf_voice_lfo_process(&v->modlfo, samples);
	if (updateVibLFO) tsf_voice_lfo_process(&v->viblfo, samples);

//	gainLeft = gainMono * v->panFactorLeft, gainRight = gainMono * v->panFactorRight;
    gainLeftF16P16 = (gainMonoF16P16 * v->panFactorLeftF16P16) >> 16;
    gainRightF16P16 = (gainMonoF16P16 * v->panFactorRightF16P16) >> 16;
	
    while (samples-- && tmpSourceSamplePositionF24P8 < tmpSampleEndF24P8)
				{
					fixed24p8 pos = (unsigned int)tmpSourceSamplePositionF24P8;
//                  fixed24p8 nextPos = (pos >= tmpLoopEndF24P8 && isLooping ? tmpLoopStartF24P8 : pos + (1 << 8));

//					// Simple linear interpolation.
//					float alpha = (float)(tmpSourceSamplePosition - pos), val = (input[pos] * (1.0f - alpha) + input[nextPos] * alpha);
#if 0
					if (display)
					   {
						printf("shortSamples Addr: %p pos: %d end:%d loopS:%d LoopE %d", input, (int)(pos >> 8), region->end, v->loopStart, v->loopEnd);
						for (int di=0; di<8; di++)
						    printf(" %d", input[(pos >> 8)+di]);
						printf("\n");
						display=false;
					   }
#endif 
                    fixed16p16 val = input[pos >> 8];

					// Low-pass filter.
					//if (tmpLowpass.active) val = tsf_voice_lowpass_process(&tmpLowpass, val);

                    fixed16p16 smp;

                    smp = *outL;
                    smp += (val * gainLeftF16P16) >> 16;
                    *outL++ = smp;
                    smp = *outL;
                    smp += (val * gainRightF16P16) >> 16;
                    *outL++ = smp;

					// Next sample.
					tmpSourceSamplePositionF24P8 += pitchRatioF16P16 >> 8;
					if (tmpSourceSamplePositionF24P8 >= tmpLoopEndF24P8 && isLooping) tmpSourceSamplePositionF24P8 -= (tmpLoopEndF24P8 - tmpLoopStartF24P8 + 1);
				}

        // Stop the voice if no loop and finished
		if (tmpSourceSamplePositionF24P8 >= tmpSampleEndF24P8 || v->ampenv.segment == TSF_SEGMENT_DONE)
		{
			tsf_voice_kill(v);
			return;
		}

	v->sourceSamplePositionF24P8 = tmpSourceSamplePositionF24P8;
	//if (tmpLowpass.active || dynamicLowpass) v->lowpass = tmpLowpass;
}

bool pm_tsf_render_short_2x(tsf* f, short* buffer)
{
  bool mixed=false;
  struct tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
  int32_t *buffer32 = (int32_t *)buffer;

  TSF_MEMSET(buffer, 0, 2 * sizeof(short) * PM_SAMPLES_PER_BUFFER * 2 /* We sum in 32bs then downsample here to 16b to minimized saturation calcs */);
  for (; v != vEnd; v++)
         if (v->playingPreset != -1) { 
                  mixed=true;
                  pm_tsf_voice_render_short(f, v, buffer32);
                }
  if (!mixed) return false; // Do the buffer clipping only if something was mixed

  for (int i = 0; i < PM_SAMPLES_PER_BUFFER * 2; i++) {
            int32_t t = buffer32[i];
#ifdef PICO_RP2350
            int16_t saturated;
            asm("ssat %0, #16, %1" : "=r" (saturated) : "r" (t));
            buffer[i] = saturated;
#else
            if (t > 32767) buffer[i] = 32767;
            else if (t < -32768) buffer[i] = -32768;
            else buffer[i] = t;
#endif
          }
  return true;              
}

