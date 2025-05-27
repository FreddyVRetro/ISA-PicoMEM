## 2022-11-26 : v1.41
- Fix a problem with DC offset when muting a channel (issue #4).
- Disable frequency limiter if the internal rate converter is not active (i.e. quality == 0).

## 2022-11-25 : v1.40
Breaking Change: ym2149's internal clock divider is disabled by default.
Use PSG_setClockDivider to enable clock divider.

- Fix the bug with the changing period register faster than frequency (issue #3).
- Fix the confusing PSG_setVolumeMode type argument.
- Rename PSG_set_rate to PSG_setRate.
- Rename PSG_set_quality to PSG_setQuality.
- Restore PSG_setClock that was removed on v1.20 for compatibility.
- Add PSG_setClockDivider function.
- Fix noise period at zero frequency.

## 2021-09-29 : v1.30
- Fix some envelope generator problems (issue #2).

## 2016-09-06 : v1.20
- Support per-channel output.

## 2015-12-13 : v1.17
- Changed own integer types to C99 stdint.h types.

## 2004-01-11 : v1.16
- Fixed the envelope problem where the envelope frequency register is written before key-on.

## 2003-09-19 : v1.15
- Added PSG_setMask and PSG_toggleMask.

## 2002-10-13 : v1.14
- Fixed the envelope unit.

## 2002-03-02 : v1.12
- Removed PSG_init & PSG_close.

## 2001-10-03 : v1.11     
- Added PSG_set_quality().

## 2001-08-14 : v1.10
- Bump Version.

## 2001-04-28 : v1.00 beta 
- 1st Beta Release.








