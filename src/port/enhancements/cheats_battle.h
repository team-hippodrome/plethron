// Battle cheat hooks (cheats_battle.c, settings in cheats.h). Each one leaves the game's own behaviour unless a cheat
// is on for the player concerned.
#pragma once
#include "global.h"

struct ActivePokemon;
struct BattleMonRuntime;

void PortCheat_BattleFrame(void);                        // after the battle HUD, every frame: Infinite HP, status
s32 PortCheat_ShieldDamage(struct BattleMonRuntime* target); // damage.c: 1 = the target takes no HP damage
s32 PortCheat_KeepPp(s32 side);                          // Battle_DecrementPP: 1 = no PP is spent
s32 PortCheat_ForceMiss(struct BattleMonRuntime* user, struct BattleMonRuntime* target); // Battle_MoveHitTest
s32 PortCheat_SureHit(struct BattleMonRuntime* user);   // Battle_MoveHitTest: 1 = skip the accuracy roll
s32 PortCheat_MoveOrder(s32 first_side);                 // Battle_StartTurn: the side that acts first
void PortCheat_InitRuntime(struct ActivePokemon* ap);    // end of Battle_InitPokemonRuntimeState: stats
