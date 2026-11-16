/* Declarations for src/rom_device.c. */
#ifndef ROM_DEVICE_H
#define ROM_DEVICE_H

#include "global.h"

// rom_device.c
void Rom_InitDevice(s32);
void Rom_DmaRead(u32 dev_addr, uintptr_t dram_addr, u32 size); // PORT: dram_addr is a host pointer
void Audio_SetChannelVolume(s32 arg0, u32 arg1);

#endif // ROM_DEVICE_H
