/* PORT: not part of the decomp. Subtitles for the announcer's voice lines, after pokestadium0's feat/subs branch.
 *
 * Every sound effect starts in func_8003815C (libnumus/player.c), which tells Subtitles_OnSound the effect table and
 * the effect id. The announcer's lines are the effects of JIKKYOU.WBK (effect table D_800B41A8); line `id` is the row
 * SUBTITLE_ANNOUNCER_<id as two hex digits> of translations/ai.csv / human.csv, so the text comes in every translated
 * language and human translations override it like any other string. The original language (にほんご) shows none.
 * The line stays up while its sound plays and SUBTITLE_HOLD_FRAMES after, on a translucent box above the battle
 * message box, wrapped to the box at run time. Options > Subtitles (shown in the translated languages) turns them
 * off; the choice is the port setting gPort.Subtitles, so the N64 save stays as the cartridge writes it.
 */
#include "global.h"
#include "font.h"
#include "graphics_textures.h" // font_context
#include "stage_loader.h"
#include "subtitles.h"
#include "text_lookup.h"
#include "unicode_font.h"
#include "audio_commands.h" // D_800B41A8

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Ui_DrawPrimColorRect(s32 x, s32 y, s32 w, s32 h, s32 r, s32 g, s32 b, s32 a);
s32 Audio_QueueSetMonoOutputCommand(s32 handle); // despite the name: nonzero while sound `handle` plays

// Frames a subtitle stays up after its clip ends, so short clips can be read.
#define SUBTITLE_HOLD_FRAMES 45
// The box, in 320x240 coordinates (doubled on a 640x480 stage): left edge, width and the y its bottom sits at, just
// above the battle message box (y 179). Text is inset by SUBTITLE_PAD on each side.
#define SUBTITLE_BOX_X 8
#define SUBTITLE_BOX_W 216
#define SUBTITLE_BOX_BOTTOM 176
#define SUBTITLE_PAD 6
// Glyph height in 320x240 pixels (the trainer name boxes use 12).
#define SUBTITLE_GLYPH_HEIGHT 12
#define SUBTITLE_MAX_LINES 6

// Written by Subtitles_OnSound, which runs wherever the sound starts (the audio thread for queued lines).
static volatile s32 sNewId;
static volatile s32 sNewHandle;
static volatile u32 sNewSerial;

// The line on screen, owned by Subtitles_Draw (the game thread).
static u32 sSeenSerial;
static s32 sShownId = -1;
static s32 sShownHandle;
static s32 sHold;
static s32 sTestId = -2; // PORT_SUBTITLE_TEST, -1 when unset

void Subtitles_OnSound(const void* effects, s32 id, s32 handle) {
    if ((effects == NULL) || (effects != (const void*) D_800B41A8) || (handle == 0) || (handle == -1) || (id < 0) ||
        (id > 0xFF)) {
        return;
    }
    sNewId = id;
    sNewHandle = handle;
    sNewSerial++;
}

// Where a line may break in front of `cp` without a space: between CJK characters (Japanese has no spaces), but not
// in front of closing punctuation or the long-vowel mark.
static s32 Subtitles_CanBreakBefore(u32 cp) {
    static const u32 kNoBreakBefore[] = { 0x3001, 0x3002, 0xFF01, 0xFF1F, 0xFF0C, 0xFF0E, 0x2026, 0x30FC, 0x300D,
                                          0x300F, 0xFF09, 0x3063, 0x30C3, 0x3083, 0x3085, 0x3087, 0x30E3, 0x30E5,
                                          0x30E7, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x30A1, 0x30A3, 0x30A5,
                                          0x30A7, 0x30A9, '!', '?', '.', ',' };
    u32 i;

    if ((cp < 0x2E80) || ((cp >= 0xAC00) && (cp <= 0xD7A3))) { // Latin and Hangul break at spaces only
        return 0;
    }
    for (i = 0; i < ARRAY_COUNT(kNoBreakBefore); i++) {
        if (cp == kNoBreakBefore[i]) {
            return 0;
        }
    }
    return 1;
}

static s32 Subtitles_Advance(u32 cp, s32 height) {
    int tex_width, draw_width, advance;

    if ((Port_Font_Glyph(cp, height, &tex_width, &draw_width, &advance) != NULL) ||
        (Port_Font_Glyph('?', height, &tex_width, &draw_width, &advance) != NULL)) {
        return advance;
    }
    return 0;
}

typedef struct SubtitleLine {
    const char* start;
    const char* end;
} SubtitleLine;

