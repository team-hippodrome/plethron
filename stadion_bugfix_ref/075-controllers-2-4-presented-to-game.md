# Only controller 1 was presented to the game (every multiplayer choice greyed out)

- **Category:** input
- **Zero status:** needed (port side inherited, but Zero's `Cont_SetupControllers` does not use it)
- **stadion:** [PR #75](https://github.com/team-hippodrome/stadion/pull/75), commit(s) `bacbf7e`
- **stadion files:** `src/port/os_devices.cpp`, `src/port/launcher.cpp`, `src/port/sdk/port_compat.h`, `patches/game/02-core.patch` (`src/controller.c`)

## Symptom
Free Battle with 2-4 players, team battles (1P & 2P vs COM) and multiplayer minigames were greyed out.

## Root cause
libultraship's `ControlDeck::Init` sets only bit 0 of the controller bits `osContInit` returns; nothing set the others, and the game reads the bits once at boot.

## Fix in stadion
`Port_ControllerBits()`: controller 1 always; 2-4 when the launcher's Controls panel "Plugged in" is on (`gPort.Controller<N>.PluggedIn`), `PORT_CONTROLLERS=1,2`, or a replay's `# controllers 1 2` line. The game takes its bits from it:
```c
osContInit(&gSIEventMesgQueue, &gControllerBits, &gControllerStatuses[0]);
gControllerBits = Port_ControllerBits(); // PORT: libultraship reports only controller 1
```

## Zero / plethron
Port side present: `src/port/os_devices.cpp:718-750` (`Port_ControllerBits`), recording line at `:827-830`, launcher check at `src/port/launcher.cpp:1232-1237`, declared in `src/port/StadionPort.h:79`. Game side missing: `game/src/controller.c:125-133` (`Cont_SetupControllers`) calls `osContInit(&gSIEventMesgQueue, &D_800AAFF8, ...)` and returns `D_800AAFF8` without overriding it; `port_osContInit` (`src/port/os_devices.cpp:488-501`) passes libultraship's bits through. `grep -rn Port_ControllerBits game/` is empty. Zero reads the mask via `Cont_GetConnectedMask` (`controller.c:181`) and Free Battle keeps a `connectedControllerMask` (`free_battle_setup.h:161`). Fix: `D_800AAFF8 = Port_ControllerBits();` after `osContInit` at `controller.c:130`.

## How to detect
grep the game for the `osContInit` call and check what writes the bits afterwards; at runtime, the Free Battle player-count options with `PORT_CONTROLLERS=1,2`.
