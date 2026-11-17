// ares_ref — headless ares (N64 core + paraLLEl-RDP) driver that produces reference frames for the port.
//
// Boot logic (paks, CIC detection, power-on) follows michiiik/N64Recomp ares-bridge/src/ares_core_glue.cpp.
// Adds what the bridge does not have yet: scripted controller input, VI frame capture and save states.
//
// Usage: ARES_REF_FIRMWARE=<ares>/mia/Firmware/Nintendo\ 64 ares_ref <rom.z64> <script> [--load state.bin]
// Script lines (frames are emulated VI frames, counted from boot or from the loaded state):
//   hold <from> <to> <BUTTONS>   hold buttons (A, B, Z, START, L, R, UP, DOWN, LEFT, RIGHT, CU, CD, CL, CR; joined
//                                with +) for frames from..to inclusive
//   tap <frame> <BUTTONS>         hold for 2 frames
//   shot <frame> <file.ppm>       save the picture shown at that frame
//   every <n> <dir>               save a picture every n frames as <dir>/f_<frame>.ppm (for finding a moment)
//   window <from> <to> <dir>      save every frame from..to as <dir>/f_<frame>.ppm (tools/screen_diff: the frames
//                                 around a checkpoint, so a few frames of timing drift do not count as a difference)
//   save <frame> <state.bin>      save the emulator state
//   dump <frame> <file>           write RDRAM (8 MB, big-endian byte order) to a file
//   fill <from> <to> <addr> <bytes> <u16 hex>   before each of those frames, fill RDRAM [addr, addr+bytes) with a
//                                halfword (experiments: what does the RDP read from there?)
//   sync <frame> <event>          a sync point: from that frame on, the frames above wait until the game reaches
//                                 the event ("cmd": a battle command phase starts; "move": a move's presentation starts, after its message),
//                                 as the port's PORT_SYNC (tools/screen_diff sync:<event>; the xcheck section below)
//   end <frame>                   stop
// Environment: ARES_REF_SRAM / ARES_REF_SRAM_OUT (Stadium Zero's 32 KiB SRAM in / out, N64 byte order, the same bytes
// as the port's saves/stadium.sra), ARES_REF_GBROM / ARES_REF_GBSAV / ARES_REF_GBSAV_OUT / ARES_REF_GBBOARD
// (Transfer Pak cartridge; needs ares built with the Game Boy core), ARES_REF_WAV=<file> (the N64 audio output (AI),
// resampled to 48 kHz, as raw signed 16-bit stereo little-endian PCM), ARES_REF_XCHECK_RNG=<seed> and
// ARES_REF_XCHECK=<file> (the battle's random numbers seeded as the port's PORT_XCHECK_RNG: the xcheck section below).
// Pictures are what ares's VI shows (normally 640x480 for Zero's 320x240 frame buffers at "Quality SD").
#include <ares/ares.hpp>
#include <n64/n64.hpp>
#include <nall/main.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

using namespace ares;

