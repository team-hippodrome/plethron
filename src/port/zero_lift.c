// Data lifter runtime (see zero_lift.h). The tables come from tools/zero/gen_lift.py.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zero_lift.h"

const uint8_t* Port_RomBytes(void);
size_t Port_RomLength(void);

static int sTrace = -1;
#define TRACE (sTrace < 0 ? (sTrace = getenv("PORT_TRACE_LIFT") != NULL) : sTrace)

// Unnamed data that a lifted pointer points at, lifted on first use (as the pointer's pointee type, or bytes).
typedef struct LazyBlock {
    uint32_t addr, n64Ext, count, rom;
    int32_t type, image;
    uint8_t* host;
    uint32_t hostSize;
} LazyBlock;
static LazyBlock* sLazy;
static uint32_t sLazyCount, sLazyCap;

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// ---- image bytes: fragments in the ROM are mostly PERS-SZP (Yay0) compressed ----------------------------------------
typedef struct ImageBytes {
    const uint8_t* data;
    size_t size;
} ImageBytes;
static ImageBytes* sImages;

static void yay0Decode(const uint8_t* src, uint8_t* dst, uint32_t outSize) {
    uint32_t linkOff = be32(src + 8), chunkOff = be32(src + 12);
    uint32_t maskOff = 16, mask = 0, bits = 0, out = 0;
    while (out < outSize) {
        if (bits == 0) {
            mask = be32(src + maskOff);
            maskOff += 4;
            bits = 32;
        }
        if (mask & 0x80000000u) {
            dst[out++] = src[chunkOff++];
        } else {
            uint32_t link = ((uint32_t)src[linkOff] << 8) | src[linkOff + 1];
            linkOff += 2;
            uint32_t dist = (link & 0xFFF) + 1;
            uint32_t len = link >> 12;
            if (len == 0) {
                len = src[chunkOff++] + 0x12;
            } else {
                len += 2;
            }
            for (uint32_t i = 0; i < len && out < outSize; i++, out++) {
                dst[out] = dst[out - dist];
            }
        }
        mask <<= 1;
        bits--;
    }
}

// The image as the game sees it after loading (decompressed, relocation words restored, bss part zeroed).
static const uint8_t* imageBytes(int image, size_t* size) {
    const uint8_t* rom = Port_RomBytes();
    size_t romSize = Port_RomLength();
    if (sImages == NULL) {
        sImages = calloc(gZLiftImageCount, sizeof(*sImages));
    }
    ImageBytes* ib = &sImages[image];
    if (ib->data == NULL && rom != NULL) {
        uint32_t off = gZLiftImages[image].rom;
        if (off + 0x18 <= romSize && memcmp(rom + off, "PERS-SZP", 8) == 0) {
            const uint8_t* h = rom + off;
            uint32_t headerSize = be32(h + 8), size1 = be32(h + 12), size2 = be32(h + 16), relocs = be32(h + 20);
            uint8_t* out = calloc(1, size2 + 16);
            yay0Decode(h + headerSize, out, be32(h + headerSize + 4) < size1 ? be32(h + headerSize + 4) : size1);
            for (uint32_t i = 0; i < relocs; i++) {
                const uint8_t* pair = h + 0x18 + i * 8; // (value, offset)
                memcpy(out + be32(pair + 4), pair, 4);
            }
            ib->data = out;
            ib->size = size2;
        } else {
            ib->data = rom + off;
            ib->size = romSize - off;
        }
    }
    *size = ib->size;
    return ib->data;
}

static const ZLiftSym* findSym(uint32_t addr) {
    uint32_t lo = 0, hi = gZLiftSymCount;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (gZLiftSyms[mid].addr <= addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return NULL;
    }
    const ZLiftSym* s = &gZLiftSyms[lo - 1];
    // one past the end counts too (end-of-table pointers)
    if (addr <= s->addr + s->n64Ext) {
        return s;
    }
    return NULL;
}

static void* findFunc(uint32_t addr) {
    uint32_t lo = 0, hi = gZLiftFuncCount;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (gZLiftFuncs[mid].addr < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < gZLiftFuncCount && gZLiftFuncs[lo].addr == addr) {
        return gZLiftFuncs[lo].fn;
    }
    return NULL;
}

static const ZLiftPart* findPart(uint32_t addr) {
    for (uint32_t i = 0; i < gZLiftPartCount; i++) {
        if (addr >= gZLiftParts[i].vram && addr - gZLiftParts[i].vram < gZLiftParts[i].size) {
            return &gZLiftParts[i];
        }
    }
    return NULL;
}

