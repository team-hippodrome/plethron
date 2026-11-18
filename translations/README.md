# Translations

The PC port shows the original Japanese by default (にほんご, kana). On the language
select screen, pressing C-Left unlocks the AI translations below. Text is looked up
at runtime by key. Nothing here contains the original game text, except the kana of the short names in `localized.csv` (see below).

| File | What it is |
| --- | --- |
| `ai.csv` | Machine-assisted translations. Every language column is filled here first. |
| `human.csv` | Human translations and fixes. It has a row for every string with empty language cells; any cell you fill wins over `ai.csv`. |
| `localized.csv` | The localized form of the name rows (see [Names](#names-localized-or-transliterated)). Edit it directly, by hand or by AI. |
| `rom-text/` | **Gitignored.** Your own ROM's text, written by `tools/text/show_kana.py`. Never commit it. |

## Format

UTF-8, RFC 4180 quoting, one header row, identical columns in both files:

```
id,md5,addr,ja,en,es,de,pt,ko,fr,it,note
```

- `id` names the string (`[A-Z][A-Z0-9_]*`, unique within a file), for example
  `BULBASAUR_DEX_ENTRY`, `THUNDERBOLT_MOVE_NAME` or `BATTLE_MSG_CANT_MOVE`. It is
  the key the port relies on. A row in `human.csv` uses the same id as the row it
  overrides in `ai.csv`.
- `md5` is the lowercase hex md5 of the original string's raw bytes as the game
  holds them (EUC-JP, up to but not including the NUL). It is always filled and
  is the fallback key: when the port can't tell which id a string is, it looks it
  up by md5.
- `addr` lists the N64 link addresses the string sits at, as 8 uppercase hex
  digits separated by spaces (`80079EE4 80079EF8`). The port binds ids by
  address. It is empty for the strings that only exist inside a compressed
  archive (Free Battle rentals and preset teams); those are found by md5.
- A few strings have identical bytes but different meanings, such as ゴースト,
  which is both Haunter's name and the Ghost type. Each meaning gets its own row
  and id (`HAUNTER_NAME`, `GHOST_TYPE_NAME`) with the same md5 and its own
  addresses. When only the md5 is known, the first of those rows in the file wins.
- `ja` is Japanese with kanji. The kana original needs no column: it's the ROM. Strings with
  nothing to convert (katakana names, people's names, nicknames, debug text) are left
  empty on purpose and fall through to the original.
- `en` … `it` are the translations. A line break is a real newline inside a quoted
  cell. A non-printing byte is written `\xNN`.
- `note` is context for translators that the id and the table below don't
  already give. It never holds the original text.
- An empty cell falls through: `human.csv`, then `ai.csv`, then the original.
- A new language is a new column added before `note`. It's nice to keep `ai.csv`
  under 512 KB so GitHub renders it as a table, but not at the cost of content.

### Names: localized or transliterated

COM Pokémon nicknames (`<TRAINER>_<SPECIES>`), COM trainer names and handles (`HANDLE_*`) and the Nintendo Cup '97 finalists'
names (`FINALIST_*`) can be shown two ways, picked in Options > Names (port setting `gPort.NameStyle`, debug override
`PORT_NAME_STYLE=localized|transliterated`):

- **Localized** (the default): the meaning translated, おれさま → THE MAN, テンサイ → Genius, どろキング →
  Thiefking. These cells are in `localized.csv`, which wins over `human.csv` and `ai.csv` while names are localized.
- **Transliterated**: the kana romanized, おれさま → Oresama, どろキング → Dorokingu. These are the rows' cells in
  `ai.csv` (en, es, de, pt, ko, fr and it alike), written by `tools/text/romanize_names.py <rom>` from your ROM's kana:
  Hepburn without macrons, long vowels doubled (Yuusuke, Ruuruu), honorifics after a hyphen (Jura-sama). Fix one
  in `human.csv`.

`localized.csv` has a `kana` column after `addr` (`id,md5,addr,kana,ja,…,note`): the names are short, so their
kana sit next to the translation to help discuss it. The script refreshes it. Every row's id, md5 and addr match
its `ai.csv` row. The rows in `localized.csv` are the set of names the option switches; species names stored as
nicknames (ダグトりオ, カイりュー …), trainer classes and regions are not in it and stay the same either way.
Japanese with kanji keeps the kana names, so the option does nothing there.

### Where each id appears

Notes that would repeat on every row of a group live here instead.

| Ids | Context |
| --- | --- |
| `any id ending _CATEGORY` | Pokédex category, shown through "%s Pokémon" |
| `any id ending _STATUS_NAME` | Stat or status label |
| `any id ending _TM_CARD` | TM/HM info card: move, type, PP / power, accuracy (fixed-width columns) |
| `BATTLE_FRAGMENT_*` | Battle text fragment appended after a name |
| `BATTLE_MENU_*` | Battle menu |
| `BATTLE_MODE_*` | Battle Mode menus and tournament rules |
| `BATTLE_MSG_*` | Battle message; %s is a Pokémon or move name |
| `<TRAINER>_<SPECIES>` | A COM Pokémon's nickname, named after its owner and species (puns are fine to keep loose): `MIKA_DUGTRIO` is Nintendo Cup '97 finalist Fujita Mika's Dugtrio (finalists by given name, Lv. 50–55 Tournament); `BUG_CATCHER_BEEDRILL` the Lv. 1–30 Tournament Bug Catcher's (one row for all four cups); `FREE_BATTLE_LASS_TAUROS` a Free Battle preset team's (Yay0 data, no addr). A nickname two owners share (ぎゅうた, まさこ, a species name used as a nickname such as ディグダ) has a row per owner, same md5, own addr |
| `CONTINUE_*` | Tournament continue prompt |
| `CONTROLLER_ERROR_*` | Controller / 64DD error screen |
| `DEBUG_*` | Battle debug menu (unused in normal play) |
| `FATAL_ERROR_*` | Fatal error screen |
| `FINALIST_*` | Nintendo Cup '97 finalist region or name (locked romanization, glossary) |
| `GB_PAK_*` | GB Player / Game Pak messages |
| `GB_PLAYER_*` | The GB Player's own screens: the C-Up menu and its help, Help & config, the speed-up help, the Game Pak trouble screens, the cartridge copy screen's scrolling line and the status strip under the Game Boy screen. The GB Player draws these with its own font from glyph lists, not ROM strings, so `game/src/gb_player_text.c` lays the cells out again: `md5` is the md5 of the kana in EUC-JP, `addr` is empty. Cells are wrapped at run time to the original box (a `\n` forces a break); words in `{braces}` take the screen's highlight colour. English, Spanish, German, Portuguese, French, Italian and Japanese draw with the GB Player's font (accents are added to its letters), Korean and kanji with Unifont |
| `HANDLE_*` | COM trainer handle or name (the OT of a COM team's Pokémon, e.g. `HANDLE_ORESAMA` おれさま) or trainer class word |
| `HABITAT_*` | Pokédex habitat map and index |
| `LAB_*` | Lab: PC boxes, items and Pokémon list |
| `MAIN_MENU_*` | GB Player / main menu |
| `MODE_*` | Stadium mode or rule label |
| `MODEL_TEXT_*` | Words the game draws as 3D models, not text: the round titles on the VS screen (バトル, セミファイナル, ファイナルバトル; the round number and dash stay the original models, moved right when the word is longer), チャンピオン! over the badge cap after a cup's final, and かち / まけ on the results panels. The port builds the meshes from `assets/font/text3d_subset.otf` (`src/port/text3d.cpp`); run `tools/font/make_text3d_subset.py` after adding characters. Not ROM text: `md5` is the md5 of the kana in EUC-JP, `addr` is empty, and the kana are in the note. Round titles and チャンピオン! stay katakana loanwords: Latin capitals (BATTLE, CHAMPION!), hangul for Korean (배틀, 챔피언!). An empty `ja` keeps the original models |
| `NICKNAME_*` | Free Battle rental data (Yay0): the rentals' OT ポケスタ and species names written with り |
| `PIKACHU_SURF_*` | Pikachu Surf reward after a tournament |
| `PLACE_*` | Pokédex habitat map label |
| `REGISTER_*` | Pokémon registration |
| `SAVE_ERROR_*` | Save/Game Pak error (lines may be printed separately) |
| `STAT_*` | Stat or status label |
| `SUMMARY_*` | Party/summary screen |
| `SUBTITLE_ANNOUNCER_*` | Subtitle for the announcer's voice line with that effect id (hex) in JIKKYOU.WBK, shown while it plays (`game/src/subtitles.c`). Not ROM text: `ja` is a transcription of the audio (from pokestadium0's `text/subtitles.json`, branch `feat/subs`), `md5` is the md5 of that transcription in EUC-JP, `addr` is empty. Wrapped at run time, so cells need no line breaks. Ids 01–08 (badge call-outs) and 47 ("good luck", spoken in English) have no row on purpose: they stay unsubtitled |
| `TEAM_SELECT_*` | Team select prompts; "＞" is a page break |
| `TERM_*` | Menu or battle term |
| `TRAINER_CLASS_*` | COM trainer class |

Ids ending `_NAME`, `_DEX_ENTRY`, `_MOVE_NAME`, `_MOVE_DESC`, `_ITEM_NAME`,
`_ITEM_DESC` and `_TYPE_NAME` are the species, Pokédex, move, item and type
tables. `CREDITS_*` is the staff roll and `OPTIONS_*` the options menu.

The runtime hooks the game's printf-style text calls. It identifies the format string
and every `%s` argument separately (by address, falling back to md5), so a Pokémon
or move name is translated once and reused everywhere. Keep the same printf conversions
(`%s`, `%d`, `%02d` …) in the same order as the original. If a language needs a
different order, use positional forms (`%2$s … %1$s`) and say so in `note`.

Some text is drawn piece by piece (a name, then "fainted"), and those pieces
are translated as fragments. Text drawn into 3D models isn't translated yet, and
textures are only replaced where the game can't be understood without it, using
pictograms rather than per-language art.

## Line length

The game wraps nothing itself: every `\n` is where the original broke the line.
Japanese lines hold about 20 full-width characters, so keep translated lines to
about 36 half-width characters (Pokédex entries, move and item descriptions) and
keep the same number of lines or fewer.

## Naming

Use the official names. `docs/glossary.md` in team-hippodrome/pokestadium0 is
the reference for English (Pokémon, moves, items, types, places, trainer classes,
Stadium modes). Zero's GB mode is the **GB Player**, never "GB Tower". The
Nintendo Cup '97 finalists keep the glossary's locked romanizations (for example,
ふじた みか is Fujita Mika). Proper nouns with no official name in a language are
transliterated from the English name. COM trainer nicknames are puns; loose
renderings are fine.

## Workflow for human translators

1. Dump your ROM's text next to the current translations:

   ```
   python3 tools/text/show_kana.py path/to/your/rom.z64
   ```

   This writes `translations/rom-text/kana.csv` (md5, location, kana),
   `review.csv` (kana beside what the port shows for each language, and whether
   that came from human.csv or ai.csv) and `index.html` (the same table, searchable,
   in a browser). The folder is gitignored.

2. Fill cells in `human.csv`. It already has a row for every string, with the
   `id`, `md5` and `addr` filled and every language cell empty; find the id in
   `review.csv` and fill only the columns you're translating. Leave the others empty so they fall through.

3. Check your edits:

   ```
   python3 tools/text/check_tables.py                      # format only
   python3 tools/text/check_tables.py --rom your/rom.z64   # also keys and %s parity
   ```

## Finding text the scan missed

`tools/text/romtext.py` finds strings by scanning the ROM for NUL-terminated
EUC-JP text in the resident and fragment images, plus the Yay0 archive that holds
the COM trainer tables. Text the game builds at runtime (`sprintf`, string
concatenation) has no fixed key. The port logs every lookup miss (md5 and raw
bytes) to a gitignored file in the user data folder; send those to us and we'll
add rows.
