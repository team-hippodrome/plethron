/* Stadium Zero resident code. */

#include "global.h"
#include "m2c_macros.h"
#include "block_relocate.h"

/**
 * Relocates a sound bank against its own address (Audio_RelocateSoundBankWrapper(bank, bank)).
 * @unverified: no callers found.
 */
void Audio_RelocateSoundBankInPlace(SoundBank* bank) { // PORT: bank was s32
    Audio_RelocateSoundBankWrapper(bank, (uintptr_t)bank);
}

/**
 * Relocates a loaded block in place: adds the block's own address to the word at +0x08, to the
 * table offset at +0x0C, and to each of the `count` words of that table, then sets bit 0 of the flags
 * word at +0x04 so it is not relocated twice. Used on sequence and audio data after loading.
 * PORT: 32-bit words cannot hold host addresses. On the host only the (big-endian) header is swapped to host
 * order; `unk08`, `table` and the table's entries stay big-endian offsets from the block, read through the
 * AUDIO_FX_* macros in libnumus/player.h.
 */
void Block_RelocateSelf(RelocHeader8003A710* arg0) {
    Port_SwapWords(arg0, 4);
    arg0->flags |= 1;
}

/**
 * Relocates the sound bank that follows this header against the header's end, marks it relocated
 * (flags bit 0) and returns the bank data. @unverified: no callers found.
 */
u8* Audio_RelocateSoundBankHeader(Unk8003A7AC* arg0) { // PORT: returned s32
    uintptr_t end = arg0->size + (uintptr_t)arg0;

    Audio_RelocateSoundBankWrapper((SoundBank*)arg0->data, end);
    arg0->flags |= 1;
    return arg0->data;
}
