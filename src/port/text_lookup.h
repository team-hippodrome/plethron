// Runtime text translation: finds a drawn string's row in translations/human.csv and translations/ai.csv by its id
// (the host address of the row's N64 addresses, or a noted copy), falling back to the md5 of its original bytes (see
// translations/README.md). Language 0 is the original (kana) and does no lookup.
#pragma once
#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum PortLanguage {
    PORT_LANG_ORIGINAL = 0, // にほんご: the ROM's own kana text
    PORT_LANG_JA,           // Japanese with kanji
    PORT_LANG_EN,
    PORT_LANG_ES,
    PORT_LANG_DE,
    PORT_LANG_PT,
    PORT_LANG_KO, // the Unicode test case (Russian, Arabic and Hindi were dropped)
    PORT_LANG_FR, // French and Italian come after Korean so a saved gPort.Language keeps its meaning
    PORT_LANG_IT,
    PORT_LANG_COUNT
};

void Port_Text_Init(void);
void Port_Text_SetLanguage(int lang);
int Port_Text_GetLanguage(void);
// PORT_LANGUAGE set the language (a debug aid): the language select screen is skipped.
int Port_Text_LanguageForced(void);
// The language chosen on the last boot (settings key gPort.Language), and saving a new choice.
int Port_Text_SavedLanguage(void);
void Port_Text_SaveLanguage(int lang);
// Whether the announcer's lines get subtitles in the translated languages (port setting gPort.Subtitles, on by
// default; Options > Subtitles), and saving a new choice.
int Port_Subtitles_Enabled(void);
void Port_Subtitles_SetEnabled(int on);
// Whether the Pokédex index's kana order follows the translated names (port setting gPort.PokedexNameOrder, on by
// default; game/src/pokedex_name_order.c).
int Port_PokedexNameOrder_Enabled(void);
// How COM Pokémon nicknames and the Nintendo Cup '97 finalists' names are drawn in the translated languages (port
// setting gPort.NameStyle; Options > Names): localized (the meaning, おれさま -> THE MAN, translations/localized.csv,
// the default) or transliterated (the kana romanized, おれさま -> Oresama, translations/ai.csv).
enum PortNameStyle { PORT_NAMES_LOCALIZED = 0, PORT_NAMES_TRANSLITERATED = 1 };
int Port_Names_GetStyle(void);
void Port_Names_SetStyle(int style);
// The language's name in that language (UTF-8), for the language select screen.
const char* Port_Text_LanguageName(int lang);

// The translation of `s` (original bytes up to the NUL) in the current language as UTF-8, or NULL when the
// language is the original or no table has a non-empty cell for it. A miss is logged once.
const char* Port_Text_Lookup(const char* s);

// The translation of the row named `id` (translations/*.csv) in the current language, or NULL. For text the port
// draws itself (the language select screen) and strings it knows by name.
const char* Port_Text_LookupId(const char* id);

// The game copied string `src` into `dst` (Text_CopyString): `dst` keeps `src`'s id while it holds the same bytes, so
// a copy of a string whose bytes have two meanings (ゴースト: HAUNTER_NAME / GHOST_TYPE_NAME) translates as its source.
void Port_Text_NoteCopy(const char* dst, const char* src);

// printf into `out` with the format string and every %s argument translated. Supports the conversions the game
// uses (flags, width, precision, h/l, d i u x X o c s p f %) and positional %n$ forms in translated formats.
// Returns the length written (without the NUL). With the original language this is a plain vsnprintf.
int Port_Text_Vformat(char* out, size_t cap, const char* fmt, va_list ap);

// The game formatted `fmt` with `ap` into the buffer at `dst` by itself (BattleMessage_Format): `dst` translates as
// the translated format with translated arguments while it holds the same bytes. No-op in the original language.
void Port_Text_NoteFormatted(const char* dst, const char* fmt, va_list ap);
// The game built `dst` from `head` followed by `tail` (a name and a fragment such as 「をあつめる」): `dst` translates
// as head's translation followed by tail's while it holds the same bytes. No-op in the original language.
void Port_Text_NoteJoined(const char* dst, const char* head, const char* tail);
// `dst` now holds a copy of `src`'s bytes and translates as `src` does (a noted format, an id or the md5).
void Port_Text_NoteSame(const char* dst, const char* src);
// `s` as UTF-8 in the current language: its noted format, its translation, or the EUC-JP bytes converted. The
// pointer stays valid until `s` is noted again. NULL in the original language.
const char* Port_Text_Utf8Of(const char* s);

// `s` as UTF-8 for text the port draws: like Port_Text_Utf8Of, and in the original language its EUC-JP bytes
// converted. Never NULL.
const char* Port_Text_Display(const char* s);

// Records a translated line (up to '\n') that is wider than its box, once per text and language, in
// text_overflows.csv next to the app: language,width,available,text.
void Port_Text_LogOverflow(const char* utf8, int width, int avail);

// Checks id, address and md5 lookups against the tables and the ROM (main.cpp --text-selftest). 0 = passed.
int Port_Text_SelfTest(void);

// Writes the lowercase hex md5 of `len` bytes to `hex` (33 bytes).
void Port_Text_Md5Hex(const void* data, size_t len, char hex[33]);

#ifdef __cplusplus
}
#endif
