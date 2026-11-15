// Runtime text translation: looks drawn strings up by the md5 of their original bytes in translations/human.csv and
// translations/ai.csv (see translations/README.md). Language 0 is the original (kana) and does no lookup.
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
// The language's name in that language (UTF-8), for the language select screen.
const char* Port_Text_LanguageName(int lang);

// The translation of `s` (original bytes up to the NUL) in the current language as UTF-8, or NULL when the
// language is the original or no table has a non-empty cell for it. A miss is logged once.
const char* Port_Text_Lookup(const char* s);

// printf into `out` with the format string and every %s argument translated. Supports the conversions the game
// uses (flags, width, precision, h/l, d i u x X o c s p f %) and positional %n$ forms in translated formats.
// Returns the length written (without the NUL). With the original language this is a plain vsnprintf.
int Port_Text_Vformat(char* out, size_t cap, const char* fmt, va_list ap);

// Interim renderer bridge until the Unicode font lands: rewrites UTF-8 into the game's EUC-JP glyph codes (ASCII
// to the full-width JIS forms, U+3000 block kana as is, anything else as a full-width '?'). Returns the length.
int Port_Text_Utf8ToGameEuc(const char* in, unsigned char* out, size_t cap);

// Writes the lowercase hex md5 of `len` bytes to `hex` (33 bytes).
void Port_Text_Md5Hex(const void* data, size_t len, char hex[33]);

#ifdef __cplusplus
}
#endif
