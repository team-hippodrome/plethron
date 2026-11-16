/*
 * Stadium Zero fragment038_38A540: models of the Game Boy cartridge check scene (fragment006),
 * i.e. the P-OS terminal desk (geo layout D_8F4886F8) and the three per-cartridge models, plus
 * the anti-piracy check hidden in the terminal's switch-node callback.
 */

#include "global.h"
#include "m2c_macros.h"
#include "cartridge_check_models.h"
#include "PR/rcp.h"

// PORT: the cartridge bus is the loaded ROM image (os_devices.cpp)
const u8* Port_RomBytes(void);
size_t Port_RomLength(void);

// PORT: the low half of the big-endian ROM word at `offset`, which the N64 read over the PI bus.
static u32 Port_ReadCartHalfLow(u32 offset) {
    const u8* rom = Port_RomBytes();
    return (rom != NULL && offset + 4 <= Port_RomLength()) ? ((rom[offset + 2] << 8) | rom[offset + 3]) : 0;
}

/**
 * Anti-piracy check. With interrupts masked and the PI idle, reads the cartridge ROM word at
 * 0x504 (inside the IPL3 boot code) straight over the bus. A genuine boot code has
 * `lui at, 0xA460` there; if the low half is anything else, returns -1 instead of `value`.
 * Seen in play (2026-11-14): stubbing it to return -1 froze the cartridge check screen
 * (pressing A no longer reached the "P-OS version 1.0 NOW LOADING" screen).
 * Stadium 1's counterpart is Pak_CheckPiIntegrity.
 */
intptr_t CartridgeCheckModels_CheckBootCode(intptr_t value) { // PORT: the value is a child-list pointer
    OSIntMask prev_mask;

    prev_mask = osSetIntMask(OS_IM_NONE);
    if (!(IO_READ(PI_STATUS_REG) & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) &&
        (Port_ReadCartHalfLow(0x504) != 0xA460)) { // PORT: was IO_READ(PHYS_TO_K1(PI_DOM1_ADDR2) + 0x504) & 0xFFFF
        value = -1;
    }
    osSetIntMask(prev_mask);
    return value;
}

extern DisplayObject* D_8006049C;

/**
 * Geo callback of the P-OS terminal's switch node (in D_8F4886F8). On traversal (context 2) it
 * selects the child given by the rendered display object's callbackArg: fragment006 sets it to 1
 * when the terminal appears. Play (2026-11-14): poking the object's callbackArg to 1 showed the
 * terminal desk, 0 and 2 showed nothing.
 * Before that, the node's child list goes through CartridgeCheckModels_CheckBootCode, so on a
 * copy with a foreign boot code the list becomes -1 and the game hangs walking it.
 */
void CartridgeCheckModels_GeoSwitchTerminal(s32 context, GeoSwitchCaseNode* node) {
    if (context == 2) {
        node->node.children = (struct CameraNode*)CartridgeCheckModels_CheckBootCode((uintptr_t)node->node.children);
        node->selectedChild = (s16) D_8006049C->node.callbackArg;
    }
}
