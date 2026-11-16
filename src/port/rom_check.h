// The ROM the port runs with: the original Japanese Pocket Monsters Stadium cartridge dump (Stadium Zero), checked by SHA-1 (rom_check.cpp). The port's
// tables (src/port/generated) come from the decomp's link map of exactly that ROM; any other dump (a revision, a hack
// or translation, a trimmed or damaged file) starts but reads its data from the wrong places and crashes later.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Port {

// SHA-1 of the original "Pocket Monsters Stadium (Japan).z64" (16 MiB, big-endian), as pokestadium0's matching build
constexpr const char* kRomSha1 = "8bc8d2ff7df25b26bd0c353d2adfe83e4e3a7a87";
constexpr size_t kRomSize = 16 * 1024 * 1024;

// lowercase hex SHA-1 of a buffer, or of a file ("" when the file cannot be read)
std::string Sha1Hex(const uint8_t* data, size_t size);
std::string Sha1HexFile(const std::string& path);

// "" when the ROM is the original dump, else a short reason. PORT_ANY_ROM=1 accepts any ROM (debugging).
std::string CheckRomDump(const uint8_t* data, size_t size);
std::string CheckRomDumpFile(const std::string& path);

} // namespace Port
