// Stadium Zero's cartridge save: 32 KiB of SRAM (pokestadium0 docs/annotation/n64-save-format.md, SaveSram in
// game/src/save_data.h). The file on disk (stadium.sra) is kept in N64 byte order, the same bytes a cartridge or an
// emulator such as ares or mupen64plus holds, so saves move between the port and emulators as they are.
//
// The game itself keeps the image in host order: os_devices.cpp swaps the multi-byte record fields (flags, trainer
// IDs, counts, the GB Player button word, the continue record's GB player ID, and the "POKE" footer) on every SRAM
// transfer. Pokémon, names and items are byte arrays and are never touched. The footer sum is a byte sum, so it is
// the same in either order.
//
// Shared by the game's SRAM transfers (os_devices.cpp), the launcher's import, export and checks
// (launcher_files.cpp, launcher_reliability.cpp) and the save editor (save_editor/stadium_save.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ZeroSram {

constexpr size_t kSize = 0x8000;

// One record kind: `slots` records, each stored twice (primary, then backup = primary + size).
struct RecordKind {
    const char* key; // "teams", "boxes", "item_boxes", "list", "options", "continue"
    uint32_t offset; // SRAM offset of slot 0's primary copy
    uint32_t size;   // one copy, footer included
    int slots;
};
const std::vector<RecordKind>& Kinds();

// The byte order a 32 KiB file was found in.
enum class Order {
    N64,         // big-endian, as the cartridge holds it (ares, mupen64plus, this port)
    WordSwapped, // each 32-bit word reversed (Project64)
    HalfSwapped, // the bytes of each 16-bit half swapped
    HostFields,  // the record fields in little-endian order ("OPEK" footers): saves of earlier port builds
    Blank,       // all 0x00 or all 0xFF: a new chip, the game formats it
};
const char* OrderName(Order order);

// Swaps every multi-byte field that lies wholly inside [sramOffset, sramOffset + size) of `data`, which holds those
// SRAM bytes. Swapping is its own inverse: it turns N64 order into little-endian fields and back.
void SwapFields(uint8_t* data, size_t sramOffset, size_t size);

// What the game transfers: N64 order <-> host order (SwapFields on a little-endian host, nothing on a big-endian one).
void ToHost(uint8_t* data, size_t sramOffset, size_t size);
void ToN64(uint8_t* data, size_t sramOffset, size_t size);

// Copies of records with a valid "POKE" footer and sum, read in N64 order.
int ValidCopies(const uint8_t* image);

// Finds the byte order of a 32 KiB save and gives it back in N64 order. False (and `error`) when the file is not
// 32 KiB or no order gives an unambiguous set of valid records. Invalid records are kept as they are: the game
// repairs them from their other copy, or resets them, when it loads the save.
bool Normalize(const std::vector<uint8_t>& source, std::vector<uint8_t>& normalized, Order* order, std::string& error);

// The side file beside the save that keeps Western nicknames and OT names (gb_save_convert.h): "<save>.western".
std::string WesternSidePath(const std::string& savePath);

} // namespace ZeroSram
