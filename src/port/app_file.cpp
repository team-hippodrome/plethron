#include "app_file.h"

#include <fstream>
#include <iterator>
#include <libultraship/bridge/resourcebridge.h>
#include <ship/core/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

namespace Port {

bool ReadAppFile(const std::string& relPath, std::string& out, std::string* where) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs(relPath);
    if (std::ifstream f(path, std::ios::binary); f) {
        out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        if (where != nullptr) {
            *where = path;
        }
        return true;
    }
    auto rm = ResourceGetResourceManager();
    if (rm != nullptr && rm->GetArchiveManager() != nullptr) {
        if (auto file = rm->GetArchiveManager()->LoadFile(relPath); file && file->Buffer) {
            out.assign(file->Buffer->begin(), file->Buffer->end());
            if (where != nullptr) {
                *where = relPath + " (built in)";
            }
            return true;
        }
    }
    if (where != nullptr) {
        *where = path;
    }
    return false;
}

} // namespace Port
