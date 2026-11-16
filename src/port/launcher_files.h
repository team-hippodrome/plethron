#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace Port {
std::string PathUtf8(const std::filesystem::path& path);
std::string ResolveLauncherPath(const std::string& path, const std::filesystem::path& directory);
std::string StoreLauncherPath(const std::string& path, const std::filesystem::path& directory);

// Accept a raw 32 KiB Pocket Monsters Stadium SRAM save in any byte order emulators use and give it back in N64 byte
// order (zero_sram.h), keeping invalid records for the game's own backup repair.
bool NormalizeZeroSave(const std::vector<uint8_t>& source, std::vector<uint8_t>& normalized, std::string& error);
// Import a separate working copy (saves/<name>-imported.sra); never overwrite the source or an earlier import.
// The source's Western name side file (<source>.western, gb_save_convert.h) comes along as <copy>.western.
bool ImportZeroSave(const std::filesystem::path& source, const std::filesystem::path& directory,
                    std::string& imported, std::string& error);
// Copies the Western name side file of `save` to `destination` + ".western" when it exists. False (and `error`)
// only when it exists and cannot be copied.
bool CopyWesternSideFile(const std::filesystem::path& save, const std::filesystem::path& destination,
                         std::string& error);
} // namespace Port
