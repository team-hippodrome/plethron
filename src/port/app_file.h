#pragma once
// The port's own data files (translations/*.csv, assets/font/*): a copy next to the executable wins, so translators
// can edit the tables without rebuilding; otherwise the copy built into plethron.o2r (tools/make_o2r.py) is read.
// A release ships only the executable, so the built-in copy is what players get.
#include <string>

namespace Port {

// Reads `relPath` (e.g. "translations/ai.csv") into `out`. Returns false when neither copy exists; `where` then
// names the loose path that was tried, else it says which copy was read.
bool ReadAppFile(const std::string& relPath, std::string& out, std::string* where = nullptr);

} // namespace Port