static uint32_t nextBound(uint32_t addr) {
    uint32_t lo = 0, hi = gZLiftBoundCount;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (gZLiftBounds[mid] <= addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < gZLiftBoundCount ? gZLiftBounds[lo] : addr + 4;
}

// Host offset of an N64 byte offset inside count elements of a type (-1 if it falls in padding).
static long translate(int type, uint32_t off) {
    const ZLiftType* t = &gZLiftTypes[type];
    long base = 0;
    if (t->n64Size != 0 && off >= t->n64Size) {
        base = (long)(off / t->n64Size) * t->hostSize;
        off %= t->n64Size;
    }
    for (uint32_t i = 0; i < t->opCount; i++) {
        const ZLiftOp* o = &gZLiftOps[t->opStart + i];
        if (o->kind == ZLIFT_REC) {
            uint32_t ext = (uint32_t)o->n64Stride * o->count;
            if (off >= (uint32_t)o->n64Off && off < o->n64Off + ext) {
                uint32_t j = (off - o->n64Off) / o->n64Stride;
                long r = translate(o->sub, (off - o->n64Off) % o->n64Stride);
                return r < 0 ? r : base + o->hostOff + (long)j * o->hostStride + r;
            }
        } else {
            uint32_t ext = (uint32_t)o->n64Stride * (o->count - 1) + o->size;
            if (off >= (uint32_t)o->n64Off && off < o->n64Off + ext) {
                uint32_t j = (off - o->n64Off) / o->n64Stride;
                uint32_t r = (off - o->n64Off) % o->n64Stride;
                long h = base + o->hostOff + (long)j * o->hostStride;
                if (r >= (uint32_t)o->size) {
                    return -1;
                }
                if (o->kind == ZLIFT_SCALAR || o->kind == ZLIFT_BITS) {
                    return h + (o->size - 1 - r); // byte r of a big-endian value
                }
                return h;
            }
        }
    }
    return off < t->n64Size ? -1 : base;
}

static void* resolve(uint32_t v, int pointee);

static void liftElems(const uint8_t* src, uint8_t* dst, int type, uint32_t count);

static void applyOp(const ZLiftOp* o, const uint8_t* src, uint8_t* dst) {
    for (int j = 0; j < o->count; j++) {
        const uint8_t* s = src + o->n64Off + (long)j * o->n64Stride;
        uint8_t* d = dst + o->hostOff + (long)j * o->hostStride;
        switch (o->kind) {
            case ZLIFT_SCALAR:
                for (int b = 0; b < o->size; b++) {
                    d[b] = s[o->size - 1 - b];
                }
                break;
            case ZLIFT_RAW:
                memcpy(d, s, o->size);
                break;
            case ZLIFT_PTR: {
                void* p = resolve(be32(s), o->sub);
                memcpy(d, &p, sizeof(p));
                break;
            }
            case ZLIFT_FNPTR: {
                uint32_t v = be32(s);
                void* p = v == 0 ? NULL : findFunc(v);
                if (p == NULL && v != 0) {
                    p = resolve(v, -1);
                }
                memcpy(d, &p, sizeof(p));
                break;
            }
            case ZLIFT_MAYBEPTR: {
                uint32_t v = be32(s);
                uintptr_t p = v;
                if (v >= 0x80000000u) {
                    void* h = findFunc(v);
                    if (h == NULL && (findSym(v) != NULL || findPart(v) != NULL)) {
                        h = resolve(v, -1);
                    }
                    if (h != NULL) {
                        p = (uintptr_t)h;
                    }
                }
                memcpy(d, &p, sizeof(p));
                break;
            }
            case ZLIFT_REC:
                liftElems(s, d, o->sub, 1);
                break;
            case ZLIFT_BITS: {
                uint64_t unit = 0, host = 0;
                int bits = o->size * 8;
                for (int b = 0; b < o->size; b++) {
                    unit = (unit << 8) | s[b];
                    host |= (uint64_t)d[b] << (8 * b);
                }
                uint64_t mask = o->width >= 64 ? ~0ull : ((1ull << o->width) - 1);
                uint64_t val = (unit >> (bits - o->n64Bit - o->width)) & mask;
                host = (host & ~(mask << o->hostBit)) | (val << o->hostBit);
                for (int b = 0; b < o->size; b++) {
                    d[b] = (uint8_t)(host >> (8 * b));
                }
                break;
            }
        }
    }
}

static void liftElems(const uint8_t* src, uint8_t* dst, int type, uint32_t count) {
    const ZLiftType* t = &gZLiftTypes[type];
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t k = 0; k < t->opCount; k++) {
            applyOp(&gZLiftOps[t->opStart + k], src + (size_t)i * t->n64Size, dst + (size_t)i * t->hostSize);
        }
    }
}

static int sWarnedUnknown;

