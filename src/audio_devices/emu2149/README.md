emu2149
=======

A YM2149 (aka PSG) emulator written in C.

# Breaking Change on v1.40
- Internal clock divider is disabled by default. Use `PSG_setClockDivider(psg, 1)` to enable clock divider.

```
/* Typical YM2149 configuration */
PSG *psg = PSG_new(3579545, 44100);
PSG_setClockDivider(psg, 1);
PSG_setVolumeMode(psg, 1); // YM style
PSG_reset();

/* Typical AY-3-8910 configuration */
PSG *psg = PSG_new(3579545/2, 44100);
PSG_setVolumeMode(psg, 2); // AY style
PSG_reset();
```

# Note for rate conversion

The internal sample rate converter in emu2149 is a light-weight, simplified procedure.
Even if `1` is set to the quality (PSG_setQuality), the alias noise cannot be completely eliminated.

To obtain the best accurate result, an external sample rate conversion should be used. 
Please follow these steps.

1. Set the sample rate to 1/8 of the chip's master clock. i.e. rate=223721Hz for master=1.78MHz.
2. Set `0` to the quality value with `PSG_setQuality(psg, 0)` so that you can bypass the internal rate converter.
3. Call `PSG_calc()` to generate samples.
4. Apply your preferred external rate converter to the samples to obtain your desired sample rate.