# DOSBox Disk Library, Adapted for the PicoMEM Board

Original code from DOSBOX(www.dosbox.com) and small portion from DOSBOX-X With some bug corrected
(Some Int13h functions added)

The BIOS int13h is executed from the Pi Pico.
The PicoMEM BIOS save the registers in the BIOS Shared memory Area and ask the PicoMEM to execute the Int13h emulation code.