// Splits `text` into lines no wider than `max_width` at glyph height `height`: at spaces, between CJK characters,
// at '\n', and mid-word only when a word alone is too wide. Returns the line count.
static s32 Subtitles_Wrap(const char* text, s32 height, s32 max_width, SubtitleLine* lines, s32 max_lines) {
    s32 count = 0;
    const char* p = text;

    while ((*p != '\0') && (count < max_lines)) {
        const char* start;
        const char* q;
        const char* brk_end = NULL;
        const char* brk_next = NULL;
        const char* end = NULL;
        const char* next = NULL;
        s32 width = 0;

        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = q = p;
        while (*q != '\0') {
            const char* before = q;
            u32 cp = Port_Utf8Next(&q);
            s32 w;

            if (cp == '\n') {
                end = before;
                next = q;
                break;
            }
            if (cp == ' ') {
                brk_end = before;
                brk_next = q;
            } else if ((before != start) && Subtitles_CanBreakBefore(cp)) {
                brk_end = before;
                brk_next = before;
            }
            w = Subtitles_Advance(cp, height);
            if ((width + w > max_width) && (cp != ' ')) {
                if (brk_end != NULL) {
                    end = brk_end;
                    next = brk_next;
                } else if (before != start) {
                    end = before;
                    next = before;
                }
                if (end != NULL) {
                    break;
                }
            }
            width += w;
        }
        if (end == NULL) {
            end = next = q;
        }
        while ((end > start) && (end[-1] == ' ')) {
            end--;
        }
        lines[count].start = start;
        lines[count].end = end;
        count++;
        p = next;
    }
    return count;
}

void Subtitles_Draw(void) {
    s32 scale = StageContext_IsHighResolution() ? 2 : 1; // 640x480 or 320x240 stage
    SubtitleLine lines[SUBTITLE_MAX_LINES];
    char key[32];
    char buf[0x200];
    const char* text;
    u8 saved_set;
    s8 saved_spacing;
    u8 saved_line_height;
    u8 saved_flags;
    s32 saved_tex;
    s32 height = SUBTITLE_GLYPH_HEIGHT * scale;
    s32 line_height = height + scale;
    s32 count;
    s32 box_h;
    s32 y;
    s32 i;

    if (sTestId == -2) {
        // PORT_SUBTITLE_TEST=<hex id> (a debug aid, like PORT_LANGUAGE): keeps announcer line <id> on screen, so the
        // box can be checked without the sound driver (PORT_NO_AUDIO)
        const char* env = getenv("PORT_SUBTITLE_TEST");
        sTestId = (env != NULL) ? (s32) strtol(env, NULL, 16) : -1;
    }
    if (sTestId >= 0) {
        sShownId = sTestId;
        sHold = SUBTITLE_HOLD_FRAMES;
    } else if (sNewSerial != sSeenSerial) {
        sSeenSerial = sNewSerial;
        sShownId = sNewId;
        sShownHandle = sNewHandle;
        sHold = SUBTITLE_HOLD_FRAMES;
    }
    if (sShownId < 0) {
        return;
    }
    if (Audio_QueueSetMonoOutputCommand(sShownHandle) == 0) {
        if (sHold == 0) {
            sShownId = -1;
            return;
        }
        sHold--;
    }
    if ((font_context == NULL) || !Port_Subtitles_Enabled()) {
        return;
    }
    snprintf(key, sizeof(key), "SUBTITLE_ANNOUNCER_%02X", sShownId);
    text = Port_Text_LookupId(key); // NULL in the original language and for lines left unsubtitled (badge call-outs, 0x47)
    if ((text == NULL) || (*text == '\0')) {
        return;
    }

    count = Subtitles_Wrap(text, height, (SUBTITLE_BOX_W - 2 * SUBTITLE_PAD) * scale, lines, SUBTITLE_MAX_LINES);
    box_h = (count * line_height) + ((SUBTITLE_PAD + 2) * scale); // room under the last line's descenders
    y = (SUBTITLE_BOX_BOTTOM * scale) - box_h;

    // The stage's text state is put back afterwards: screens set their font up once and print with it every frame.
    saved_set = font_context->unk_50;
    saved_spacing = font_context->unk_51;
    saved_line_height = font_context->line_height;
    saved_flags = font_context->line_break_flag;
    saved_tex = font_context->unk_4C;

    Ui_DrawPrimColorRect(SUBTITLE_BOX_X * scale, y, SUBTITLE_BOX_W * scale, box_h, 0, 0, 0, 0xA0);
    Font_BeginTranslucentTextRendering();
    font_context->unk_51 = 0; // no extra letter spacing; the active set only matters for its fallback advance
    Port_Font_SetHeight(height);
    Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, 0xFF);
    for (i = 0; i < count; i++) {
        s32 len = lines[i].end - lines[i].start;

        if (len >= (s32) sizeof(buf)) {
            len = sizeof(buf) - 1;
        }
        memcpy(buf, lines[i].start, len);
        buf[len] = '\0';
        Port_Font_PrintUtf8((SUBTITLE_BOX_X + SUBTITLE_PAD) * scale, y + (SUBTITLE_PAD / 2) * scale + i * line_height,
                            buf);
    }
    Port_Font_SetHeight(0);
    Font_EndTexturedTextRendering();
    font_context->unk_50 = saved_set;
    font_context->unk_51 = saved_spacing;
    font_context->line_height = saved_line_height;
    font_context->line_break_flag = saved_flags;
    font_context->unk_4C = saved_tex;
}
