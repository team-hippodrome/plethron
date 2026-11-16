#include "zero_sram.h"

#include <algorithm>
#include <cstring>

namespace ZeroSram {

// SaveSram (game/src/save_data.h): kinds 16-21 in Save_LoadTypedRecordWithRepair's order.
const std::vector<RecordKind>& Kinds() {
    static const std::vector<RecordKind> kinds = {
        { "teams", 0x0000, 0x128, 16 },     // SaveTeamRecord
        { "boxes", 0x2500, 0x560, 4 },      // SaveBoxRecord
        { "item_boxes", 0x5000, 0xE0, 4 },  // SaveItemRecord
        { "list", 0x5700, 0x11A0, 1 },      // SaveListRecord
        { "options", 0x7A40, 0x10, 1 },     // SaveOptionsRecord
        { "continue", 0x7A60, 0x288, 1 },   // SaveContinueRecord
    };
    return kinds;
}

const char* OrderName(Order order) {
    switch (order) {
        case Order::N64: return "N64 byte order";
        case Order::WordSwapped: return "word-swapped (Project64) byte order";
        case Order::HalfSwapped: return "halfword-swapped byte order";
        case Order::HostFields: return "the byte order of earlier port builds";
        case Order::Blank: return "a blank save";
    }
    return "";
}

namespace {

struct Field {
    uint16_t offset; // in the record
    uint8_t width;   // 2 or 4
};

// The multi-byte fields of one copy of each kind, the footer's three halfwords included ('PO', 'KE', the sum:
// Save_WriteFooter stores them as u16). Every other byte of a record is a byte array (names, GB Pokémon records,
// items, the tournament session).
std::vector<Field> FieldsOf(const RecordKind& kind) {
    std::vector<Field> f;
    const std::string key = kind.key;
    f.push_back({ 0x00, 2 }); // flags
    if (key == "teams" || key == "boxes") {
        f.push_back({ 0x08, 2 }); // trainerId
        f.push_back({ 0x12, 2 }); // count
    } else if (key == "item_boxes") {
        f.push_back({ 0x08, 2 }); // trainerId
        f.push_back({ 0x0A, 2 }); // count
    } else if (key == "list") {
        f.push_back({ 0x02, 2 }); // count
    } else if (key == "options") {
        f.push_back({ 0x04, 4 }); // gbPlayerButtons (progress at 0x08 is two bytes, read big-endian by the game)
    } else if (key == "continue") {
        f.push_back({ 0x0E, 2 });         // count
        f.push_back({ 0x11E + 0x02, 2 }); // gbSnapshot.playerId (GbSavePortSnapshot)
    }
    for (uint16_t back : { 6, 4, 2 }) f.push_back({ static_cast<uint16_t>(kind.size - back), 2 });
    return f;
}

struct AbsField {
    uint32_t offset;
    uint8_t width;
};

const std::vector<AbsField>& AllFields() {
    static const std::vector<AbsField> all = [] {
        std::vector<AbsField> out;
        for (const RecordKind& kind : Kinds()) {
            const std::vector<Field> fields = FieldsOf(kind);
            for (int copy = 0; copy < kind.slots * 2; copy++) {
                for (const Field& x : fields) out.push_back({ kind.offset + copy * kind.size + x.offset, x.width });
            }
        }
        return out;
    }();
    return all;
}

bool LittleEndianHost() {
    const uint16_t probe = 1;
    uint8_t first;
    memcpy(&first, &probe, 1);
    return first == 1;
}

bool CopyValid(const uint8_t* rec, size_t size) {
    if (memcmp(rec + size - 6, "POKE", 4) != 0) return false;
    uint16_t sum = 0;
    for (size_t i = 0; i + 2 < size; i++) sum += rec[i];
    return sum == ((rec[size - 2] << 8) | rec[size - 1]);
}

// The footer sum is a byte sum, so it cannot tell a halfword-swapped file from one with swapped fields (both put
// "POKE" and the sum back in place). The trainer names can: a name is empty (0x50 first) or has characters and then
// the 0x50 terminator. Counted over the teams, boxes and item records.
int PlausibleNames(const uint8_t* image) {
    int n = 0;
    for (const RecordKind& kind : Kinds()) {
        const std::string key = kind.key;
        if (key != "teams" && key != "boxes" && key != "item_boxes") continue;
        for (int copy = 0; copy < kind.slots * 2; copy++) {
            const uint8_t* name = image + kind.offset + copy * kind.size + 2;
            const uint8_t* end = std::find(name, name + 6, 0x50);
            n += end < name + 6 && std::find(name, end, 0x00) == end;
        }
    }
    return n;
}

} // namespace

void SwapFields(uint8_t* data, size_t sramOffset, size_t size) {
    for (const AbsField& f : AllFields()) {
        if (f.offset < sramOffset || f.offset + f.width > sramOffset + size) continue;
        std::reverse(data + (f.offset - sramOffset), data + (f.offset - sramOffset) + f.width);
    }
}

void ToHost(uint8_t* data, size_t sramOffset, size_t size) {
    if (LittleEndianHost()) SwapFields(data, sramOffset, size);
}

void ToN64(uint8_t* data, size_t sramOffset, size_t size) {
    if (LittleEndianHost()) SwapFields(data, sramOffset, size);
}

int ValidCopies(const uint8_t* image) {
    int valid = 0;
    for (const RecordKind& kind : Kinds()) {
        for (int copy = 0; copy < kind.slots * 2; copy++) valid += CopyValid(image + kind.offset + copy * kind.size, kind.size);
    }
    return valid;
}

bool Normalize(const std::vector<uint8_t>& source, std::vector<uint8_t>& normalized, Order* order, std::string& error) {
    normalized.clear();
    error.clear();
    if (source.size() != kSize) {
        error = "Choose a raw 32 KiB Pocket Monsters Stadium (Japan) SRAM save (.sra), not a save state, a Controller "
                "Pak file or a Pokémon Stadium flash save.";
        return false;
    }
    for (uint8_t fill : { uint8_t(0x00), uint8_t(0xFF) }) {
        if (std::all_of(source.begin(), source.end(), [fill](uint8_t b) { return b == fill; })) {
            normalized = source;
            if (order) *order = Order::Blank;
            return true;
        }
    }
    struct Candidate {
        Order order;
        std::vector<uint8_t> bytes;
        int valid; // valid copies first, plausible names to break a tie
    };
    std::vector<Candidate> candidates;
    for (Order o : { Order::N64, Order::WordSwapped, Order::HalfSwapped, Order::HostFields }) {
        std::vector<uint8_t> c(kSize);
        const size_t x = o == Order::WordSwapped ? 3 : o == Order::HalfSwapped ? 1 : 0;
        for (size_t i = 0; i < kSize; i++) c[i] = source[i ^ x];
        if (o == Order::HostFields) SwapFields(c.data(), 0, kSize);
        const int copies = ValidCopies(c.data());
        const int valid = copies == 0 ? 0 : copies * 1000 + PlausibleNames(c.data());
        candidates.push_back({ o, std::move(c), valid });
    }
    const Candidate* best = nullptr;
    bool ambiguous = false;
    for (const Candidate& c : candidates) {
        if (best == nullptr || c.valid > best->valid) {
            best = &c;
            ambiguous = false;
        } else if (c.valid == best->valid && c.bytes != best->bytes) {
            ambiguous = true;
        }
    }
    if (best == nullptr || best->valid == 0 || ambiguous) {
        error = "No valid Pocket Monsters Stadium save records found in a single byte order. The file may be damaged, "
                "a save state, or for another game.";
        return false;
    }
    normalized = best->bytes;
    if (order) *order = best->order;
    return true;
}

std::string WesternSidePath(const std::string& savePath) {
    return savePath + ".western";
}

} // namespace ZeroSram
