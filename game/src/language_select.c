/* PORT: not part of the decomp. The language select screen, shown before the N64 logo (State_Boot). */
#include "global.h"
#include "controller.h"
#include "font.h"
#include "gfx_buffer.h"
#include "memory_main.h"
#include "stage_loader.h"
#include "ui_graphics.h"
#include "text_lookup.h"
#include "unicode_font.h"
#include <stdlib.h>
#include <string.h>

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
u8* Asset_LoadToSegment(s32 id, u8* rom_start, u8* rom_end, s32 arg3);
void Audio_PlaySystemSound(s32 id);
extern u8 common_menu1_ui_ROM_START[];
extern u8 common_menu1_ui_ROM_END[];

// At first only the original にほんご (kana) is offered; C-left adds the languages whose text comes from the AI
// translation table (translations/ai.csv), human translations taking over row by row as they land (human.csv).
// The screen is shown on the first launch only: once a language is chosen (port setting gPort.Language) later
// launches use it, and Options (C-Right on the Game Pak check screen) changes it.
static s32 sLanguageCursor;
static s32 sLanguageUnlocked;

// The windows open as the game's open: each expands from its middle, then fills with its contents; it closes the
// other way round (hippodrome-utils AGENTS.md, design principle 2). First the list, then a note that the language
// can be changed in Options, in the chosen language.
enum LanguageSelectPhase {
    LANGSEL_LIST_OPEN,   // the list expands and fills, then takes input
    LANGSEL_LIST_CLOSE,  // a language was chosen: the list empties and contracts
    LANGSEL_NOTE_OPEN,   // the note expands and fills, then waits for A
    LANGSEL_NOTE_CLOSE,  // the note empties and contracts
    LANGSEL_DONE
};
static s32 sLanguagePhase;
static s32 sLanguageAnim; // 0 closed .. LANGSEL_EXPAND + LANGSEL_FILL fully open

#define LANGSEL_EXPAND 8
#define LANGSEL_FILL 6
#define LANGSEL_OPEN (LANGSEL_EXPAND + LANGSEL_FILL)

#define LANGSEL_ITEM_STEP 34
#define LANGSEL_NOTE_LINE 28
#define LANGSEL_NOTE_WIDTH 472 // the note's widest line; its window is 64 px wider than its text
#define LANGSEL_BUTTON_UP 0x0800
#define LANGSEL_BUTTON_DOWN 0x0400
#define LANGSEL_BUTTON_CLEFT 0x0002
#define LANGSEL_BUTTON_A 0x8000
#define LANGSEL_BUTTON_B 0x4000
#define LANGSEL_BUTTON_START 0x1000

static const char sLanguagePrompt[] = "Press C◀ for AI translations";
static const char sLanguageAiNote[] = "AI translation";
// The note in the original kana (the tables are not used for it); PORT_LANGUAGE_OPTIONS_NOTE in the others.
static const char sLanguageNoteKana[] = "ことばは オプションで いつでも かえられます";
static const char sLanguageNoteFallback[] = "You can change the language anytime in Options.";

static s32 LanguageSelect_ItemCount(void) {
    return sLanguageUnlocked ? PORT_LANG_COUNT : 1;
}

// Buttons pressed this frame on any controller, with the stick counting as the D-pad when it crosses half way.
static u16 LanguageSelect_ReadButtons(void) {
    static s32 sStickHeld;
    u16 pressed = 0;
    s32 stick = 0;
    s32 i;

    Cont_StartReadInputs(); // screens read the pads themselves, once a frame (as the title screen does)
    Cont_ReadInputs();
    for (i = 0; i < 4; i++) {
        pressed |= gControllers[i].buttonPressed;
        if (gControllers[i].rawStickY > 40) {
            stick = LANGSEL_BUTTON_UP;
        } else if (gControllers[i].rawStickY < -40) {
            stick = LANGSEL_BUTTON_DOWN;
        }
    }
    if (stick != sStickHeld) {
        pressed |= stick;
    }
    sStickHeld = stick;
    return pressed;
}

