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

// Checks id, address and md5 lookups against the tables and the ROM (main.cpp --text-selftest). 0 = passed.
int Port_Text_SelfTest(void);

// Writes the lowercase hex md5 of `len` bytes to `hex` (33 bytes).
void Port_Text_Md5Hex(const void* data, size_t len, char hex[33]);

#ifdef __cplusplus
}
#endif
