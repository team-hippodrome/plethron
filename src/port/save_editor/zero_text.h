// Names in Stadium Zero's save: the Japanese Game Boy character set (5 kana and a 0x50 terminator in 6 bytes), the
// same codes Zero's GB->EUC table maps (game/src/gb_data.c Text_UntranscodeName). Used by the save library
// (stadium_save.cpp) for nicknames, OT names and the trainer names of teams, boxes and item records.
//
// The launcher's font has no kana, so the save editor shows and takes names in romaji (KanaToRomaji,
// RomajiToKana): capital letters for katakana, small letters for hiragana ("PIKACHUU" = ピカチュウ, "satoshi" = さとし).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace stadium_save {

// Text with kana, ♂/♀, full-width digits and letters; a byte with no character is "<XX>".
std::string DecodeJp(const uint8_t* d, size_t n);
// Ends the text with 0x50 and pads with `pad`. Katakana ヘ ベ ペ リ use the hiragana codes, as the cartridges do.
// Accepts <M>/<F> and <XX> (a byte in hex). Throws stadium_save::Error when a character has no code or it is too long.
std::vector<uint8_t> EncodeJp(const std::string& text, size_t size, uint8_t pad);

// Kana as romaji for the editor's font: "ピカチュウ" -> "PIKACHUU"; full-width letters as {A}; ♂ ♀ stay.
std::string KanaToRomaji(const std::string& text);
// Romaji (or kana) as kana: capitals give katakana, small letters hiragana; "-" is ー, "n'" a lone ん, a doubled
// consonant っ, x before a vowel or ya/yu/yo/tsu its small kana; digits and ?!. become full width; {A} a full-width letter.
// Throws stadium_save::Error on what it cannot read.
std::string RomajiToKana(const std::string& text);

// Japanese species names by National Pokédex number (1-151; [0] is empty), as the Gen 1 cartridges spell them.
extern const char* const JP_SPECIES[152];

} // namespace stadium_save
