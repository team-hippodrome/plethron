# Save editor could not enter Game Boy special characters in names (forced upper case, no gender signs, PK/MN)

- **Category:** saves
- **Zero status:** already ported (plethron's save editor has the tokens and Symbols picker)
- **stadion:** [PR #137](https://github.com/team-hippodrome/stadion/pull/137), commit(s) `579cb85`
- **stadion files:** `src/port/save_editor/save_editor_ui.cpp`, `src/port/save_editor/stadium_save.cpp`, `tools/save_editor/stadium_save/text.py`

## Symptom
Name fields forced upper case (é became É, which the GB character set lacks, so it failed), the UI font has no ♂/♀,
PK/MN could not be typed, and a stray `<` raised an error.

## Root cause
The field uppercased input and the codec only knew the raw glyphs, which the ImGui font cannot draw or type.

## Fix in stadion
Fields keep the typed case and spell `<M> <F> <PK> <MN>`; a Symbols picker inserts them and é × ' ¥ etc.; both codecs
accept the tokens and typographic ' and -, and report bad tokens as ordinary encoding errors.

## Zero / plethron
Present: `src/port/save_editor/save_editor_ui.cpp:69-94` (token mapping, `GbSymbols()`), :444-488 (Symbols button);
`src/port/save_editor/stadium_save.cpp:231-232` (`<PK>`/`<MN>`), :297 (`<M>` → ♂). Applies to the Game Boy save side
of plethron's editor; Zero's own names go through `zero_text.cpp` (separate codec, not covered by this fix).

## How to detect
Round-trip all 256 GB bytes through decode/encode (stadion `tools/save_editor/tests/test_native.py`).
