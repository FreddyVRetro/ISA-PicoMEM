#ifndef _VMPU_H_
#define _VMPU_H_

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out);
void VMPU_SBMidi_RawWrite( uint8_t value );

#ifdef __cplusplus
}
#endif

#endif//_VMPU_H_
