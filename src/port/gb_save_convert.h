// Western (English, German, French, Italian, Spanish) Pokémon Red/Blue/Yellow saves shown to Zero in the Japanese
// layout it reads. Zero only knows the Japanese Gen 1 save: 5-letter names, 8 PC boxes of 30. Western saves have
// 10-letter names and 12 boxes of 20, and everything after the player name sits 5 or 10 bytes later. The Transfer
// Pak (transfer_pak.cpp) keeps the cartridge's own battery RAM and serves Zero a Japanese copy made from it; what
// Zero writes to the copy is carried back into the Western save.
//
// Boxes are packed in order: the 12 Western boxes, read box by box, fill the 8 Japanese boxes 30 at a time. When
// Zero changes the boxes, each Pokémon goes back to the Western box it came from (matched by its record and names),
// and new ones to the first box with room.
//
// Names: letters the Japanese character set lacks get unused Japanese codes, which Port_GbCharsetExtend adds to
// Zero's character table as full-width Latin letters; accented letters lose their accent. A name Zero did not change
// keeps its original Western bytes. A nickname that is the species name becomes the species' Japanese name, so it
// is shown (and translated) like a Japanese cartridge's.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace GbSaveConvert {

// Whether `rom` is a Western Gen 1 Pokémon game (destination code 1, "POKEMON RED/BLUE/YEL..." title).
bool IsWesternGen1(const std::vector<uint8_t>& rom);

struct Context {
    std::vector<std::vector<uint8_t>> speciesNames; // Western name of each internal species index (1-190), GB codes
    // original Western names of the Pokémon in the copy, by the record's identity and its Japanese names
    std::multimap<std::string, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> names; // -> (nickname, OT)
    std::vector<uint8_t> base; // the Japanese copy as last made or synced, to see what Zero changed
};

// Reads the species name table from the cartridge ROM (searched for by its first entry).
void Init(Context& ctx, const std::vector<uint8_t>& rom);

// Makes the Japanese copy `jp` (same size as `west`) from the Western battery RAM.
void WestToJp(Context& ctx, const std::vector<uint8_t>& west, std::vector<uint8_t>& jp);

// Carries what Zero changed in `jp` (compared with ctx.base) into `west`, then ctx.base = jp. Returns whether
// `west` changed.
bool JpToWest(Context& ctx, const std::vector<uint8_t>& jp, std::vector<uint8_t>& west);

} // namespace GbSaveConvert

extern "C" void Port_GbCharsetExtend(uint16_t* table); // the full-width Latin letters, into Zero's GB->EUC table
