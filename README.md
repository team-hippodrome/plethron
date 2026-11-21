# Plethron
Plethron is a PC port of the Japan-only [**ポケモンスタジアム**](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9mon_Stadium_(Japanese)) (Also known as Pokemon Stadium Zero or Pocket Monsters Stadium) using a fork of [libultraship](https://github.com/team-hippodrome/libultraship) and our decomp [pokestadium0](https://github.com/team-hippodrome/pokestadium0). The game is now available to be played in:
- English
- Spanish
- German
- Portuguese
- Korean
- French
- Italian
- Japanese with Kanji

To fix or add a translation, please see the [translation](https://github.com/team-hippodrome/plethron/tree/main/translations) folder.

> **Note:** You must supply your own rom with SHA256 `ed9f7cd7da795467c1e2b08ed1f878ff5aed906eae9ffe252074aef7a6703d8b` to use this port.

- [**Join our Discord server**](https://discord.gg/tCpZzcFwv8)
- [**Report an issue**](https://github.com/team-hippodrome/plethron/issues/new/choose)
- [**Building Instructions**](https://github.com/team-hippodrome/plethron/blob/main/docs/BUILDING.md)

## Features
- Full transfer pak support for all official version of the Generation 1 games.
- Full translations for supported languages.
- Widescreen support.
- Anti-aliasing.
- Frame interpolation to 60 FPS.
- Optional enhancements from [hippodrome-utils](https://github.com/team-hippodrome/hippodrome-utils), all off by default (launcher > Graphics > Enhancements):
  - Summary bars: both parties beside the game's picture during battles (HP, status, the moves seen, the opponent's estimated Speed). They need room beside the picture: a 4:3 picture in a wide window, or 16:9 on an ultrawide screen.
  - Trainer memory: the tournament trainers' teams, moves and Speed you have seen, on a C button at the cup select.
- Cheats, all off by default (launcher: tick Debug, then Cheats under Save editor):
  - Stadium battles, ticked per player (1P-4P): Infinite HP, Infinite PP, Status always good, Your moves never miss, Opponent never hits, Always your turn first, Perfect stats, Opponent minimum stats. They change the running battle only, never the N64 save.
  - Game Boy: GameShark codes (`01VVLLHH`) for the cartridge in each Transfer Pak, written to the Game Boy's work RAM while the GB Player runs it. What they change is kept in the cartridge save if you save in the Game Boy game.

---

Part of the [R.A.I.D. (Retro AI Development) Community](https://discord.gg/tCpZzcFwv8)
