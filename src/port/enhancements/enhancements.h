// The port's enhancements from hippodrome-utils (the team's shared library, the hippodrome-utils submodule; specs in
// hippodrome-utils/docs/specs): side summary bars, speed stat estimates and trainer memory. All of them are options,
// off by default, so the game plays and looks as on the N64 unless they are turned on (launcher > Enhancements).
//
// enhancements.cpp keeps the settings and the trainer memory file; battle_enhancements.c and memory_screen.c are
// compiled with the game code (they read the battle and session structs) and call into it.
#pragma once
#include <stdint.h>

#include <hippodrome/summary_card.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hu_tm;

// Settings (port settings gEnhancements.*, kept by the port, not in the N64 save). PORT_SUMMARY_BARS=0/1 and
// PORT_TRAINER_MEMORY=0/1 override them (test aids).
int Port_Enh_SummaryBarsEnabled(void);
void Port_Enh_SetSummaryBarsEnabled(int on);
int Port_Enh_TrainerMemoryEnabled(void);
void Port_Enh_SetTrainerMemoryEnabled(int on);

// The trainer memory store, loaded from <save>.memory beside the N64 save on first use (an empty store if there is
// none yet or it is damaged; a damaged file is kept as <save>.memory.bad). Never NULL.
struct hu_tm* Port_Enh_Memory(void);
// What the summary bars show, published by the battle each frame it draws its HUD (battle_enhancements.c) and drawn
// by summary_bars.cpp outside the game's picture. The strings are UTF-8 and stay valid for the whole run.
typedef struct PortEnhBattleView {
    hu_side sides[2]; // 0 = 1P (left), 1 = the opponent (right)
    const char* names[2][HU_SIDE_MAX];
    const char* moves[2][HU_SIDE_MAX][HU_CARD_MOVES];
    uint8_t moveTypes[2][HU_SIDE_MAX][HU_CARD_MOVES]; // hu_type, for the move names' colours
    hu_speed_range speed[2][HU_SIDE_MAX]; // as shown: the active one's with its stage and paralysis
    int32_t viewer;                       // the side whose own Pokémon are known (0 against the COM), -1 if none
} PortEnhBattleView;
void Port_Enh_PublishBattle(const PortEnhBattleView* view);
// The last view if one was published in the last few frames (a battle is on screen); 0 otherwise.
int Port_Enh_LatestBattle(PortEnhBattleView* out);

// Adds the summary bars' font to ImGui (before its first frame builds the atlas: Engine.cpp, after the window's Init).
void Port_Enh_LoadFonts(void);

// Writes the store if it changed since the last load or save.
void Port_Enh_SaveMemory(void);
// The file's path (<save>.memory).
const char* Port_Enh_MemoryPath(void);

#ifdef __cplusplus
}
#endif