static void LanguageSelect_Update(void) {
    u16 pressed = LanguageSelect_ReadButtons();
    s32 count = LanguageSelect_ItemCount();

    switch (sLanguagePhase) {
        case LANGSEL_LIST_OPEN:
        case LANGSEL_NOTE_OPEN:
            if (sLanguageAnim < LANGSEL_OPEN) {
                sLanguageAnim++;
                return; // no input until the window is open
            }
            break;
        default:
            if (--sLanguageAnim <= 0) {
                sLanguageAnim = 0;
                sLanguagePhase = (sLanguagePhase == LANGSEL_LIST_CLOSE) ? LANGSEL_NOTE_OPEN : LANGSEL_DONE;
            }
            return;
    }
    if (sLanguagePhase == LANGSEL_NOTE_OPEN) {
        if (pressed & (LANGSEL_BUTTON_A | LANGSEL_BUTTON_B | LANGSEL_BUTTON_START)) {
            Audio_PlaySystemSound(SFX_MENU_CONFIRM);
            sLanguagePhase = LANGSEL_NOTE_CLOSE;
        }
        return;
    }
    if ((pressed & LANGSEL_BUTTON_CLEFT) && !sLanguageUnlocked) {
        sLanguageUnlocked = 1;
        Audio_PlaySystemSound(SFX_MENU_CONFIRM);
    } else if (pressed & (LANGSEL_BUTTON_A | LANGSEL_BUTTON_START)) {
        Audio_PlaySystemSound(SFX_MENU_CONFIRM);
        // the note that follows is in the chosen language
        Port_Text_SetLanguage(sLanguageCursor);
        sLanguagePhase = LANGSEL_LIST_CLOSE;
    } else if (pressed & LANGSEL_BUTTON_UP) {
        Audio_PlaySystemSound(SFX_MENU_SCROLL);
        sLanguageCursor = (sLanguageCursor + count - 1) % count;
    } else if (pressed & LANGSEL_BUTTON_DOWN) {
        Audio_PlaySystemSound(SFX_MENU_SCROLL);
        sLanguageCursor = (sLanguageCursor + 1) % count;
    }
}

static void LanguageSelect_PrintCentered(s32 y, const char* utf8) {
    Port_Font_PrintUtf8(320 - Port_Font_MeasureUtf8(utf8) / 2, y, utf8);
}

// Draws the window (x, y, w, h when fully open) at the current step of its animation and returns its contents'
// opacity (0 while it expands or contracts).
static u8 LanguageSelect_DrawWindow(s32 x, s32 y, s32 w, s32 h) {
    Color_RGB8 top = { 0x30, 0x48, 0xA0 };
    Color_RGB8 bottom = { 0x10, 0x18, 0x50 };
    s32 expand = (sLanguageAnim >= LANGSEL_EXPAND) ? LANGSEL_EXPAND : sLanguageAnim;
    s32 cw = (w * expand) / LANGSEL_EXPAND;
    s32 ch = (h * expand) / LANGSEL_EXPAND;

    if (cw >= 2 && ch >= 2) {
        Ui_DrawGradientPanel(x + (w - cw) / 2, y + (h - ch) / 2, cw, ch, &top, &bottom);
    }
    if (sLanguageAnim <= LANGSEL_EXPAND) {
        return 0;
    }
    return (u8)((0xFF * (sLanguageAnim - LANGSEL_EXPAND)) / LANGSEL_FILL);
}

static void LanguageSelect_DrawList(void) {
    s32 count = LanguageSelect_ItemCount();
    s32 list_height = count * LANGSEL_ITEM_STEP;
    s32 list_y = 240 - (list_height + 40) / 2;
    u8 alpha = LanguageSelect_DrawWindow(144, list_y - 24, 352, list_height + 88);
    s32 i;

    if (alpha == 0) {
        return;
    }
    if (alpha == 0xFF) {
        Ui_DrawTextureMarker(184, list_y + sLanguageCursor * LANGSEL_ITEM_STEP + 4); // (textures have no fade)
    }
    Font_BeginTranslucentTextRendering();
    Font_SetActive(0x10, 0);
    for (i = 0; i < count; i++) {
        if (i == sLanguageCursor) {
            Gfx_SetEnvColor(0xFF, 0xFF, 0x00, alpha); // #FFFF00, as the game's own highlighted text
        } else {
            Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, alpha);
        }
        LanguageSelect_PrintCentered(list_y + i * LANGSEL_ITEM_STEP, Port_Text_LanguageName(i));
    }
    Font_SetActive(0x8, 0);
    Gfx_SetEnvColor(0xB0, 0xC8, 0xFF, alpha);
    if (!sLanguageUnlocked) {
        LanguageSelect_PrintCentered(list_y + list_height + 14, sLanguagePrompt);
    } else if (sLanguageCursor != PORT_LANG_ORIGINAL) {
        LanguageSelect_PrintCentered(list_y + list_height + 14, sLanguageAiNote);
    }
    Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, 0xFF);
    Font_EndTexturedTextRendering();
}

// Splits `text` into lines no wider than `width` at its spaces (a word wider than that gets a line of its own).
// Returns the line count; lines[i] are NUL-terminated copies.
#define LANGSEL_NOTE_MAX_LINES 4
static s32 LanguageSelect_WrapAt(const char* text, s32 width, char lines[][160]) {
    s32 count = 0;
    const char* p = text;

    while (*p != '\0' && count < LANGSEL_NOTE_MAX_LINES) {
        const char* end = p;
        const char* fit = NULL;
        char trial[160];

        while (*p == ' ') {
            p++;
        }
        end = p;
        for (;;) {
            const char* next = end;
            s32 len;
            while (*next == ' ') {
                next++;
            }
            while (*next != '\0' && *next != ' ') {
                next++;
            }
            len = (s32)(next - p);
            if (len >= (s32)sizeof(trial)) {
                break;
            }
            memcpy(trial, p, len);
            trial[len] = '\0';
            if (fit != NULL && Port_Font_MeasureUtf8(trial) > width) {
                break;
            }
            fit = next;
            end = next;
            if (*next == '\0') {
                break;
            }
        }
        if (fit == NULL) {
            break;
        }
        memcpy(lines[count], p, fit - p);
        lines[count][fit - p] = '\0';
        count++;
        p = fit;
    }
    return count;
}