namespace {

// ---- boot helpers (from ares-bridge) ----
uint64_t ipl2checksum(uint32_t seed, const uint8_t* rom) {
    auto rotl = [](uint32_t v, uint32_t s) -> uint32_t { return (v << s) | (v >> ((-s) & 31)); };
    auto rotr = [](uint32_t v, uint32_t s) -> uint32_t { return (v >> s) | (v << ((-s) & 31)); };
    auto csum = [](uint32_t a0, uint32_t a1, uint32_t a2) -> uint32_t {
        if (a1 == 0) a1 = a2;
        uint64_t prod = (uint64_t)a0 * a1;
        uint32_t diff = (uint32_t)(prod >> 32) - (uint32_t)prod;
        return diff ? diff : a0;
    };
    size_t cursor = 0;
    auto readm = [&]() -> uint32_t {
        uint32_t v = rom[cursor] << 24 | rom[cursor + 1] << 16 | rom[cursor + 2] << 8 | rom[cursor + 3];
        cursor += 4;
        return v;
    };
    uint32_t init = 0x6c078965 * (seed & 0xff) + 1;
    uint32_t data = readm();
    init ^= data;
    uint32_t state[16];
    for (auto& s : state) s = init;
    uint32_t dataNext = data, dataLast, loop = 0;
    while (true) {
        loop++;
        dataLast = data;
        data = dataNext;
        state[0] += csum(1007 - loop, data, loop);
        state[1] = csum(state[1], data, loop);
        state[2] ^= data;
        state[3] += csum(data + 5, 0x6c078965, loop);
        state[9] = (dataLast < data) ? csum(state[9], data, loop) : state[9] + data;
        state[4] += rotr(data, dataLast & 0x1f);
        state[7] = csum(state[7], rotl(data, dataLast & 0x1f), loop);
        state[6] = (data < state[6]) ? (state[3] + state[6]) ^ (data + loop) : (state[4] + data) ^ state[6];
        state[5] += rotl(data, dataLast >> 27);
        state[8] = csum(state[8], rotr(data, dataLast >> 27), loop);
        if (loop == 1008) break;
        dataNext = readm();
        state[15] = csum(csum(state[15], rotl(data, dataLast >> 27), loop), rotl(dataNext, data >> 27), loop);
        state[14] = csum(csum(state[14], rotr(data, dataLast & 0x1f), loop), rotr(dataNext, data & 0x1f), loop);
        state[13] += rotr(data, data & 0x1f) + rotr(dataNext, dataNext & 0x1f);
        state[10] = csum(state[10] + data, dataNext, loop);
        state[11] = csum(state[11] ^ data, dataNext, loop);
        state[12] += state[8] ^ data;
    }
    uint32_t buf[4];
    for (auto& b : buf) b = state[0];
    for (loop = 0; loop < 16; loop++) {
        uint32_t d = state[loop];
        uint32_t tmp = buf[0] + rotr(d, d & 0x1f);
        buf[0] = tmp;
        buf[1] = d < tmp ? buf[1] + d : csum(buf[1], d, loop);
        uint32_t tb = (d & 2) >> 1, t2 = d & 1;
        buf[2] = tb == t2 ? buf[2] + d : csum(buf[2], d, loop);
        buf[3] = t2 == 1 ? buf[3] ^ d : csum(buf[3], d, loop);
    }
    uint64_t checksum = (uint64_t)csum(buf[0], buf[1], 16) << 32;
    checksum |= buf[3] ^ buf[2];
    return checksum & 0xffffffffffffull;
}

std::string detectCic(const uint8_t* ipl3) {
    switch (ipl2checksum(0x3f, ipl3)) {
        case 0x45cc73ee317aull: return "CIC-NUS-6101";
        case 0x44160ec5d9afull: return "CIC-NUS-7102";
        case 0xa536c0f1d859ull: return "CIC-NUS-6102";
    }
    if (ipl2checksum(0x78, ipl3) == 0x586fd4709867ull) return "CIC-NUS-6103";
    if (ipl2checksum(0x91, ipl3) == 0x8618a45bc2d3ull) return "CIC-NUS-6105";
    if (ipl2checksum(0x85, ipl3) == 0x2bbad4e6eb74ull) return "CIC-NUS-6106";
    return "CIC-NUS-6102";
}

// ---- input / video ----
std::atomic<uint32_t> gButtons{ 0 };
enum : uint32_t { bA = 1, bB = 2, bZ = 4, bSTART = 8, bL = 16, bR = 32, bUP = 64, bDOWN = 128, bLEFT = 256,
                  bRIGHT = 512, bCU = 1024, bCD = 2048, bCL = 4096, bCR = 8192 };
const std::map<std::string, uint32_t> kButtonNames = {
    { "A", bA }, { "B", bB }, { "Z", bZ }, { "START", bSTART }, { "L", bL }, { "R", bR }, { "UP", bUP },
    { "DOWN", bDOWN }, { "LEFT", bLEFT }, { "RIGHT", bRIGHT }, { "CU", bCU }, { "CD", bCD }, { "CL", bCL },
    { "CR", bCR } };
const std::map<std::string, uint32_t> kNodeNames = {
    { "A", bA }, { "B", bB }, { "Z", bZ }, { "Start", bSTART }, { "L", bL }, { "R", bR }, { "Up", bUP },
    { "Down", bDOWN }, { "Left", bLEFT }, { "Right", bRIGHT }, { "C-Up", bCU }, { "C-Down", bCD },
    { "C-Left", bCL }, { "C-Right", bCR } };

std::mutex gVideoMutex;
std::vector<uint32_t> gPicture;
uint32_t gPicW = 0, gPicH = 0;
std::atomic<uint64_t> gPictures{ 0 };

struct RefPlatform : ares::Platform {
    std::shared_ptr<vfs::directory> systemPak, cartridgePak, gbPak;
    auto pak(Node::Object node) -> std::shared_ptr<vfs::directory> override {
        if (!node) return {};
        string name = node->name();
        if (name == "Nintendo 64") return systemPak;
        if (name == "Game Boy Cartridge" || name == "Game Boy Color Cartridge") return gbPak;
        if (name.endsWith("Cartridge")) return cartridgePak;
        return {};
    }
    auto status(string_view message) -> void override { fprintf(stderr, "[ares] %s\n", string{ message }.data()); }
    auto video(Node::Video::Screen, const u32* data, u32 pitch, u32 width, u32 height) -> void override {
        std::lock_guard<std::mutex> lock(gVideoMutex);
        gPicture.resize((size_t)width * height);
        for (u32 y = 0; y < height; y++) {
            memcpy(&gPicture[(size_t)y * width], (const uint8_t*)data + (size_t)y * pitch, width * 4);
        }
        gPicW = width;
        gPicH = height;
        gPictures++;
    }
    FILE* wav = nullptr;
    uint64_t wavFrames = 0;
    auto audio(Node::Audio::Stream stream) -> void override {
        // the N64 has one stream (AI); drain it whenever samples are pending
        while (stream->pending()) {
            f64 s[2] = { 0, 0 };
            stream->read(s);
            if (!wav) continue;
            int16_t out[2];
            for (int c = 0; c < 2; c++) {
                f64 v = s[c] * 32767.0;
                out[c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
            }
            fwrite(out, 2, 2, wav);
            wavFrames++;
        }
    }
    auto input(Node::Input::Input node) -> void override {
        // only controller port 1's gamepad is driven
        if (auto button = node->cast<Node::Input::Button>()) {
            auto parent = node->parent().lock();
            auto port = parent ? parent->parent().lock() : nullptr;
            if (!port || string{ port->name() } != "Controller Port 1") return;
            auto it = kNodeNames.find(std::string{ string{ node->name() }.data() });
            button->setValue(it != kNodeNames.end() && (gButtons.load() & it->second));
        } else if (auto axis = node->cast<Node::Input::Axis>()) {
            axis->setValue(0);
        }
    }
} gPlatform;

bool savePicture(const std::string& path) {
    std::lock_guard<std::mutex> lock(gVideoMutex);
    if (gPicture.empty()) return false;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fprintf(f, "P6\n%u %u\n255\n", gPicW, gPicH);
    for (uint32_t p : gPicture) {
        uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}

uint32_t parseButtons(const std::string& s) {
    uint32_t v = 0;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, '+')) {
        auto it = kButtonNames.find(tok);
        if (it == kButtonNames.end()) {
            fprintf(stderr, "unknown button '%s'\n", tok.c_str());
            exit(2);
        }
        v |= it->second;
    }
    return v;
}



// ---- seeded battle random numbers (ARES_REF_XCHECK_RNG=<seed>; needs ares-host-hooks.patch, see build.sh) ----
// Stadium Zero's battle draws entropy from osGetCount in Battle_Random and the COM's BattleAi_StirRandom; with a seed,
// those two osGetCount calls return a splitmix64 stream instead, and guRandom (the effects' particles) restarts at each
// move's presentation. The port does the same with PORT_XCHECK_RNG (src/port/xcheck.cpp),
// so both games draw the same numbers although their CPU timing differs. Each battle's start
// (BattleScene_InitializeRuntime) restarts the streams, guRandom and Battle_Random's sum. ARES_REF_XCHECK=<file> writes the drawn values
// ("R <function> <call> 0 <value> frame=<n>"), as the port's PORT_XCHECK trace. Addresses: pokestadium0's
// linker_scripts/jp/symbol_addrs.txt; the battle engine is fragment 0x11 (link address 0x82100000), relocated to
// wherever the pool put it, which Memmap_SetFragmentMap records in gFragments.
namespace xcheck {
constexpr u32 kFragmentTable = 0x800A8D20;   // gFragments[240] {vaddr, size}
constexpr u32 kBattleId = 0x11, kLinkBase = 0x82100000;
constexpr u32 kSetFragmentMap = 0x80001EB0;  // Memmap_SetFragmentMap(id, vaddr, size): main code
constexpr u32 kOsGetCount = 0x80053A80;      // osGetCount: main code
constexpr u32 kGuRandomSeed = 0x80079100;    // guRandom's xseed (lui 0x8008 / lw -0x6F00 in the ROM's guRandom)
constexpr u32 kGuRandomStart = 174823885;    // its initial value (ROM), as Port_GuRandomSeed
constexpr u32 kBattleRandomSum = 0x82199F7C; // Battle_Random's running sum (u8)
constexpr u32 kInitRuntime = 0x82106FE0;     // BattleScene_InitializeRuntime: a battle starts
constexpr u32 kTickCommandTimer = 0x8210C6B0;  // BattleMenu_TickCommandTimer: sync event "cmd" when its timer is 0
constexpr u32 kActionStepIntro = 0x82128CDC;   // BattleScene_ActionStepIntro: guRandom restarts on its first frame
constexpr u32 kUpdatePhase = 0x82131610;       // BattleScene_UpdatePhase (ARES_REF_PHASELOG)
constexpr u32 kBattleScene = 0x8219A5A0;       // sBattleScene: its first word points at the BattleScene
constexpr u32 kCommandTimer = 0x82199EB0;      // its timer (f32)
constexpr u32 kResetAnimStep = 0x82124A40;     // BattleScene_ResetAnimationStep: a move's presentation starts when
constexpr u32 kReadCommandButtons = 0x8210D6BC; // BattleMenu_ReadCommandButtons(active_pokemon): ARES_REF_WEAK_COM
constexpr u32 kActivePokemon = 0x82199DE0;      // sActivePokemon[2]
constexpr u32 kSetHitCount = 0x82128F2C, kSetHitCountEnd = 0x82128FBC; // BattleScene_ActionStepSetHitCount calls it
                                               // (sync event "move"; guRandom restarts)
constexpr u32 kPlayMoveAnim = 0x82101B54;      // BattleAnim_PlayMoveEffectScript(user, move) and
constexpr u32 kPlayChargeAnim = 0x82101BBC;    // BattleAnim_PlayChargingMoveEffectScript: sync event "anim1" when the
constexpr u32 kSide1Active = 0x82199DE8;       // user is 1P's Pokémon (sSide1ActivePokemon) (tools/move_coverage)
struct Caller { u32 from, to; const char* name; }; // [from, to) link addresses of osGetCount's callers to replace
const Caller kCallers[] = { { 0x8210F3F0, 0x8210F4E0, "Battle_Random" }, { 0x82183B60, 0x82183BA8, "BattleAi_StirRandom" } };

// ---- sync points (script "sync <frame> <event>", tools/screen_diff sync:<event>) ----
// The script's frames are route frames: they follow VI frames, except that at each sync point's frame they wait until
// the game reaches that event, so input and pictures after it line up with the game whatever its speed before (the
// N64 slows down in battle effects, the port does not). The port does the same (PORT_SYNC, src/port/os_devices.cpp).
// ARES_REF_SYNC_LOG=<file>: one line per sync point: "sync <n> <event> <frame> waited <VI frames>".
struct SyncPoint { u64 at; std::string event; };
std::vector<SyncPoint> gSync;
size_t gSyncNext = 0;
u64 gSyncLag = 0, gSyncWaitStart = 0;
bool gSyncSeen = false;
FILE* gSyncLog = nullptr;

bool gForce = false;
bool gWeakCom = false; // ARES_REF_WEAK_COM=1: the port's PORT_DEBUG_WEAK_COM (Port_Debug_WeakenCom, command_menu.c)
u32 gSeed = 0, gBase = 0;
FILE* gOut = nullptr;
u64 gFrame = 0;
std::map<u32, u32> gCalls; // per function: calls since the battle engine was mapped

auto& ram() { return Nintendo64::rdram.ram; }
u32 phys(u32 vaddr) { return vaddr & 0x1FFFFFFF; }
// a word as the CPU sees it: from its data cache when the line is there, else from RDRAM
u32 rd32(u32 vaddr) {
    u32 p = phys(vaddr) & ~3u;
    if (vaddr >= 0x80000000u && vaddr < 0xA0000000u) {
        auto& line = Nintendo64::cpu.dcache.line(vaddr);
        if (line.valid() && (line.tagKey & ~1u) == (p & ~0xFFFu)) return line.words[p >> 2 & 3];
    }
    if (p + 4 > ram().size) return 0;
    return ram().Nintendo64::Memory::Writable::read<Nintendo64::Word>(p);
}
// a byte as the CPU would store it: into its data cache line when the line is there, and into RDRAM
void wr8(u32 vaddr, u8 v) {
    u32 p = phys(vaddr) & ~3u, sh = (3 - (vaddr & 3)) * 8;
    if (p + 4 > ram().size) return;
    if (vaddr >= 0x80000000u && vaddr < 0xA0000000u) {
        auto& line = Nintendo64::cpu.dcache.line(vaddr);
        if (line.valid() && (line.tagKey & ~1u) == (p & ~0xFFFu)) {
            u32& w = line.words[p >> 2 & 3];
            w = (w & ~(0xFFu << sh)) | ((u32)v << sh);
        }
    }
    u32 w = ram().Nintendo64::Memory::Writable::read<Nintendo64::Word>(p);
    w = (w & ~(0xFFu << sh)) | ((u32)v << sh);
    ram().Nintendo64::Memory::Writable::write<Nintendo64::Word>(p, w);
}
void wr16(u32 vaddr, u16 v) { wr8(vaddr, (u8)(v >> 8)); wr8(vaddr + 1, (u8)v); }
void wr32(u32 vaddr, u32 v) { for (int i = 0; i < 4; i++) wr8(vaddr + i, (u8)(v >> (24 - 8 * i))); }
u32 rt(u32 link) { return gBase + (link - kLinkBase); } // link address -> runtime address

// splitmix64 of (seed, function, call index): identical in src/port/xcheck.cpp (StreamValue / StreamKey)
u32 streamValue(u32 seed, u32 key, u32 k) {
    u64 z = ((u64)seed << 32) ^ ((u64)key << 24) ^ k;
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (u32)(z ^ (z >> 31));
}

bool returnWith(u64 value) {
    auto& cpu = Nintendo64::cpu;
    cpu.ipu.r[2].u64 = value;
    cpu.pipeline.setPc(cpu.ipu.r[31].u64);
    return true;
}

// the route frame of VI frame <tick> (called once per frame, in order)
u64 routeFrame(u64 tick) {
    if (gSyncNext >= gSync.size()) return tick - gSyncLag;
    const SyncPoint& sp = gSync[gSyncNext];
    if (tick - gSyncLag <= sp.at) {
        gSyncWaitStart = tick;
        return tick - gSyncLag;
    }
    u64 waited = tick - gSyncWaitStart;
    if (gSyncSeen || waited > 6000) { // (a sync that never comes: go on after 100 s rather than hang)
        if (gSyncLog) {
            fprintf(gSyncLog, "sync %zu %s %llu waited %llu%s\n", gSyncNext, sp.event.c_str(), (unsigned long long)sp.at,
                    (unsigned long long)waited, gSyncSeen ? "" : " timeout");
            fflush(gSyncLog);
        }
        gSyncSeen = false;
        gSyncNext++;
        return tick - gSyncLag;
    }
    return tick - ++gSyncLag;
}

// the game reached a sync event: the next sync point, if it is waiting for this event, lets the route clock go on (one
// seen before the route clock gets there counts too: then it does not wait), as the port's Port_SyncEvent
void syncEvent(const char* name) {
    if (gSyncNext < gSync.size() && gSync[gSyncNext].event == name) gSyncSeen = true;
}

void place(u32 base);

// ARES_REF_PHASELOG=<file>: the battle scene's state each time it changes (BattleScene_UpdatePhase's entry), as the
// port's PORT_PHASELOG: "P <frame> <mode> <phase> <step> <substep> <acting side> <move effect kind>". Fields of the
// scene sBattleScene.scene points at: mode 0x1C, phaseStep 0x20, actingSide 0x2C (u16), phase 0x38, substep 0x3C, kind 0x48.
FILE* gPhaseLog = nullptr;
void phaseLog() {
    static u32 last[6] = { 0xFFFFFFF7 };
    u32 sc = rd32(rt(kBattleScene));
    u32 side = rd32(sc + 0x2C) >> 16;
    u32 v[6] = { rd32(sc + 0x1C), rd32(sc + 0x38), rd32(sc + 0x20), rd32(sc + 0x3C), side, rd32(sc + 0x48) };
    if (memcmp(v, last, sizeof v) == 0) return;
    memcpy(last, v, sizeof v);
    fprintf(gPhaseLog, "P %llu %d %d %d %d %d %d\n", (unsigned long long)gFrame, (s32)v[0], (s32)v[1], (s32)v[2], (s32)v[3],
            (s32)v[4], (s32)v[5]);
    fflush(gPhaseLog);
}

bool hook(u32 address) {
    auto& cpu = Nintendo64::cpu;
    if (address == kSetFragmentMap) {
        if (cpu.ipu.r[4].u32 == kBattleId) place(cpu.ipu.r[5].u32);
        return false;
    }
    if (!gBase) return false;
    if (address == rt(kUpdatePhase)) {
        phaseLog();
        return false;
    }
    if (address == rt(kTickCommandTimer)) {
        if (rd32(rt(kCommandTimer)) == 0) syncEvent("cmd"); // (0.0f)
        return false;
    }
    if (address == rt(kActionStepIntro)) { // (as the port's Port_XcheckMoveStart there; substep 0x3C of the scene)
        if (gForce && rd32(rd32(rt(kBattleScene)) + 0x3C) == 0) wr32(kGuRandomSeed, kGuRandomStart);
        return false;
    }
    if (gWeakCom && address == rt(kReadCommandButtons)) {
        // as the port's Port_Debug_WeakenCom, at the same call (its first statement): when the player's Pokémon reads
        // its command buttons, the COM side keeps its shown Pokémon at 1 HP (at most) and the rest at 0. N64 layout:
        // ActivePokemon.side 0x85C, .teamRoster 0x860; TeamRoster.isHuman 0x0 (bit 1: COM), .pokemonPicked[6] 0x1C
        // (BattleMon 0x50, .hp 0x2); BattleSide.displayMon 0x4
        u32 self = cpu.ipu.r[4].u32;
        u32 a = rd32(rt(kActivePokemon)), b = rd32(rt(kActivePokemon) + 4);
        u32 foe = self == a ? b : a;
        u32 mine = rd32(self + 0x860), theirs = rd32(foe + 0x860);
        if (!(rd32(mine) >> 24 & 2) && (rd32(theirs) >> 24 & 2)) {
            u32 shown = rd32(rd32(foe + 0x85C) + 4);
            if (shown) {
                u32 battleHp = rd32(foe + 0x7D0) & 0xFFFF; // state.runtime.hp (0x78C + 0x3A + 0xC): the HP damage uses
                if (battleHp > 1) wr16(foe + 0x7D2, 1);
                for (u32 i = 0; i < 6; i++) {
                    u32 mon = theirs + 0x1C + i * 0x50;
                    u32 hp = rd32(mon) & 0xFFFF; // (hp is the low halfword of the word at +0)
                    if (mon == shown) { if (hp > 1) wr16(mon + 2, 1); }
                    else wr16(mon + 2, 0);
                }
            }
        }
        return false;
    }
    if (address == rt(kPlayMoveAnim) || address == rt(kPlayChargeAnim)) {
        if ((u32)cpu.ipu.r[4].u64 == rd32(rt(kSide1Active))) syncEvent("anim1");
        return false;
    }
    if (address == rt(kResetAnimStep)) {
        u32 ra = (u32)cpu.ipu.r[31].u64;
        if (ra >= rt(kSetHitCount) && ra < rt(kSetHitCountEnd)) {
            if (gForce) wr32(kGuRandomSeed, kGuRandomStart); // (as the port's Port_XcheckMoveStart)
            syncEvent("move");
        }
        return false;
    }
    if (!gForce) return false;
    if (address == kOsGetCount) {
        u32 ra = (u32)cpu.ipu.r[31].u64;
        for (int fn = 0; fn < 2; fn++) {
            if (ra < rt(kCallers[fn].from) || ra >= rt(kCallers[fn].to)) continue;
            u32 key = (u32)fn << 16, k = gCalls[key]++;
            u32 v = streamValue(gSeed, key, k);
            if (gOut) {
                // the caller of Battle_Random / BattleAi_StirRandom: its return address, saved at sp+0x14 / sp+0x1C
                u32 caller = rd32((u32)cpu.ipu.r[29].u64 + (fn == 0 ? 0x14 : 0x1C)) - gBase + kLinkBase;
                fprintf(gOut, "R %s %u 0 %u frame=%llu caller=%08x\n", kCallers[fn].name, k, v, (unsigned long long)gFrame, caller);
                fflush(gOut);
            }
            return returnWith((u64)(s64)(s32)v);
        }
        return false;
    }
    if (address == rt(kInitRuntime)) { // a battle starts (as the port's Port_XcheckBattleStart)
        gCalls.clear();
        wr32(kGuRandomSeed, kGuRandomStart);
        wr8(rt(kBattleRandomSum), 0);
        if (gOut) fprintf(gOut, "B frame=%llu\n", (unsigned long long)gFrame);
    }
    return false;
}

// the battle engine was just mapped (Memmap_SetFragmentMap, before its relocation and the clearing of its bss, so
// nothing is written into it here): hooks at its new address
void place(u32 base) {
    auto& cpu = Nintendo64::cpu;
    gBase = base;
    cpu.hostHooks.addresses = { kSetFragmentMap };
    if (gForce) for (u32 a : { kOsGetCount, rt(kInitRuntime), rt(kActionStepIntro) }) cpu.hostHooks.addresses.push_back(a);
    if (gForce || !gSync.empty()) cpu.hostHooks.addresses.push_back(rt(kResetAnimStep));
    if (!gSync.empty()) cpu.hostHooks.addresses.push_back(rt(kTickCommandTimer));
    if (gWeakCom) cpu.hostHooks.addresses.push_back(rt(kReadCommandButtons));
    if (gPhaseLog) cpu.hostHooks.addresses.push_back(rt(kUpdatePhase));
    if (!gSync.empty()) {
        cpu.hostHooks.addresses.push_back(rt(kPlayMoveAnim));
        cpu.hostHooks.addresses.push_back(rt(kPlayChargeAnim));
    }
    std::sort(cpu.hostHooks.addresses.begin(), cpu.hostHooks.addresses.end());
    cpu.recompiler.reset();
    fprintf(stderr, "[xcheck] battle engine mapped at %08x (frame %llu)\n", base, (unsigned long long)gFrame);
    if (gOut) fprintf(gOut, "F base=%08x frame=%llu\n", base, (unsigned long long)gFrame);
}

void init() {
    const char* rng = getenv("ARES_REF_XCHECK_RNG");
    if (const char* l = getenv("ARES_REF_SYNC_LOG")) gSyncLog = fopen(l, "w");
    gWeakCom = getenv("ARES_REF_WEAK_COM") != nullptr;
    if (const char* l = getenv("ARES_REF_PHASELOG")) gPhaseLog = fopen(l, "w");
    if (!rng && gSync.empty() && !gWeakCom && !gPhaseLog) return;
    gForce = rng != nullptr;
    if (rng) gSeed = (u32)strtoul(rng, nullptr, 0);
    if (const char* out = getenv("ARES_REF_XCHECK")) gOut = fopen(out, "w");
    auto& cpu = Nintendo64::cpu;
    cpu.hostHooks.callback = hook;
    cpu.hostHooks.addresses = { kSetFragmentMap };
    cpu.recompiler.reset();
    if (gForce) fprintf(stderr, "[xcheck] seeded battle random numbers, seed %u\n", gSeed);
    if (!gSync.empty()) fprintf(stderr, "[xcheck] %zu sync points\n", gSync.size());
    if (gWeakCom) fprintf(stderr, "[xcheck] weak COM (one hit wins)\n");
}
} // namespace xcheck

struct Hold { uint64_t from, to; uint32_t buttons; };
struct Fill { uint64_t from, to; uint32_t addr, bytes; uint16_t value; };
struct Action { std::string kind, arg; uint64_t n; };
struct Window { uint64_t from, to; std::string dir; };

} // namespace

