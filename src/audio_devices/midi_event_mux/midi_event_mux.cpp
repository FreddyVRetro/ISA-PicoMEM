

uint8_t midi_in_index=0;
uint8_t midi_in_buff[4];

void USBHostMIDI::rxHandler() {
    // MIDI event handling

    // process MIDI message
    // switch by code index number
    switch (midi_in_buff[0] & 0xf) {
        case 0: // miscellaneous function codes
                        miscellaneousFunctionCode(midi_in_buff[1], midi_in_buff[2], midi_in_buff[3]);
                        break;
        case 1: // cable events
                        cableEvent(midi_in_buff[1], midi_in_buff[2], midi_in_buff[3]);
                        break;
        case 2: // two bytes system common messages 
                        systemCommonTwoBytes(midi_in_buff[1], midi_in_buff[2]);
                        break;
        case 3: // three bytes system common messages 
                        systemCommonThreeBytes(midi_in_buff[1], midi_in_buff[2], midi_in_buff[3]);
                        break;
        case 4: // SysEx starts or continues
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[1];
                        if (sysExBufferPos >= 64) {
                            systemExclusive(sysExBuffer, sysExBufferPos, true);
                            sysExBufferPos = 0;
                        }
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[2];
                        if (sysExBufferPos >= 64) {
                            systemExclusive(sysExBuffer, sysExBufferPos, true);
                            sysExBufferPos = 0;
                        }
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[3];
                        // SysEx continues. don't send
                        break;
        case 5: // SysEx ends with single byte
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[1];
                        systemExclusive(sysExBuffer, sysExBufferPos, false);
                        sysExBufferPos = 0;
                        break;
        case 6: // SysEx ends with two bytes
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[1];
                        if (sysExBufferPos >= 64) {
                            systemExclusive(sysExBuffer, sysExBufferPos, true);
                            sysExBufferPos = 0;
                        }
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[2];
                        systemExclusive(sysExBuffer, sysExBufferPos, false);
                        sysExBufferPos = 0;
                        break;
        case 7: // SysEx ends with three bytes
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[1];
                        if (sysExBufferPos >= 64) {
                            systemExclusive(sysExBuffer, sysExBufferPos, true);
                            sysExBufferPos = 0;
                        }
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[2];
                        if (sysExBufferPos >= 64) {
                            systemExclusive(sysExBuffer, sysExBufferPos, true);
                            sysExBufferPos = 0;
                        }
                        sysExBuffer[sysExBufferPos++] = midi_in_buff[3];
                        systemExclusive(sysExBuffer, sysExBufferPos, false);
                        sysExBufferPos = 0;
                        break;
        case 8:
                        noteOff(midi_in_buff[1] & 0xf, midi_in_buff[2], midi_in_buff[3]);
                        break;
        case 9:
                        if (midi[3]) {
                            noteOn(midi_in_buff[1] & 0xf, midi_in_buff[2], midi_in_buff[3]);
                        } else {
                            noteOff(midi_in_buff[1] & 0xf, midi_in_buff[2], midi_in_buff[3]);
                        }
                        break;
        case 10:
                        polyKeyPress(midi_in_buff[1] & 0xf, midi_in_buff[2], midi_in_buff[3]);
                        break;
        case 11:
                        controlChange(midi_in_buff[1] & 0xf, midi_in_buff[2], midi_in_buff[3]);
                        break;
        case 12:
                        programChange(midi_in_buff[1] & 0xf, midi_in_buff[2]);
                        break;
        case 13:
                        channelPressure(midi_in_buff[1] & 0xf, midi_in_buff[2]);
                        break;
        case 14:
                        pitchBend(midi_in_buff[1] & 0xf, midi_in_buff[2] | (midi_in_buff[3] << 7));
                        break;
        case 15:
                        singleByte(midi_in_buff[1]);
                        break;
    }

}