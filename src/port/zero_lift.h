// Data lifter: Stadium Zero's data, rodata and bss live in the decomp's assembly, not in C. The port defines host
// storage for every symbol the C code references (src/port/generated/zero_lift_data.c, tools/zero/gen_lift.py) and
// fills it from the user's ROM: big-endian scalars swapped, 32-bit pointers widened and relocated to host addresses,
// function pointers mapped to the statically linked functions.
#ifndef ZERO_LIFT_H
#define ZERO_LIFT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { ZLIFT_SCALAR, ZLIFT_PTR, ZLIFT_FNPTR, ZLIFT_MAYBEPTR, ZLIFT_REC, ZLIFT_BITS, ZLIFT_RAW };

typedef struct ZLiftType {
    uint32_t n64Size; // one element on the N64 (ILP32, big-endian)
    uint32_t hostSize;
    uint32_t opStart;
    uint32_t opCount;
} ZLiftType;

typedef struct ZLiftOp {
    int32_t kind;
    int32_t n64Off, hostOff;
    int32_t size; // scalar bytes, or the bitfield's storage unit
    int32_t count;
    int32_t n64Stride, hostStride;
    int32_t sub; // ZLIFT_REC: element type; ZLIFT_PTR: pointee type (-1 unknown)
    int32_t n64Bit, hostBit, width, isSigned; // ZLIFT_BITS
} ZLiftOp;

typedef struct ZLiftSym {
    uint32_t addr;   // N64 address
    uint32_t n64Ext; // bytes covered on the N64
    uint32_t count;  // elements of the type
    int32_t type;
    int32_t image;
    int32_t bss;
    uint32_t rom; // offset of the first byte in the image (decompressed)
    void* host;
} ZLiftSym;

typedef struct ZLiftImage {
    const char* name;
    uint32_t vram;
    uint32_t rom;
} ZLiftImage;

typedef struct ZLiftFunc {
    uint32_t addr;
    void* fn;
} ZLiftFunc;

typedef struct ZLiftPart {
    uint32_t vram, size, rom; // rom: offset in the (decompressed) image
    int32_t image;
} ZLiftPart;

#define ZLIFT_STORAGE(name, size) __attribute__((aligned(16))) unsigned char name[size]

extern const ZLiftType gZLiftTypes[];
extern const uint32_t gZLiftTypeCount;
extern const ZLiftOp gZLiftOps[];
extern const ZLiftSym gZLiftSyms[];
extern const uint32_t gZLiftSymCount;
extern const ZLiftImage gZLiftImages[];
extern const uint32_t gZLiftImageCount;
extern const uint32_t gZLiftBounds[];
extern const uint32_t gZLiftBoundCount;
extern const ZLiftFunc gZLiftFuncs[];
extern const uint32_t gZLiftFuncCount;
extern const ZLiftPart gZLiftParts[];
extern const uint32_t gZLiftPartCount;

// Fills (or resets) every symbol of an image from the ROM; bss is zeroed. Image 0 is the resident code, image n+1
// is fragment n.
void ZLift_LoadImage(int image);
// PORT_LIFT_AUDIT=1: lift every image, report pointers the lifter had to redirect, exit (zero_lift.c).
void ZLift_AuditAll(void);
// The image whose vram slot holds addr (-1 if none).
int ZLift_ImageOfAddr(uint32_t addr);
// Host address of an N64 address in lifted data or code (NULL if unknown). Unnamed data is lifted on first use as
// bytes.
void* ZLift_Resolve(uint32_t addr);
// Whether a host pointer lies in lifted data that kept its N64 (big-endian byte) form: byte-typed symbols and unnamed
// data. Display lists and geo layouts there are read as raw ROM-format data.
int ZLift_IsRaw(const void* host);
// 2 / 4: host pointer into lifted data typed as 16- / 32-bit scalars (host byte order); 1: lifted data in N64 byte
// form; 0: not lifted data.
int ZLift_ScalarSize(const void* host);
// The statically linked host function for an N64 function address (NULL if none).
void* ZLift_FindFunc(uint32_t addr);
// As ZLift_FindFunc, and also follows a `j target` jump stub at addr.
void* ZLift_ResolveCode(uint32_t addr);
// Whether a host pointer lies in any lifted data (raw or typed).
int ZLift_IsLifted(const void* host);
// N64 address of a host pointer into lifted data or a known function (0 if none).
uint32_t ZLift_N64Addr(const void* host);

#ifdef __cplusplus
}
#endif

#endif
