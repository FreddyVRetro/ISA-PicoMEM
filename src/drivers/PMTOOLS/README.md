# PMinit v0.1.0


## Using

Simply run `PMINIT.EXE` to detect and initialize your card with default
settings. Options may be given for other settings:

### Global options

* `/?` - shows help for PicoGUSinit. Detects the current card mode and only
  shows options valid for that mode.
* `/f firmware.uf2` - uploads firmware in the file named firmware.uf2 to the
  PicoGUS.
* `/j` - enables game port joystick emulation mode (disabled by default).
* `/v x` - sets wavetable header volume to x percent (PicoGUS 2.0 boards only).
  Available in all modes in case you want to use the wavetable header as an aux
  input (for example for an internal CD-ROM drive).

Firmware files that come with the releases:

* `pg-gus.uf2` - GUS emulation
* `pg-sb.uf2` - Sound Blaster/AdLib emulation
* `pg-mpu.uf2` - MPU-401 with intelligent mode emulation
* `pg-tandy.uf2` - Tandy 3-Voice emulation
* `pg-cms.uf2` - CMS/Game Blaster emulation
* `pg-joyex.uf2` - Joystick exclusive mode (doesn't emulate any sound cards,
  just the game port)

## Compiling

PicoGUSinit can be compiled with OpenWatcom 1.9 or 2.0. In DOS with OpenWatcom
installed, run `wmake` to compile.
