# Return-value audit (functions that leave their result in v0)

The decomp matches IDO code where a function has no return statement, or is defined `void`, while its callers use the
result. On the N64 the caller reads `v0`, which still holds what the callee's last call left there. On the PC the value
is whatever the compiler left in the return register: it happens to be right in an optimised build when the last call is
a tail call, and garbage otherwise (debug builds, or any code after the call). `BattleCalc_EstimateDamage` (PR #36) made
the COM pick different moves this way.

`tools/check_return_values.py` finds the whole class across every game file (prototypes in other files and the
`func_X` aliases resolved) and runs in the Checks workflow. Its report lists each function, every call site and whether
the site uses the value.

## Fixed (callers use the value)

Each one now returns what the matched N64 code left in `v0` (read from the decomp's linked images), and is marked
`// PORT: v0`.

| Function | Shape | Callers that read it | N64 `v0` at return |
| --- | --- | --- | --- |
| `FreeBattle_RunFrame` | void, passed as an `s32 (*)(u8)` | `BgStage_RunUntilCondition` loops until it is nonzero | `FreeBattle_UpdateScreen`'s result |
| `PartyScreen_RunFrame` | void, cast to `s32 (*)(u8)` | `BgStage_RunUntilCondition` loops until it is nonzero | `PartyScreen_Update`'s result |
| `Particle_ApproachPrimAlpha` | void, declared `s32` | 27 call sites in move effects (fades end when it reports arrival) | `ParticleMath_ApproachU8`'s result |
| `Particle_ApproachPrim{Red,Green,Blue}`, `Particle_ApproachEnv{Red,Green,Blue}` | void, declared `s32` | `ParticleGfx_Approach*Palette` | `ParticleMath_ApproachU8`'s result |
| `Particle_ApproachScratchA` | void, declared `s32` | tint fades, `BattleAnim_RunUserHueChange` | `ParticleMath_StepS16Toward`'s result |
| `ParticleMath_ComputeViewRotationY` | void, declared `s32` | Confuse Ray particles (face the camera) | `MathUtil_Atan2s`'s angle |
| `Audio_SetMonoOutput` | void, declared `s32` | `Audio_PlayCommand` | `Audio_QueueSetMonoOutputCommand`'s result |
| `AudioThread_ScheduleProcessCmds` | no return | `AudioThread_ResetAudioHeap` | `osSendMesg`'s result |
| `AudioLoad_SyncLoadSeq` | no return | `AudioLoad_SyncInitSeqPlayerInternal` (sequence data) | `AudioLoad_SyncLoad`'s result |
| `AudioLoad_SyncInitSeqPlayerInternal` | no return on success | `AudioLoad_SyncInitSeqPlayer{,SkipTicks}` | 16 (the empty loop's counter) |
| `GbApu_GetWaveformSample` | no return for types 4, 5, 8+ | the four GB APU channels | the caller's `v0`; unreachable (duty is 2 bits), returns 0 |
| `BattleCalc_EstimateDamage` | void, declared `u16` (PR #36) | COM AI | `BattleAi_ApplyMoveEffectDamageOverride`'s result |

The two `AudioLoad` functions and the `GbApu` path never run in Zero; they are fixed so the class is closed.

## Left as the decomp has them

95 functions have no return value but no caller reads it (the 30 remaining `-Wreturn-type` warnings among them). The
check lists them as harmless and fails as soon as a new caller uses one. Calls through function pointers that use the
result were checked by hand: fragment entries (every entry returns a value, `FatalError_Main` never returns), geo node
callbacks (the result goes to `D_800AB244`, which nothing reads), battle effect update callbacks (registered through a
typed slot), sort comparators, list filters and stage conditions (passed in code, so the check sees them), and the
libultra/libnaudio dispatchers.

## Running it

```sh
git submodule update --init libultraship   # once: the check needs libultraship's headers
python3 -I tools/check_return_values.py --report build/return-values.txt
```

It takes about a minute on 8 cores. To read what `v0` held for a new case, build the decomp (`make init && make rom` in
pokestadium0) and disassemble the function from `build/production/*/fragmentNNN.elf` or `resident/resident.elf`.
