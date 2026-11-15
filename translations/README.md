# Translations

The PC port shows the original Japanese by default (にほんご, kana). On the language
select screen, pressing C-Left unlocks the AI translations below. Text is looked up
at runtime by key; nothing here contains the original game text.

| File | What it is |
| --- | --- |
| `ai.csv` | Machine-assisted translations. Every language column is filled here first. |
| `human.csv` | Human translations and fixes. Any non-empty cell here wins over `ai.csv`. |
| `rom-text/` | **Gitignored.** Your own ROM's text, written by `tools/text/show_kana.py`. Never commit it. |

## Format

UTF-8, RFC 4180 quoting, one header row, identical columns in both files:

```
md5,ja,en,es,de,pt,ko,ru,ar,hi,note
```

- `md5` is the lowercase hex md5 of the original string's raw bytes as the game
  holds them (EUC-JP, up to but not including the NUL). This is the key the runtime
  hook computes.
- `ja` is Japanese with kanji. The kana original needs no column: it's the ROM. Strings with
  nothing to convert (katakana names, people's names, nicknames, debug text) are left
  empty on purpose and fall through to the original.
- `en` … `hi` are the translations. A line break is a real newline inside a quoted
  cell. A non-printing byte is written `\xNN`.
- `note` is context for translators (where the string appears, what `%s` stands
  for). It never holds the original text.
- An empty cell falls through: `human.csv`, then `ai.csv`, then the original.
- `ru`, `ar` and `hi` are there mainly to prove Cyrillic, right-to-left and Devanagari
  text work. Menus, rules, battle messages, types and species names (transliterated)
  are translated. Pokédex entries, move and item descriptions, move and item names and
  categories are copied from `en` for now, so those columns never fall through to kana.

The runtime hooks the game's printf-style text calls. It looks up the format string
by its md5 and every `%s` argument separately by its own md5, so a Pokémon or move
name is translated once and reused everywhere. Keep the same printf conversions
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

2. Add or fix rows in `human.csv`. Copy the md5 from `review.csv` and fill only
   the columns you're translating. Leave the others empty so they fall through.

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