static void* resolve(uint32_t v, int pointee) {
    if (v == 0) {
        return NULL;
    }
    if (v < 0x80000000u || v >= 0xA0000000u) {
        return (void*)(uintptr_t)v; // segment addresses, offsets and small numbers stay as they are
    }
    const ZLiftSym* s = findSym(v);
    if (s != NULL) {
        if (v == s->addr) {
            return s->host;
        }
        if (v == s->addr + s->n64Ext) {
            return (uint8_t*)s->host + (size_t)gZLiftTypes[s->type].hostSize * s->count;
        }
        long off = translate(s->type, v - s->addr);
        if (off < 0) {
            off = 0;
        }
        return (uint8_t*)s->host + off;
    }
    void* fn = findFunc(v);
    if (fn != NULL) {
        return fn;
    }
    for (uint32_t i = 0; i < sLazyCount; i++) {
        LazyBlock* b = &sLazy[i];
        if (v >= b->addr && v < b->addr + b->n64Ext) {
            long off = b->type >= 0 ? translate(b->type, v - b->addr) : (long)(v - b->addr);
            return b->host + (off < 0 ? 0 : off);
        }
    }
    const ZLiftPart* part = findPart(v);
    if (part == NULL) {
        if (TRACE || sWarnedUnknown++ < 20) {
            fprintf(stderr, "[lift] pointer 0x%08X is not in any image's data; left as is\n", v);
        }
        return (void*)(uintptr_t)v;
    }
    // Unnamed data: lift from here to the next symbol boundary.
    uint32_t end = nextBound(v);
    if (end > part->vram + part->size) {
        end = part->vram + part->size;
    }
    if (sLazyCount == sLazyCap) {
        sLazyCap = sLazyCap ? sLazyCap * 2 : 256;
        sLazy = realloc(sLazy, sLazyCap * sizeof(*sLazy));
    }
    LazyBlock* b = &sLazy[sLazyCount++];
    b->addr = v;
    b->n64Ext = end - v;
    b->type = pointee;
    b->image = part->image;
    b->rom = part->rom + (v - part->vram); // offset in the image
    if (pointee >= 0 && gZLiftTypes[pointee].n64Size != 0) {
        b->count = b->n64Ext / gZLiftTypes[pointee].n64Size;
        if (b->count == 0) {
            b->count = 1;
        }
        b->hostSize = b->count * gZLiftTypes[pointee].hostSize;
    } else {
        b->type = -1;
        b->count = b->n64Ext;
        b->hostSize = b->n64Ext;
    }
    b->host = calloc(1, b->hostSize + 16);
    uint8_t* host = b->host;
    int type = b->type;
    uint32_t count = b->count, rom = b->rom, ext = b->n64Ext;
    if (TRACE) {
        fprintf(stderr, "[lift] unnamed 0x%08X..0x%08X as type %d x%u\n", v, end, type, count);
    }
    // sLazy may move while lifting (nested lazies): use the copies above
    size_t isz;
    const uint8_t* img = imageBytes(b->image, &isz);
    if (img != NULL && rom + ext <= isz) {
        if (type >= 0) {
            liftElems(img + rom, host, type, count);
        } else {
            memcpy(host, img + rom, ext);
        }
    }
    return host;
}

void* ZLift_Resolve(uint32_t addr) {
    return resolve(addr, -1);
}

uint32_t ZLift_N64Addr(const void* host) {
    const uint8_t* h = host;
    for (uint32_t i = 0; i < gZLiftSymCount; i++) {
        const ZLiftSym* s = &gZLiftSyms[i];
        size_t hs = (size_t)gZLiftTypes[s->type].hostSize * s->count;
        if (h >= (const uint8_t*)s->host && h < (const uint8_t*)s->host + hs) {
            // only exact element starts map back precisely; good enough for the callers (identity checks)
            const ZLiftType* t = &gZLiftTypes[s->type];
            size_t off = h - (const uint8_t*)s->host;
            return s->addr + (uint32_t)(off / t->hostSize) * t->n64Size;
        }
    }
    for (uint32_t i = 0; i < gZLiftFuncCount; i++) {
        if (gZLiftFuncs[i].fn == host) {
            return gZLiftFuncs[i].addr;
        }
    }
    return 0;
}

int ZLift_ImageOfAddr(uint32_t addr) {
    for (uint32_t i = 1; i < gZLiftImageCount; i++) {
        if ((addr & 0xFFF00000u) == (gZLiftImages[i].vram & 0xFFF00000u)) {
            return (int)i;
        }
    }
    return (addr >= 0x80000000u && addr < 0x81000000u) ? 0 : -1;
}

void ZLift_LoadImage(int image) {
    size_t romSize;
    const uint8_t* rom = imageBytes(image, &romSize);
    uint32_t n = 0;

    for (uint32_t i = 0; i < gZLiftSymCount; i++) {
        const ZLiftSym* s = &gZLiftSyms[i];
        if (s->image != image) {
            continue;
        }
        size_t hs = (size_t)gZLiftTypes[s->type].hostSize * s->count;
        memset(s->host, 0, hs);
        if (!s->bss && rom != NULL && s->rom + s->n64Ext <= romSize) {
            liftElems(rom + s->rom, s->host, s->type, s->count);
        }
        n++;
    }
    for (uint32_t i = 0; i < sLazyCount; i++) {
        LazyBlock b = sLazy[i];
        if (b.image == image && rom != NULL && b.rom + b.n64Ext <= romSize) {
            memset(b.host, 0, b.hostSize);
            if (b.type >= 0) {
                liftElems(rom + b.rom, b.host, b.type, b.count);
            } else {
                memcpy(b.host, rom + b.rom, b.n64Ext);
            }
        }
    }
    if (TRACE) {
        fprintf(stderr, "[lift] image %d (%s): %u symbols\n", image,
                image >= 0 && (uint32_t)image < gZLiftImageCount ? gZLiftImages[image].name : "?", n);
    }
}