// Wraps the note into as few lines as LANGSEL_NOTE_WIDTH allows, with the lines as even as they can be (no lone
// last word).
static s32 LanguageSelect_WrapNote(const char* text, char lines[][160]) {
    s32 count = LanguageSelect_WrapAt(text, LANGSEL_NOTE_WIDTH, lines);
    s32 width;

    if (count <= 1) {
        return count;
    }
    for (width = Port_Font_MeasureUtf8(text) / count; width < LANGSEL_NOTE_WIDTH; width += 8) {
        if (LanguageSelect_WrapAt(text, width, lines) == count) {
            return count;
        }
    }
    return LanguageSelect_WrapAt(text, LANGSEL_NOTE_WIDTH, lines);
}

static void LanguageSelect_DrawNote(void) {
    const char* text = (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL) ? sLanguageNoteKana
                                                                       : Port_Text_LookupId("PORT_LANGUAGE_OPTIONS_NOTE");
    char lines[LANGSEL_NOTE_MAX_LINES][160];
    s32 count, width, height, y, i;
    u8 alpha;

    if (text == NULL) {
        text = sLanguageNoteFallback;
    }
    Font_BeginTranslucentTextRendering();
    Font_SetActive(0x10, 0);
    count = LanguageSelect_WrapNote(text, lines); // measured in the list's font set
    width = 0;
    for (i = 0; i < count; i++) {
        s32 w = Port_Font_MeasureUtf8(lines[i]);
        width = (w > width) ? w : width;
    }
    Font_EndTexturedTextRendering();
    width = ((width < 272) ? 272 : width) + 64; // the window: the text with a margin, at least the list's width
    height = count * LANGSEL_NOTE_LINE;
    y = 240 - (height + 48) / 2;
    alpha = LanguageSelect_DrawWindow(320 - width / 2, y, width, height + 48);
    if (alpha == 0) {
        return;
    }
    Font_BeginTranslucentTextRendering();
    Font_SetActive(0x10, 0);
    Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, alpha);
    for (i = 0; i < count; i++) {
        LanguageSelect_PrintCentered(y + 20 + i * LANGSEL_NOTE_LINE, lines[i]);
    }
    Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, 0xFF);
    Font_EndTexturedTextRendering();
}

static s32 LanguageSelect_DrawFrame(u8 fade_mode) {
    if (fade_mode == 0) { // the screen's fade in has ended (and the fade out has not begun)
        LanguageSelect_Update();
    }
    BgStage_DrawFrame();
    GfxImage_FillCurrent(&gDisplayListHead, 1);
    if (sLanguagePhase <= LANGSEL_LIST_CLOSE) {
        LanguageSelect_DrawList();
    } else if (sLanguagePhase <= LANGSEL_NOTE_CLOSE) {
        LanguageSelect_DrawNote();
    }
    return sLanguagePhase == LANGSEL_DONE;
}

/**
 * On the first launch (no gPort.Language setting yet) runs the language select screen until a language is chosen,
 * then the note that Options can change it, and sets the language (Port_Text_SetLanguage) and saves it. Later
 * launches set the saved language without showing anything. PORT_LANGUAGE=<column> skips the screen (and saves
 * nothing); PORT_LANGUAGE_SELECT=1 shows it even with a saved language (a debug aid).
 */
void Port_RunLanguageSelect(void) {
    StageContext* stage_context;

    if (Port_Text_LanguageForced()) {
        return;
    }
    if (Port_Text_LanguageChosen() && getenv("PORT_LANGUAGE_SELECT") == NULL) {
        Port_Text_SetLanguage(Port_Text_SavedLanguage());
        return;
    }
    sLanguageCursor = Port_Text_SavedLanguage();
    sLanguageUnlocked = sLanguageCursor != PORT_LANG_ORIGINAL;
    sLanguagePhase = LANGSEL_LIST_OPEN;
    sLanguageAnim = 0;

    MainPool_PushState('LANG');
    Font_Init(0x18, 0);
    Gfx_InitDisplayListBuffers(0x10000, 0);
    stage_context = StageContext_Allocate(1, 1, 2, 0, 2, 1);
    Asset_LoadToSegment(1, common_menu1_ui_ROM_START, common_menu1_ui_ROM_END, 0);
    StageContext_Activate(stage_context);
    StageFade_Start(1);
    BgStage_RunUntilCondition(0, LanguageSelect_DrawFrame);
    StageFade_Start(0x10);
    BgStage_RunUntilCondition(0x10, LanguageSelect_DrawFrame);
    StageContext_Deactivate();
    Gfx_FreeDisplayListBuffers();
    MainPool_PopState('LANG'); // frees the font too

    Port_Text_SetLanguage(sLanguageCursor);
    Port_Text_SaveLanguage(sLanguageCursor);
}
