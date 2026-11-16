#include "launcher_files.h"
#include "file_io.h"
#include "zero_sram.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <cerrno>

namespace fs = std::filesystem;

namespace Port {
std::string PathUtf8(const fs::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

std::string ResolveLauncherPath(const std::string& path, const fs::path& directory) {
    if (path.empty()) return "";
    const auto value = fs::u8path(path);
    return PathUtf8((value.is_absolute() ? value : directory / value).lexically_normal());
}

std::string StoreLauncherPath(const std::string& path, const fs::path& directory) {
    if (path.empty()) return "";
    const auto value = fs::u8path(path).lexically_normal();
    const auto relative = value.lexically_relative(directory.lexically_normal());
    if (!relative.empty() && *relative.begin() != "..") {
        const auto stored = relative.generic_u8string();
        return {stored.begin(), stored.end()};
    }
    return PathUtf8(value);
}

bool NormalizeZeroSave(const std::vector<uint8_t>& source, std::vector<uint8_t>& normalized, std::string& error) {
    return ZeroSram::Normalize(source, normalized, nullptr, error);
}

bool CopyWesternSideFile(const fs::path& save, const fs::path& destination, std::string& error) {
    const auto side = fs::u8path(ZeroSram::WesternSidePath(PathUtf8(save)));
    std::error_code ec;
    if (!fs::exists(side, ec)) return true;
    const auto target = fs::u8path(ZeroSram::WesternSidePath(PathUtf8(destination)));
    fs::copy_file(side, target, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "Could not copy the Western names file (" + PathUtf8(side.filename()) + "): " + ec.message();
        return false;
    }
    return true;
}

bool ImportZeroSave(const fs::path& source, const fs::path& directory, std::string& imported, std::string& error) {
    imported.clear();
    std::ifstream file(source, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() != static_cast<std::streamoff>(ZeroSram::kSize)) {
        error = "Cannot read a raw 32 KiB Pocket Monsters Stadium SRAM save (.sra) from that file.";
        return false;
    }
    std::vector<uint8_t> data(ZeroSram::kSize), normalized;
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()), data.size())) {
        error = "Could not read the complete save file.";
        return false;
    }
    if (!NormalizeZeroSave(data, normalized, error)) return false;
    const auto saves = directory / "saves";
    std::error_code ec;
    fs::create_directories(saves, ec);
    if (ec) {
        error = "Cannot create the saves folder beside the launcher: " + ec.message();
        return false;
    }
    for (unsigned n = 1; n <= 10000; n++) {
        std::string name = PathUtf8(source.stem()) + "-imported" + (n == 1 ? "" : "-" + std::to_string(n)) + ".sra";
        const auto destination = saves / fs::u8path(name);
        // a leftover side file of a removed import must not attach itself to this one
        if (fs::exists(fs::u8path(ZeroSram::WesternSidePath(PathUtf8(destination))), ec)) continue;
        FILE* out = OpenFileUtf8(PathUtf8(destination).c_str(), "wbx");
        if (out == nullptr) {
            if (errno == EEXIST) continue;
            error = "Cannot write the imported save beside the launcher.";
            return false;
        }
        const bool written = fwrite(normalized.data(), 1, normalized.size(), out) == normalized.size();
        const bool closed = fclose(out) == 0;
        if (!written || !closed) {
            fs::remove(destination, ec);
            error = "Could not finish writing the imported save.";
            return false;
        }
        if (!CopyWesternSideFile(source, destination, error)) {
            fs::remove(destination, ec);
            return false;
        }
        imported = PathUtf8(destination);
        return true;
    }
    error = "Too many imports with this name. Choose another save filename.";
    return false;
}
} // namespace Port
