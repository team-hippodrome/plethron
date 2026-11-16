# Translations

The PC port shows the original Japanese by default (にほんご, kana). On the language
select screen, pressing C-Left unlocks the AI translations below. Text is looked up
at runtime by key; nothing here contains the original game text.

| File | What it is |
| --- | --- |
| `ai.csv` | Machine-assisted translations. Every language column is filled here first. |
| `human.csv` | Human translations and fixes. It has a row for every string with empty language cells; any cell you fill wins over `ai.csv`. |
| `rom-text/` | **Gitignored.** Your own ROM's text, written by `tools/text/show_kana.py`. Never commit it. |

## Format

UTF-8, RFC 4180 quoting, one header row, identical columns in both files:

```
id,md5,addr,ja,en,es,de,pt,ko,note
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
  address. It is empty for the 45 strings that only exist inside a compressed
  archive (the COM party nicknames); those are found by md5.
- A few strings have identical bytes but different meanings, such as ゴースト,
  which is both Haunter's name and the Ghost type. Each meaning gets its own row
  and id (`HAUNTER_NAME`, `GHOST_TYPE_NAME`) with the same md5 and its own
  addresses. When only the md5 is known, the first of those rows in the file wins.
- `ja` is Japanese with kanji. The kana original needs no column: it's the ROM. Strings with
  nothing to convert (katakana names, people's names, nicknames, debug text) are left
  empty on purpose and fall through to the original.
- `en` … `ko` are the translations. A line break is a real newline inside a quoted
  cell. A non-printing byte is written `\xNN`.
- `note` is context for translators that the id and the table below don't
  already give. It never holds the original text.
- An empty cell falls through: `human.csv`, then `ai.csv`, then the original.
- A new language is a new column added before `note`. It's nice to keep `ai.csv`
  under 512 KB so GitHub renders it as a table, but not at the cost of content.

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
| `COM_*` | COM trainer or nickname (puns are fine to keep loose) |
| `CONTINUE_*` | Tournament continue prompt |
| `CONTROLLER_ERROR_*` | Controller / 64DD error screen |
| `DEBUG_*` | Battle debug menu (unused in normal play) |
| `FATAL_ERROR_*` | Fatal error screen |
| `FINALIST_*` | Nintendo Cup '97 finalist region or name (locked romanization, glossary) |
| `GB_PAK_*` | GB Player / Game Pak messages |
| `HABITAT_*` | Pokédex habitat map and index |
| `LAB_*` | Lab: PC boxes, items and Pokémon list |
| `MAIN_MENU_*` | GB Player / main menu |
| `MODE_*` | Stadium mode or rule label |
| `NICKNAME_*` | COM party nickname (Yay0 data) |
| `PIKACHU_SURF_*` | Pikachu Surf reward after a tournament |
| `PLACE_*` | Pokédex habitat map label |
| `REGISTER_*` | Pokémon registration |
| `SAVE_ERROR_*` | Save/Game Pak error (lines may be printed separately) |
| `STAT_*` | Stat or status label |
| `SUMMARY_*` | Party/summary screen |
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