static int realMain(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ares_ref <rom.z64> <script> [--load state.bin]\n");
        return 2;
    }
    std::string loadState, firmwareDir;
    for (int i = 3; i + 1 < argc; i++) {
        if (std::string{ argv[i] } == "--load") loadState = argv[i + 1];
    }
    // (an environment variable: nall rewrites path arguments)
    if (const char* fw = getenv("ARES_REF_FIRMWARE")) firmwareDir = fw;

    std::vector<Hold> holds;
    std::vector<Fill> fills;
    std::vector<Window> windows;
    std::multimap<uint64_t, Action> actions;
    uint64_t endFrame = 0, everyN = 0;
    std::string everyDir;
    {
        std::ifstream in(argv[2]);
        std::string line;
        while (std::getline(in, line)) {
            std::stringstream ss(line);
            std::string cmd;
            if (!(ss >> cmd) || cmd[0] == '#') continue;
            if (cmd == "hold") { Hold h; std::string b; ss >> h.from >> h.to >> b; h.buttons = parseButtons(b); holds.push_back(h); }
            else if (cmd == "tap") { Hold h; std::string b; ss >> h.from >> b; h.to = h.from + 1; h.buttons = parseButtons(b); holds.push_back(h); }
            else if (cmd == "shot" || cmd == "save" || cmd == "dump") { Action a; a.kind = cmd; ss >> a.n >> a.arg; actions.insert({ a.n, a }); }
            else if (cmd == "fill") { Fill f; std::string a, v; ss >> f.from >> f.to >> a >> f.bytes >> v; f.addr = std::stoul(a, nullptr, 16) & 0x7fffff; f.value = (uint16_t)std::stoul(v, nullptr, 16); fills.push_back(f); }
            else if (cmd == "window") { Window w; ss >> w.from >> w.to >> w.dir; windows.push_back(w); }
            else if (cmd == "every") { ss >> everyN >> everyDir; }
            else if (cmd == "end") { ss >> endFrame; }
            else if (cmd == "sync") { xcheck::SyncPoint p; ss >> p.at >> p.event; xcheck::gSync.push_back(p); }
            else { fprintf(stderr, "bad script line: %s\n", line.c_str()); return 2; }
        }
    }

    std::ifstream romIn(argv[1], std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(romIn)), std::istreambuf_iterator<char>());
    if (rom.size() < 0x1000 || rom[0] != 0x80) {
        fprintf(stderr, "need a big-endian .z64 ROM\n");
        return 2;
    }

    ares::platform = &gPlatform;
    gPlatform.systemPak = std::make_shared<vfs::directory>();
    // PIF boot firmware (ares ships it in mia/Firmware/Nintendo 64); without it the game never starts
    for (const char* name : { "pif.ntsc.rom", "pif.pal.rom", "pif.sm5.rom" }) {
        std::ifstream fin(firmwareDir + "/" + name, std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        if (bytes.empty()) {
            fprintf(stderr, "missing firmware %s/%s (set ARES_REF_FIRMWARE=<ares>/mia/Firmware/Nintendo 64)\n", firmwareDir.c_str(), name);
            return 2;
        }
        gPlatform.systemPak->append(name, bytes);
    }
    auto cart = std::make_shared<vfs::directory>();
    std::string title((const char*)&rom[0x20], 20);
    while (!title.empty() && (title.back() == ' ' || title.back() == 0)) title.pop_back();
    cart->setAttribute("title", string{ title.c_str() });
    cart->setAttribute("region", string{ "NTSC" });
    cart->setAttribute("cic", string{ detectCic(&rom[0x40]).c_str() });
    cart->append("program.rom", rom);
    // ARES_REF_SRAM=<file>: the cartridge's 32 KiB SRAM (N64 byte order, as the port's saves/stadium.sra). Without it
    // the game starts from a blank save (ares gives the cartridge empty SRAM from its database entry).
    if (const char* sr = getenv("ARES_REF_SRAM")) {
        std::ifstream fin(sr, std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        if (bytes.size() != 32 * 1024) {
            fprintf(stderr, "[ref] SRAM image %s must be 32768 bytes (got %zu)\n", sr, bytes.size());
            return 2;
        }
        cart->append("save.ram", bytes);
        fprintf(stderr, "[ref] sram <- %s\n", sr);
    }
    gPlatform.cartridgePak = cart;

    Nintendo64::option("Enable GPU acceleration", "true");
    Nintendo64::option("Deterministic Entropy", "true");
    Nintendo64::option("Expansion Pak", "true");
    Nintendo64::option("Quality", "SD");
    Nintendo64::option("Recompiler", "true");
    Node::System root;
    if (!Nintendo64::load(root, "[Nintendo] Nintendo 64 (NTSC)")) {
        fprintf(stderr, "Nintendo64::load failed\n");
        return 1;
    }
    if (auto port = root->find<Node::Port>("Cartridge Slot")) { port->allocate(); port->connect(); }
    if (auto port = root->find<Node::Port>("Controller Port 1")) { port->allocate("Gamepad"); port->connect(); }
    // ARES_REF_GBROM=<file.gb> (+ ARES_REF_GBSAV=<file.sav>, ARES_REF_GBBOARD=MBC5): a Transfer Pak with that cartridge
    // in controller 1 (needs ares built with the Game Boy core: ARES_CORES="n64;gb")
    if (const char* gbrom = getenv("ARES_REF_GBROM")) {
        std::ifstream gin(gbrom, std::ios::binary);
        std::vector<uint8_t> gb((std::istreambuf_iterator<char>(gin)), std::istreambuf_iterator<char>());
        gPlatform.gbPak = std::make_shared<vfs::directory>();
        std::string gtitle(gb.size() > 0x144 ? (const char*)&gb[0x134] : "", gb.size() > 0x144 ? 15 : 0);
        gPlatform.gbPak->setAttribute("title", string{ gtitle.c_str() });
        const char* board = getenv("ARES_REF_GBBOARD");
        gPlatform.gbPak->setAttribute("board", string{ board ? board : "MBC5" });
        gPlatform.gbPak->append("program.rom", gb);
        std::vector<uint8_t> sram(32 * 1024, 0xFF);
        if (const char* gbsav = getenv("ARES_REF_GBSAV")) {
            std::ifstream sin(gbsav, std::ios::binary);
            std::vector<uint8_t> d((std::istreambuf_iterator<char>(sin)), std::istreambuf_iterator<char>());
            if (!d.empty()) sram = d;
        }
        gPlatform.gbPak->append("save.ram", sram);
        auto pak = root->find<Node::Port>("Controller Port 1/Gamepad/Pak");
        if (pak) {
            pak->allocate("Transfer Pak");
            pak->connect();
            if (auto slot = root->find<Node::Port>("Controller Port 1/Gamepad/Pak/Transfer Pak/Cartridge Slot")) {
                slot->allocate();
                slot->connect();
                fprintf(stderr, "[ref] Transfer Pak with %s\n", gbrom);
            } else {
                fprintf(stderr, "[ref] no Game Boy cartridge slot (ares built without the Game Boy core?)\n");
            }
        }
    }
    Nintendo64::system.power(false);
    FILE* wavFramesFile = nullptr;
    if (const char* wavPath = getenv("ARES_REF_WAV")) {
        gPlatform.wav = fopen(wavPath, "wb");
        wavFramesFile = fopen((std::string{ wavPath } + ".frames").c_str(), "wb");
        for (auto& stream : root->find<Node::Audio::Stream>()) stream->setResamplerFrequency(48000.0);
        fprintf(stderr, "[ref] audio -> %s (48000 Hz s16 stereo)\n", wavPath);
    }

    if (!loadState.empty()) {
        std::ifstream sin(loadState, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(sin)), std::istreambuf_iterator<char>());
        serializer s(data.data(), (u32)data.size());
        if (!Nintendo64::system.unserialize(s)) {
            fprintf(stderr, "could not load state %s\n", loadState.c_str());
            return 1;
        }
        fprintf(stderr, "[ref] loaded state %s\n", loadState.c_str());
    }

    auto t0 = std::chrono::steady_clock::now();
    xcheck::init();
    uint64_t lastFrame = 0;
    for (uint64_t tick = 1;; tick++) {
        xcheck::gFrame = tick;
        // the script's frames are route frames: VI frames, except that they wait at each sync point for its event
        const uint64_t frame = xcheck::routeFrame(tick);
        if (endFrame && frame > endFrame) break;
        const bool fresh = frame != lastFrame; // (while waiting at a sync point the route frame stays put)
        lastFrame = frame;
        uint32_t b = 0;
        for (auto& h : holds) {
            if (frame >= h.from && frame <= h.to) b |= h.buttons;
        }
        gButtons = b;
        for (auto& f : fills) {
            if (frame < f.from || frame > f.to) continue;
            auto& ram = Nintendo64::rdram.ram;
            for (u32 i = 0; i + 2 <= f.bytes; i += 2) {
                u32 a = (f.addr + i) & ~3u;
                u32 w = ram.Nintendo64::Memory::Writable::read<Nintendo64::Word>(a);
                if (((f.addr + i) & 2) == 0) w = (w & 0xffff) | ((u32)f.value << 16);
                else w = (w & 0xffff0000) | f.value;
                ram.Nintendo64::Memory::Writable::write<Nintendo64::Word>(a, w);
            }
        }
        uint64_t before = gPictures.load();
        Nintendo64::system.run();
        if (wavFramesFile) {
            uint32_t n = (uint32_t)gPlatform.wavFrames;
            fwrite(&n, 4, 1, wavFramesFile);
        }
        bool inWindow = false;
        for (auto& w : windows) inWindow |= frame >= w.from && frame <= w.to;
        bool wantShot = fresh && (actions.count(frame) || (everyN && frame % everyN == 0) || inWindow);
        if (getenv("REF_DEBUG") && frame % 30 == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); fprintf(stderr, "[ref] frame %llu pictures %llu\n", (unsigned long long)frame, (unsigned long long)gPictures.load()); }
        if (wantShot) { // the screen thread delivers the picture shortly after the frame
            for (int i = 0; i < 200 && gPictures.load() == before; i++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (fresh && everyN && frame % everyN == 0) {
            char name[64];
            snprintf(name, sizeof(name), "/f_%06llu.ppm", (unsigned long long)frame);
            savePicture(everyDir + name);
        }
        for (auto& w : windows) {
            if (!fresh || frame < w.from || frame > w.to) continue;
            char name[64];
            snprintf(name, sizeof(name), "/f_%06llu.ppm", (unsigned long long)frame);
            savePicture(w.dir + name);
        }
        auto range = fresh ? actions.equal_range(frame) : std::make_pair(actions.end(), actions.end());
        for (auto it = range.first; it != range.second; ++it) {
            const Action& a = it->second;
            if (a.kind == "shot") {
                fprintf(stderr, "[ref] frame %llu -> %s (%s)\n", (unsigned long long)frame, a.arg.c_str(), savePicture(a.arg) ? "ok" : "no picture");
            } else if (a.kind == "dump") {
                auto& ram = Nintendo64::rdram.ram;
                std::vector<uint8_t> buf(ram.size);
                for (u32 i = 0; i + 4 <= ram.size; i += 4) {
                    u32 w = ram.Nintendo64::Memory::Writable::read<Nintendo64::Word>(i);
                    buf[i] = w >> 24; buf[i + 1] = w >> 16; buf[i + 2] = w >> 8; buf[i + 3] = w;
                }
                std::ofstream out(a.arg, std::ios::binary);
                out.write((const char*)buf.data(), buf.size());
                fprintf(stderr, "[ref] frame %llu RDRAM -> %s\n", (unsigned long long)frame, a.arg.c_str());
            } else if (a.kind == "save") {
                serializer s = Nintendo64::system.serialize();
                std::ofstream out(a.arg, std::ios::binary);
                out.write((const char*)s.data(), s.size());
                fprintf(stderr, "[ref] frame %llu state -> %s (%u bytes)\n", (unsigned long long)frame, a.arg.c_str(), (unsigned)s.size());
            }
        }
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    fprintf(stderr, "[ref] %llu frames in %.1f s, %llu pictures\n", (unsigned long long)endFrame, secs, (unsigned long long)gPictures.load());
    // ARES_REF_GBSAV_OUT=<file>: the Transfer Pak cartridge's battery RAM as the game left it
    if (const char* gbout = getenv("ARES_REF_GBSAV_OUT"); gbout && gPlatform.gbPak) {
        Nintendo64::controllerPort1.save();
        if (auto fp = gPlatform.gbPak->read("save.ram")) {
            std::vector<uint8_t> bytes(fp->size());
            fp->seek(0);
            fp->read({ bytes.data(), bytes.size() });
            std::ofstream(gbout, std::ios::binary).write((const char*)bytes.data(), bytes.size());
            fprintf(stderr, "[ref] gb save -> %s (%zu bytes)\n", gbout, bytes.size());
        }
    }
    if (gPlatform.wav) fclose(gPlatform.wav);
    if (wavFramesFile) fclose(wavFramesFile);
    // ARES_REF_SRAM_OUT=<file>: the cartridge SRAM as the game left it (N64 byte order, like ARES_REF_SRAM)
    if (const char* flOut = getenv("ARES_REF_SRAM_OUT")) {
        Nintendo64::cartridge.save();
        if (auto fp = cart->read("save.ram")) {
            std::vector<uint8_t> bytes(fp->size());
            fp->seek(0);
            fp->read({ bytes.data(), bytes.size() });
            std::ofstream(flOut, std::ios::binary).write((const char*)bytes.data(), bytes.size());
            fprintf(stderr, "[ref] sram -> %s (%zu bytes)\n", flOut, bytes.size());
        } else {
            fprintf(stderr, "[ref] no SRAM to write (ARES_REF_SRAM_OUT needs ARES_REF_SRAM)\n");
        }
    }
    Nintendo64::system.unload();
    return 0;
}

// nall provides the process entry point and calls nall::main with the arguments (without the program path).
auto nall::main(nall::Arguments arguments) -> void {
    std::vector<std::string> args{ "ares_ref" };
    for (auto& a : arguments) args.push_back(a.data());
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    exit(realMain((int)argv.size(), argv.data()));
}
