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

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
u8* Asset_LoadToSegment(s32 id, u8* rom_start, u8* rom_end, s32 arg3);
void Audio_PlaySystemSound(s32 id);
extern u8 common_menu1_ui_ROM_START[];
extern u8 common_menu1_ui_ROM_END[];

// At first only the original にほんご (kana) is offered; C-left adds the languages whose text comes from the AI
// translation table (translations/ai.csv), human translations taking over row by row as they land (human.csv).
static s32 sLanguageCursor;
static s32 sLanguageUnlocked;
static s32 sLanguageDone;

#define LANGSEL_ITEM_STEP 34
#define LANGSEL_BUTTON_UP 0x0800
#define LANGSEL_BUTTON_DOWN 0x0400
#define LANGSEL_BUTTON_CLEFT 0x0002
#define LANGSEL_BUTTON_A 0x8000
#define LANGSEL_BUTTON_START 0x1000

static const char sLanguagePrompt[] = "Press C◀ for AI translations";
static const char sLanguageAiNote[] = "AI translation";

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

    if ((pressed & LANGSEL_BUTTON_CLEFT) && !sLanguageUnlocked) {
        sLanguageUnlocked = 1;
        Audio_PlaySystemSound(SFX_MENU_CONFIRM);
    } else if (pressed & (LANGSEL_BUTTON_A | LANGSEL_BUTTON_START)) {
        Audio_PlaySystemSound(SFX_MENU_CONFIRM);
        sLanguageDone = 1;
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

static s32 LanguageSelect_DrawFrame(UNUSED u8 fade_mode) {
    Color_RGB8 top = { 0x30, 0x48, 0xA0 };
    Color_RGB8 bottom = { 0x10, 0x18, 0x50 };
    s32 count = LanguageSelect_ItemCount();
    s32 list_height = count * LANGSEL_ITEM_STEP;
    s32 list_y = 240 - (list_height + 40) / 2;
    s32 i;

    if (!sLanguageDone) {
        LanguageSelect_Update();
    }
    BgStage_DrawFrame();
    GfxImage_FillCurrent(&gDisplayListHead, 1);
    Ui_DrawGradientPanel(144, list_y - 24, 352, list_height + 88, &top, &bottom);
    Ui_DrawTextureMarker(184, list_y + sLanguageCursor * LANGSEL_ITEM_STEP + 4);
    Font_BeginTranslucentTextRendering();
    Font_SetActive(0x10, 0);
    for (i = 0; i < count; i++) {
        if (i == sLanguageCursor) {
            Gfx_SetEnvColor(0xFF, 0xFF, 0x00, 0xFF); // #FFFF00, as the game's own highlighted text
        } else {
            Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, 0xFF);
        }
        LanguageSelect_PrintCentered(list_y + i * LANGSEL_ITEM_STEP, Port_Text_LanguageName(i));
    }
    Font_SetActive(0x8, 0);
    Gfx_SetEnvColor(0xB0, 0xC8, 0xFF, 0xFF);
    if (!sLanguageUnlocked) {
        LanguageSelect_PrintCentered(list_y + list_height + 14, sLanguagePrompt);
    } else if (sLanguageCursor != PORT_LANG_ORIGINAL) {
        LanguageSelect_PrintCentered(list_y + list_height + 14, sLanguageAiNote);
    }
    Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, 0xFF);
    Font_EndTexturedTextRendering();
    return sLanguageDone;
}

/**
 * Runs the language select screen until a language is chosen, then sets it (Port_Text_SetLanguage) and saves it
 * for the next boot. The cursor starts on the last choice, with the AI languages already shown if it was one of
 * them. PORT_LANGUAGE=<column> skips the screen.
 */
void Port_RunLanguageSelect(void) {
    StageContext* stage_context;
    s32 saved;

    if (Port_Text_LanguageForced()) {
        return;
    }
    saved = Port_Text_SavedLanguage();
    sLanguageCursor = saved;
    sLanguageUnlocked = saved != PORT_LANG_ORIGINAL;
    sLanguageDone = 0;

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
