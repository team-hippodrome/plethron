/* Declarations for src/jpeg_stream.c. */
#ifndef JPEG_STREAM_H
#define JPEG_STREAM_H

#include "global.h"

typedef struct ret_func_8000484C {
	/* 0x00 */ char unk00[0x90];
	/* 0x90 */ s32 unk_90;
} ret_func_8000484C; // size >= 0x94

typedef struct Yay0 {
    /* 0x00 */ char magic[0x4]; // Yay0
    /* 0x04 */ u32 decompressed_size;
    /* 0x08 */ u32 link_table_offset;
    /* 0x0C */ u32 byte_chunk_offset;
} Yay0; // size = 0x10

// The markers found in a JPEG stream (Jpeg_ParseStreamMarkers).
typedef struct JpegStreamInfo {
    /* 0x00 */ u8 mode; // @unverified: 0 if the second component ID is 0x21, else 2 (Jpeg_ParseStreamMarkers); copied to JpegDecoder.mode
    /* 0x01 */ u8 hasRestartMarkers; // @unverified: set when an RST0-RST7 marker is seen (Jpeg_ParseStreamMarkers)
    /* 0x02 */ char unk02[0x2];
    /* 0x04 */ s16 width;
    /* 0x06 */ s16 height;
    /* 0x08 */ u8* quantization_table;
    /* 0x0C */ u8* huffman_table;
    /* 0x10 */ u8* start_of_frame;
    /* 0x14 */ u8* start_of_scan;
} JpegStreamInfo; // size = 0x18

// The NJPEG task data of Jpeg_DecodeImage: the first six fields of JpegTaskData (jpeg_decoder.h), in a 0x18-byte stack slot.
typedef struct JpegTaskParams {
    /* 0x00 */ uintptr_t address; // @unverified: physical address of the MCU buffer, set per 16x16 block (Jpeg_DecodeImage); JpegTaskData.address
    /* 0x04 */ uintptr_t mbCount; // @unverified: 1 (Jpeg_BuildDspOutput); JpegTaskData.mbCount
    /* 0x08 */ uintptr_t mode; // @unverified: JpegStreamInfo.mode; JpegTaskData.mode
    /* 0x0C */ uintptr_t qTableYPtr; // @unverified: JpegTaskData.qTableYPtr
    /* 0x10 */ uintptr_t qTableUPtr; // @unverified: JpegTaskData.qTableUPtr
    /* 0x14 */ uintptr_t qTableVPtr; // @unverified: same table as qTableUPtr; JpegTaskData.qTableVPtr
} JpegTaskParams; // size = 0x18 on the N64; PORT: pointer-sized fields, as the host JPEG task (src/port/jpeg_task.cpp) reads them

#include "scheduler.h"

struct JpegDecoder;
struct JpegHuffmanTable;

// jpeg_stream.c
s32 Jpeg_ReadBigEndianU16(u8* arg0);
void Jpeg_ParseStreamMarkers(JpegStreamInfo* arg0, u8* arg1);
void Jpeg_InitializeDspTask(SchedTask* arg0, JpegTaskParams* arg1);
void Jpeg_BuildDspInput(struct JpegDecoder* arg0, struct JpegHuffmanTable* arg1, JpegStreamInfo* arg2);
void Jpeg_BuildDspOutput(JpegTaskParams* arg0, u8(arg1)[2][0x80], JpegStreamInfo* arg2);
s32 Jpeg_DecodeImage(uintptr_t addr, s32 arg1, u8* arg2);

#endif // JPEG_STREAM_H
