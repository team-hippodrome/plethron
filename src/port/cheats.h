// Cheats (GitHub issue #97), set in the launcher's Cheats panel (the button under Save editor, shown with Debug on, or
// whenever a cheat is set, so one is never on unseen; Files & help > Cheats on handhelds). All of them are off by
// default, so the game plays as on the N64. They are port settings (gCheats.*, in the port's config file), never part
// of the N64 save.
//
// Battle cheats work on Stadium's battles and are set per player (1P-4P, by controller port): each one acts on the
// Pokémon of the players it is ticked for, or, for "Opponent ...", on what their opponents do. They change the running
// battle only (cheats_battle.c): nothing is written to the N64 save or to a Game Boy cartridge.
//
// Game Boy cheats are GameShark codes (01VVLLHH: write VV to Game Boy address HHLL) for the cartridge in each Transfer
// Pak, applied at the end of every Game Boy frame while the GB Player runs it (gb_core.c). Only work RAM (C000-DFFF) is
// accepted, never the cartridge's own save RAM, but whatever they put in work RAM is kept in the cartridge save if the
// game is saved in the Game Boy game.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum PortCheat {
    PORT_CHEAT_INFINITE_HP,     // your Pokémon take no damage and are kept at full HP
    PORT_CHEAT_INFINITE_PP,     // your moves use no PP
    PORT_CHEAT_NO_STATUS,       // your Pokémon lose any status condition, confusion and bad poison at once
    PORT_CHEAT_NEVER_MISS,      // your moves skip the accuracy roll
    PORT_CHEAT_FOE_NEVER_HITS,  // every move aimed at your Pokémon misses
    PORT_CHEAT_MOVE_FIRST,      // you act first every turn
    PORT_CHEAT_PERFECT_STATS,   // your Pokémon battle with perfect DVs and stat experience (Attack/Defense/Speed/Special)
    PORT_CHEAT_FOE_MIN_STATS,   // your opponents' Pokémon battle with zero DVs and stat experience
    PORT_CHEAT_COUNT
};

#define PORT_CHEAT_GB_MAX_CODES 32

// Players (bit 0 = 1P ... bit 3 = 4P) a battle cheat is on for.
int Port_CheatPlayers(int cheat);
// Applies the GameShark codes set for the Transfer Pak / controller port (0-3) to the Game Boy memory block (the Game
// Boy address space at mem + address). Does nothing when no code is set.
void Port_GbCheatsApply(uint8_t* mem, int port);

#ifdef __cplusplus
}

#include <string>
#include <vector>

namespace Port {
struct CheatInfo {
    const char* key;   // gCheats.<key>: player mask
    const char* label; // launcher row
    const char* hint;  // launcher tooltip
};
const CheatInfo& Cheat(int cheat);
// gCheats.Gb<port+1>.Codes: the codes as typed
std::string GbCheatKey(int port);
struct GbCheatCode {
    uint16_t address;
    uint8_t value;
};
// Parses GameShark codes separated by spaces, commas or new lines. Codes that are not 01VVLLHH with an address in work
// RAM (C000-DFFF) are left out and listed in errors.
std::vector<GbCheatCode> ParseGbCheats(const std::string& text, std::vector<std::string>* errors = nullptr);
// Turns every cheat off (battle and Game Boy).
void ClearCheats();
// Any cheat set at all.
bool AnyCheatSet();
} // namespace Port
#endif
