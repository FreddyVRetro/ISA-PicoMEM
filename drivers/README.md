# ISA PicoMEM Extension board (For 8086/8088 PC)

## Introduction
This folder contaons the drivers needed for the PicoMEM

## NE2000.COM

ne2000 TCP Driver for DOS, to be used with Mtcp

command line : ne2000 0x60 0x3 0x300

The IRQ can be changed in the BIOS Menu (Default is IRQ 3)

## PM2000.COM

Same driver as NE2000 for the PicoMEM : IRQ and Port are Auto detected.
Does not start if the PicoMEM is not detected

03/03/2023 : Removed part of the Copyright text to reduce the display length

## PMMOUSE.COM

PicoMEM Mouse driver is a modified CTMouse driver.
It can detect the PicoMEM and does not need any parameter (IRQ is Auto detected)