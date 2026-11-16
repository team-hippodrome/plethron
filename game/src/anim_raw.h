/* PORT: big-endian animation data. */
#ifndef ANIM_RAW_H
#define ANIM_RAW_H

// Animation data of models loaded from ROM (model blobs, byte-typed lifted data) keeps its N64 form: big-endian
// 16-bit fields and samples, 32-bit link addresses, bit-packed streams of big-endian 16-bit words. Data the lifter
// typed (TransformAnimData, EventTrackData declared in C) is native. These accessors read either form; the raw-ness
// of a header is decided from its own address, and gAnimCurveRaw / gAnimEventRaw hold it for the animation being
// evaluated (set by ModelAnim_BeginCurveContext / ModelAnim_PrepareEventTrackRender).

#include "ultra64.h"
#include "port_endian.h"

int Port_VIsRaw(const void* p);
int ZLift_IsRaw(const void* host);

extern int gAnimCurveRaw;
extern int gAnimEventRaw;

static inline int Anim_IsRaw(const void* p) {
    return p != NULL && (Port_VIsRaw(p) || ZLift_IsRaw(p));
}

static inline u16 Anim_ReadU16(const void* p, int raw) {
    const u8* b = (const u8*)p;
    return raw ? (u16)((b[0] << 8) | b[1]) : *(const u16*)p;
}

static inline u32 Anim_ReadU32(const void* p, int raw) {
    const u8* b = (const u8*)p;
    return raw ? ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3] : *(const u32*)p;
}

// TransformAnimData / EventTrackData header fields (same layout up to 0x0C on the N64)
#define ANIM_FLAGS(d) ((s16)Anim_ReadU16((const u8*)(d) + 0x00, Anim_IsRaw(d)))
#define ANIM_START(d) ((s16)Anim_ReadU16((const u8*)(d) + 0x04, Anim_IsRaw(d)))
#define ANIM_LOOP(d) ((s16)Anim_ReadU16((const u8*)(d) + 0x06, Anim_IsRaw(d)))
#define ANIM_COUNT(d) Anim_ReadU16((const u8*)(d) + 0x08, Anim_IsRaw(d))
#define ANIM_END(d) Anim_ReadU16((const u8*)(d) + 0x0A, Anim_IsRaw(d))

// Signed bit field `offset` of `length` bits in a stream of 16-bit words (PackedBits_ReadSigned), in either form.
static inline s16 Anim_PackedBitsReadSigned(const s16* packed, s32 offset, s32 length, int raw) {
    const s16* w = &packed[(offset * length) / 16];
    s32 v = (s32)(((u32)Anim_ReadU16(&w[0], raw) << 16) | Anim_ReadU16(&w[1], raw));

    v <<= ((offset * length) % 16);
    v >>= (32 - length);
    return (s16)v;
}

#endif
