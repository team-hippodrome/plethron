/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "cartridge_check.h"
#include "asset_archive.h"
#include "audio_commands.h"
#include "audio_music.h"
#include "font.h"
#include "fragments/cartridge_check_models/cartridge_check_models.h"
#include "fragments/doduo_game_boy_model/doduo_game_boy_model.h"
#include "fragments/dodrio_game_boy_model/dodrio_game_boy_model.h"
#include "fragments/main_menu/main_menu.h"
#include "geo_node.h"
#include "graphics_textures.h"
#include "input.h"
#include "main_pool.h"
#include "matrix.h"
#include "memory_main.h"
#include "model_renderer.h"
#include "pokemon_stats.h"
#include "save_data.h"
#include "stage_loader.h"
#include "ui_graphics.h"
#include "text_lookup.h" // PORT: the Subtitles row
#include "unicode_font.h"

#include <stdio.h>

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void Announcer_SetEnabled(s32 enabled);
void Audio_SetOutputMode(s32 mode);
GraphNode* GeoLayout_Process(MemoryBlock* pool, void* segptr);
uintptr_t Util_ConvertAddrToVirtAddr(uintptr_t addr);

// Declared here instead of in a header: the callers of these disagree on their types.
M2C_UNK StageFade_Start(M2C_UNK);
void func_80006998(u16 color); // PORT: matches StageContext_SetClearColor
void func_800199E4(DisplayObject* arg0); // PORT: matches ModelRenderer_ClearDisplayObject
s32 func_80006828(u32 timer, s32 (*f)(u8)); // BgStage_RunUntilCondition
void func_8001A384(s32 mask, s32 arg1); // PORT: matches Font_SetActive
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
void func_8001AB6C(void); // PORT: matches Font_BeginTranslucentTextRendering
void func_8001ABBC(void); // PORT: matches Font_EndTexturedTextRendering
s32 Save_GetGbPlayerSettings(s8 *arg0, s32 *arg1); // PORT: matches the definition
void* func_800023C4(u32 size, s32 side); // PORT: matches MainPool_Alloc
GfxImage* func_800051F0(s32 fmt, s32 size, s32 width, s32 height, s32 side); // PORT: matches GfxImage_Allocate
void MainPool_FinalizeAllocation(struct MemoryBlock* block);
void Audio_StopMusic(s32);
void Audio_StopSoundByCategory(s32, s32);

// PORT: typed here, not in game_state.h, while vs_scene.c still declares an s8* version
s32 BezierPath_Step(Obj80027168* path, Vec3f* out);
extern Key80027168 D_88402E40[]; // PORT: Bezier path keys (was M2C_UNK)
extern Key80027168 D_88402E5C[]; // PORT: Bezier path keys (was M2C_UNK)
extern Key80027168 D_88402D98[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
extern Key80027168 D_88402DB4[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
extern Key80027168 D_88402E78[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
extern Key80027168 D_88402E94[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s

/**
 * Opens the message window: grows it over 8 frames from 32x20 to 192x48 around (160, 60).
 * Seen in play (2026-11-14): runs before 「ドードーのゲームボーイ プレゼント！！」 appears.
 */
void CartridgeCheck_OpenMessageWindow(void) {
    s16 var_s0;
    s16 var_s1;
    s32 var_s2;

    var_s0 = 0x20;
    var_s1 = 20;
    var_s2 = 0;
    do {
        CartridgeCheck_DrawFrame(0);
        CartridgeCheck_DrawMessageWindow((s16) (160 - (var_s0 / 2)), (s16) (60 - (var_s1 / 2)), var_s0, var_s1);
        BgStage_AdvanceFrame();
        var_s2 += 1;
        var_s0 += 20;
        var_s1 += 4;
    } while (var_s2 != 8);
}

/**
 * Closes the message window: shrinks it over 8 frames, the reverse of
 * CartridgeCheck_OpenMessageWindow. Seen in play (2026-11-14) after the last gift message.
 */
void CartridgeCheck_CloseMessageWindow(void) {
    s16 var_s0;
    s16 var_s1;
    s32 var_s2;

    var_s0 = 196;
    var_s1 = 52;
    var_s2 = 0;
    do {
        CartridgeCheck_DrawFrame(0);
        CartridgeCheck_DrawMessageWindow((s16) (160 - (var_s0 / 2)), (s16) (60 - (var_s1 / 2)), var_s0, var_s1);
        BgStage_AdvanceFrame();
        var_s2 += 1;
        var_s0 -= 20;
        var_s1 -= 4;
    } while (var_s2 != 8);
}

/**
 * Draws the blinking ▼ "next" arrow at (x, y): fades in, holds, fades out over a 61-frame
 * cycle (D_88403364). Seen in play (2026-11-14) in the corner of the gift messages.
 */
void CartridgeCheck_DrawNextArrow(s16 x, s16 y) {
    s32 var_a3;

    if (D_88403364 < 0x10) {
        var_a3 = D_88403364 * 0x10;
    } else if (D_88403364 < 35) {
        var_a3 = 0xFF;
    } else if (D_88403364 < 50) {
        var_a3 = (-D_88403364 * 0x10) + 0x320;
    } else {
        var_a3 = 0;
    }

    D_88403364++;
    if (D_88403364 >= 61) {
        D_88403364 = 0;
    }

    Gfx_SetEnvColor(0xFF, 0xFF, 0xFF, var_a3); // #FFFFFF
    Font_DrawCharAt(x, y, 0x2177);
}

/**
 * Shows `text` in the message window with the blinking arrow until A is pressed, then plays
 * the cursor sound and remembers the text for CartridgeCheck_ScrollToNextMessage.
 * Seen in play (2026-11-14): each gift message stayed up until A.
 */
void CartridgeCheck_ShowMessageUntilA(s8* text) { // PORT: text is a pointer (was s32)
    D_88403364 = 0x10;
    do {
        CartridgeCheck_DrawFrame(0);
        CartridgeCheck_DrawMessageWindow(30, 0x20, 260, 52);
        func_8001AB6C();
        func_8001A384(4, -1);
        Font_SetLineHeight(20);
        Font_Printf(46, 40, text);
        CartridgeCheck_DrawNextArrow(260, 60);
        func_8001ABBC();
        BgStage_AdvanceFrame();
    } while (!(gPlayer1Controller->buttonPressed & 0x8000));
    Audio_PlaySystemSound(1);
    D_88402D70 = text;
}

/**
 * Scrolls the previous message (D_88402D70) up out of the window and `text` in over 8 frames,
 * then makes `text` the current message.
 * Seen in play (2026-11-14): A on 「ドードーのゲームボーイ プレゼント！！」 scrolled to
 * 「GBモードに スピードアップの きのうが ついたぞ！」.
 */
void CartridgeCheck_ScrollToNextMessage(s8* text) {
    s32 i;
    s32 x1 = 30;
    s32 x2 = 290;
    s32 y1 = 40;
    s32 y2 = 72;

    for (i = 1; i < 9; i++) {
        CartridgeCheck_DrawFrame(0);
        CartridgeCheck_DrawMessageWindow(30, 0x20, 260, 52);

        gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, x1, y1, x2, y2);

        Font_BeginTranslucentTextRendering();
        Font_SetActive(4, -1);
        Font_SetLineHeight(20);
        Font_Printf(46, y1 - i * 4, D_88402D70);
        Font_Printf(46, 80 - i * 4, text);
        Font_EndTexturedTextRendering();
        BgStage_AdvanceFrame();
    }

    D_88402D70 = text;
}

/**
 * The Game Boy present event after a cup win: a special Game Boy model drops in and the
 * window says what it unlocks. transitionFlags 8: 「ドードーのゲームボーイ プレゼント！！」 /
 * 「GBモードに スピードアップの きのうが ついたぞ！」 (Doduo Game Boy: GB mode gets speed-up);
 * flag 0x10: 「ドードリオのゲームボーイ プレゼント！！」 / 「スピードアップの きのうが
 * きょうりょくに なったぞ！」 (Dodrio Game Boy: the speed-up gets stronger). VsScene sets the
 * flag on the first and second cup clear (Tournament_UnlockGbPlayerFrame).
 * Seen in play (2026-11-14) with transitionFlags poked to 0x0A at CartridgeCheck_Main entry:
 * the Doduo Game Boy dropped in and both Doduo messages showed. The Dodrio text is from the ROM strings.
 */
void CartridgeCheck_RunGameBoyGiftEvent(void) {
    Audio_PlayMusicIfChanged(0xC);
    D_884034E0.node.flags = (u8) (D_884034E0.node.flags | 1);
    func_80006828(115, &CartridgeCheck_DrawFrame);
    D_884034E0.node.renderFlags = (u8) (D_884034E0.node.renderFlags & 0xFFDF);
    CartridgeCheck_OpenMessageWindow();
    Audio_PlayCommand(0x51, 0, 0);
    if (gSessionContext.transitionFlags & 8) {
        CartridgeCheck_ShowMessageUntilA(D_88402D88);
        CartridgeCheck_ScrollToNextMessage(D_88402D8C);
        CartridgeCheck_ShowMessageUntilA(D_88402D8C);
    } else {
        CartridgeCheck_ShowMessageUntilA(D_88402D90);
        CartridgeCheck_ScrollToNextMessage(D_88402D94);
        CartridgeCheck_ShowMessageUntilA(D_88402D94);
    }
    CartridgeCheck_CloseMessageWindow();
    D_884034E0.node.renderFlags = (u8) (D_884034E0.node.renderFlags | 0x20);
    func_80006828(45, &CartridgeCheck_DrawFrame);
}

extern Key80027168 D_88402D98[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
extern Key80027168 D_88402DB4[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
M2C_UNK Audio_PlaySystemSound(M2C_UNK);
/**
 * Camera move that opens the scene after the title screen: follows the eye/target paths
 * D_88402DB4/D_88402D98 until they end, and on frame 42 shows the console model and plays sound 0x10.
 * Seen in play (2026-11-14): ran between Start on the title and the cartridge check screen.
 */
void CartridgeCheck_PlayBootIntro(void) {
    Obj80027168 sp8C; // PORT: was s8[0x3C]; the host struct is larger
    Obj80027168 sp50; // PORT: was s8[0x3C]; the host struct is larger
    s32 i = 0;
    CameraNode* rec; // PORT: the scene camera (was s32)
    s32 pad; // FAKE: stack padding

    rec = D_88403370->children;
    BezierPath_InitEased(&sp8C, D_88402DB4);
    BezierPath_InitEased(&sp50, D_88402D98);
    if (BezierPath_Step(&sp8C, &rec->view.eyePosition) != 0) {
        do {
            if (i++ == 42) {
                D_88403648.node.callbackArg = 1;
                Audio_PlaySystemSound(0x10);
                D_88403648.node.renderFlags |= 0x20;
            }
            BezierPath_Step(&sp50, &rec->view.targetPosition);
            CartridgeCheck_DrawFrame(0);
            BgStage_AdvanceFrame();
        } while (BezierPath_Step(&sp8C, &rec->view.eyePosition) != 0);
    }
    func_80006828(25, &CartridgeCheck_DrawFrame);
}

extern Key80027168 D_88402DD0[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
extern Key80027168 D_88402DEC[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s

M2C_UNK Audio_PlaySystemSound(M2C_UNK);
/**
 * Opening of the scene on the CartridgeCheck_EnterAfterCupWin path: runs the Game Boy present
 * event when transitionFlags has 8 or 0x10, then the camera move along D_88402DEC/D_88402DD0
 * (console shown on frame 10).
 * Seen in play (2026-11-14) with transitionFlags poked to 0x0A: the Doduo Game Boy event ran,
 * then the camera moved to the console.
 * @unverified "CupWin": transitionFlags 2 comes from VsScene's win handler (Tournament_RecordCupClear); a
 * real cup win was not played.
 */
void CartridgeCheck_PlayCupWinIntro(void) {
    Obj80027168 sp8C; // PORT: was s8[0x3C]; the host struct is larger
    Obj80027168 sp50; // PORT: was s8[0x3C]; the host struct is larger
    s32 i = 0;
    CameraNode* rec; // PORT: the scene camera (was s32)
    s32 pad; // FAKE: stack padding

    rec = D_88403370->children;
    if (gSessionContext.transitionFlags & 0x18) {
        CartridgeCheck_RunGameBoyGiftEvent();
    }
    BezierPath_InitEased(&sp8C, D_88402DEC);
    BezierPath_InitEased(&sp50, D_88402DD0);
    if (BezierPath_Step(&sp8C, &rec->view.eyePosition) != 0) {
        do {
            if (i++ == 10) {
                D_88403648.node.callbackArg = 1;
                D_88403648.node.renderFlags |= 0x20;
                Audio_PlaySystemSound(0x10);
            }
            BezierPath_Step(&sp50, &rec->view.targetPosition);
            CartridgeCheck_DrawFrame(0);
            BgStage_AdvanceFrame();
        } while (BezierPath_Step(&sp8C, &rec->view.eyePosition) != 0);
    }
    func_80006828(25, &CartridgeCheck_DrawFrame);
}

extern Color_RGB8 D_88402F94;
extern Color_RGB8 D_88402F98;
extern char* D_88402D80;
extern char* D_88402D84;
extern char D_88403228[];
extern u8 D_8F4B2B30[];
extern Gfx D_80060398[4]; // PORT: sized (open, the data after its G_ENDDL was lifted as commands)
s32 CartridgeCheck_MoveCursor(s32);
void CartridgeCheck_DrawPortPanel(s32, s32, s32);

/**
 * Draws the cartridge check screen. With a cartridge cursor it moves it, then shows
 * 「ゲームボーイのカートリッジをえらんでください。」 and the four port panels
 * (CartridgeCheck_DrawPortPanel); without one, 「ポケモンの カートリッジが セット されていません。…」.
 * Then it draws the 「オプション」 button with its C-Right icon.
 * Seen in play: runs every frame on that screen, without a Transfer Pak. The first pass had it as
 * Options_DrawTitle.
 */
void CartridgeCheck_DrawScreen(void) {
    Color_RGB8 top = D_88402F94;
    Color_RGB8 bottom = D_88402F98;

    if (D_88403358 >= 0) {
        D_88403358 = CartridgeCheck_MoveCursor(D_88403358);
        Ui_DrawGradientPanel(30, 30, 260, 24, &top, &bottom);
        Font_BeginTranslucentTextRendering();
        Font_SetActive(2, -1);
        Font_SetLineHeight(0x10);
        Font_Printf(35, 36, D_88402D80);
        Font_EndTexturedTextRendering();
        CartridgeCheck_DrawPortPanel(0, 30, 56);
        CartridgeCheck_DrawPortPanel(1, 162, 56);
        CartridgeCheck_DrawPortPanel(2, 30, 0x80);
        CartridgeCheck_DrawPortPanel(3, 162, 0x80);
    } else {
        Ui_DrawGradientPanel(30, 30, 260, 74, &top, &bottom);
        Font_BeginTranslucentTextRendering();
        Font_SetActive(2, -1);
        Font_SetLineHeight(0x10);
        Font_Printf(50, 35, D_88402D84);
        Font_EndTexturedTextRendering();
    }
    Ui_DrawGradientPanel(190, 202, 100, 24, &top, &bottom);
    gSPDisplayList(gDisplayListHead++, gUiEnvTintTextureDL);
    Gfx_DrawTextureRgba32(200, 207, 0x10, 14, (u8*)Util_ConvertAddrToVirtAddr((uintptr_t)D_8F4B2B30), 0x10, 0);
    gSPDisplayList(gDisplayListHead++, D_80060398);
    Font_BeginTranslucentTextRendering();
    Font_SetActive(2, -1);
    Font_Printf(220, 208, D_88403228);
    Font_EndTexturedTextRendering();
}

extern char D_88403234[];
extern char D_88403240[];
extern char D_8840324C[];
extern char D_88403254[];
extern char D_8840325C[];
extern char D_88403268[];
extern char D_8840327C[];
extern char D_88403290[];
extern char D_884032A0[];
extern u8 D_8F4B2EB0[];

// PORT: in the translated languages the menu has a Subtitles row after Commentary (the announcer's subtitles,
// game/src/subtitles.c). Its value is the port setting gPort.Subtitles, held here while the menu is open.
#define OPTIONS_HAS_SUBTITLES() (Port_Text_GetLanguage() != PORT_LANG_ORIGINAL)
#define OPTIONS_ROW_COUNT() (OPTIONS_HAS_SUBTITLES() ? 5 : 4)
static s32 sOptionsSubtitles;

/**
 * Draws the options: ステレオ/モノラル (Stereo/Mono), あり/なし (On/Off), バックアップ (Backup), おわり (End).
 * Seen in play: draws 「オプション」 with サウンド/じっきょう/バックアップ/おわり; the cursor moves with D-Down.
 */
void Options_DrawMenu(s32 sel_row, u8 flags) {
    Color_RGB8 sp44 = {0x00, 0x00, 0x32}; // #000032
    Color_RGB8 sp40 = {0x00, 0x00, 0xC8}; // #0000C8
    static char *D_88402FA4[] = {D_88403234, D_88403240};
    static char *D_88402FAC[] = {D_8840324C, D_88403254};
    s32 row;

    row = sel_row;
    Ui_DrawGradientPanel(50, 60, 220, OPTIONS_HAS_SUBTITLES() ? 145 : 125, &sp44, &sp40); // PORT: + Subtitles row
    gSPDisplayList(gDisplayListHead++, D_80060290);
    Gfx_DrawTextureRgba16(58, (row * 20) + 99, 24, 14, (u8 *) Util_ConvertAddrToVirtAddr((uintptr_t) D_8F4B2EB0), 24, 0x200000);
    gSPDisplayList(gDisplayListHead++, gUiRestoreTextureDL);
    Font_BeginTranslucentTextRendering();
    Font_EnableTwoCycleTexturing();
    Font_SetActive(8, -2);
    Gfx_SetPrimColor(100, 255, 150, 255); // #64FF96
    Font_Printf(110, 68, D_8840325C);
    Font_DisableTwoCycleTexturing();
    Font_SetActive(4, -2);
    Gfx_SetEnvColor(255, 255, ((row * 20) + 0x63 == 0x63) ? 0 : 255, 255); // #FFFF00 when selected, else #FFFFFF
    Font_Printf(82, 98, D_88403268, D_88402FA4[(flags & 1) != 0]);
    Gfx_SetEnvColor(255, 255, ((row * 20) + 0x63 == 0x77) ? 0 : 255, 255); // #FFFF00 when selected, else #FFFFFF
    Font_Printf(82, 118, D_8840327C, D_88402FAC[(flags & 2) != 0]);
    if (OPTIONS_HAS_SUBTITLES()) {
        // PORT: Subtitles <On/Off>, then Backup and Exit one row lower
        char line[0x100];
        const char* fmt = Port_Text_LookupId("PORT_OPTIONS_SUBTITLES");
        const char* value = Port_Text_Utf8Of(D_88402FAC[sOptionsSubtitles == 0]);

        snprintf(line, sizeof(line), (fmt != NULL) ? fmt : "Subtitles       %s", (value != NULL) ? value : "");
        Gfx_SetEnvColor(255, 255, (row == 2) ? 0 : 255, 255);
        Port_Font_PrintUtf8(82, 138, line);
        Gfx_SetEnvColor(255, 255, (row == 3) ? 0 : 255, 255);
        Font_Printf(82, 158, D_88403290);
        Gfx_SetEnvColor(255, 255, (row == 4) ? 0 : 255, 255);
        Font_Printf(82, 178, D_884032A0);
        Font_EndTexturedTextRendering();
        return;
    }
    Gfx_SetEnvColor(255, 255, ((row * 20) + 0x63 == 0x8B) ? 0 : 255, 255); // #FFFF00 when selected, else #FFFFFF
    Font_Printf(82, 138, D_88403290);
    Gfx_SetEnvColor(255, 255, ((row * 20) + 0x63 == 0x9F) ? 0 : 255, 255); // #FFFF00 when selected, else #FFFFFF
    Font_Printf(82, 158, D_884032A0);
    Font_EndTexturedTextRendering();
}

/**
 * ６４のセーブデータをクリアします よろしいですか？ / ほんとうにいいんですね？ (Clear the N64 save data, OK? / Really?) with はい/いいえ.
 * `secondAsk` picks the second question, `choice` is 0 for はい and 1 for いいえ.
 * Seen in play: draws 「64のセーブデータをクリアします よろしいですか？」 with はい / いいえ, and
 * (2026-11-14) 「ほんとうにいいんですね？」 after はい.
 */
void Options_DrawClearSavePrompt(s32 second_ask, s32 choice) {
    Color_RGB8 sp4C = { 0x00, 0x00, 0x32 }; // #000032
    Color_RGB8 sp48 = { 0x00, 0x00, 0xC8 }; // #0000C8
    UNUSED s32 pad; // FAKE: stack padding
    s16 pos[2];
    s32 temp;

    if (choice == 0) {
        temp = 40;
    } else {
        temp = 110;
    }
    pos[1] = temp + 40;
    Ui_DrawGradientPanel(40, 130, 240, 90, &sp4C, &sp48);
    gSPDisplayList(gDisplayListHead++, D_80060290);
    Gfx_DrawTextureRgba16(pos[1], 197, 24, 14, (u8*)Util_ConvertAddrToVirtAddr((uintptr_t)D_8F4B2EB0), 24, 0x200000);
    gSPDisplayList(gDisplayListHead++, gUiRestoreTextureDL);

    Font_BeginTranslucentTextRendering();
    Font_EnableTwoCycleTexturing();
    Font_SetActive(4, -2);
    Gfx_SetPrimColor(100, 255, 150, 255); // #64FF96
    Font_Printf(48, 138, D_884032A8);
    Font_DisableTwoCycleTexturing();
    Font_SetActive(8, -2);
    Gfx_SetEnvColor(255, 255, 100, 255); // #FFFF64
    if (second_ask == 0) {
        Font_Printf(90, 162, D_884032CC);
    } else {
        Font_Printf(50, 162, D_884032E0);
    }
    Font_SetActive(4, -1);
    if (choice == 0) {
        temp = 0;
    } else {
        temp = 0xFF;
    }
    Gfx_SetEnvColor(255, 255, temp, 255); // #FFFF00 when selected, else #FFFFFF
    Font_Printf(100, 196, D_884032FC);
    if (choice == 1) {
        temp = 0;
    } else {
        temp = 0xFF;
    }
    Gfx_SetEnvColor(255, 255, temp, 255); // #FFFF00 when selected, else #FFFFFF
    Font_Printf(170, 196, D_88403304);
    Font_EndTexturedTextRendering();
}

/**
 * Loop for the clear-save prompt: Left/Right toggles the choice, A confirms, B cancels (returns No).
 * Seen in play: A on バックアップ enters it (「64のセーブデータをクリアします よろしいですか？ はい いいえ」); B backs out.
 */
s32 Options_RunClearSavePrompt(s32 second_ask, s32 row, u8 flags) {
    s32 loop = 1;
    s32 sel = 1;
    s32 buttons;

    do {
        CartridgeCheck_DrawFrame(0);
        Options_DrawMenu(row, flags);
        buttons = gPlayer1Controller->buttonPressed;
        if (buttons & 0x300) {
            Audio_PlaySystemSound(1);
            sel ^= 1;
        } else if (buttons & 0x8000) {
            loop = 0;
        } else if (buttons & 0x4000) {
            sel = 1;
            loop = 0;
        }
        Options_DrawClearSavePrompt(second_ask, sel);
        BgStage_AdvanceFrame();
    } while (loop);

    loop = sel == 0;
    if (loop) {
        if (second_ask == 0) {
            Audio_PlaySystemSound(2);
        } else {
            Audio_PlaySystemSound(0x1F);
        }
    } else {
        Audio_PlaySystemSound(3);
    }
    return loop;
}

/**
 * Clears Stadium Zero's N64 save: Save_ResetAndCommitTypedRecord on every record (16 of type
 * 16, 4 each of 17 and 18, one each of 19-21), then sets stereo sound and commentary on again
 * (the values Options_RunMenu uses for ステレオ and あり).
 * Seen in play (2026-11-14): ran after はい on both 「64のセーブデータをクリアします」 questions in
 * Options > バックアップ. The save-format thread (branch claude/project-thread-qdn1uc) named it
 * from the same test.
 */
void Save_ResetAll(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        Save_ResetAndCommitTypedRecord(16, i);
    }
    for (i = 0; i < 4; i++) {
        Save_ResetAndCommitTypedRecord(17, i);
    }
    for (i = 0; i < 4; i++) {
        Save_ResetAndCommitTypedRecord(18, i);
    }
    Save_ResetAndCommitTypedRecord(19, 0);
    Save_ResetAndCommitTypedRecord(20, 0);
    Save_ResetAndCommitTypedRecord(21, 0);
    Audio_SetOutputMode(1);
    Announcer_SetEnabled(1);
}

s32 Save_GetOptionFlags(void);
void Save_SetOptionFlags(u8);
void Save_CommitTypedRecord(s32, s32);
/**
 * Loop for the options menu: Up/Down move the cursor through four entries and the chosen settings are saved.
 * Seen in play: C-Right on the cartridge check screen opens 「オプション」 (サウンド ステレオ, じっきょう あり, バックアップ,
 * おわり) and enters this.
 */
void Options_RunMenu(void) {
    s32 done = 0;
    s32 sel = 0;
    u8 flags = Save_GetOptionFlags();
    u8 diff;
    s32 rows = OPTIONS_ROW_COUNT(); // PORT: 5 with the Subtitles row
    s32 action;

    sOptionsSubtitles = Port_Subtitles_Enabled();
    do {
        if (gPlayer1Controller->buttonPressed & 0x800) {
            sel--;
            if (sel < 0) {
                sel = rows - 1;
            }
            Audio_PlaySystemSound(1);
        } else if (gPlayer1Controller->buttonPressed & 0x400) {
            sel++;
            if (sel >= rows) {
                sel = 0;
            }
            Audio_PlaySystemSound(1);
        } else if (gPlayer1Controller->buttonPressed & 0x8000) {
            // PORT: the rows after Commentary move down one when the Subtitles row (action 4) is there
            action = sel;
            if (rows == 5) {
                action = (sel == 2) ? 4 : (sel > 2) ? sel - 1 : sel;
            }
            switch (action) {
                case 4:
                    Audio_PlaySystemSound(2);
                    sOptionsSubtitles ^= 1;
                    break;
                case 0:
                    Audio_PlaySystemSound(2);
                    flags ^= 1;
                    break;
                case 1:
                    Audio_PlaySystemSound(2);
                    flags ^= 2;
                    break;
                case 2:
                    Audio_PlaySystemSound(2);
                    if (Options_RunClearSavePrompt(0, sel, flags) != 0 && Options_RunClearSavePrompt(1, sel, flags) != 0) {
                        flags = 0;
                        Save_ResetAll();
                    }
                    break;
                case 3:
                    done = 1;
                    Audio_PlaySystemSound(0x1D);
                    break;
            }
        } else if (gPlayer1Controller->buttonPressed & 0x4000) {
            done = 2;
            Audio_PlaySystemSound(3);
        }
        CartridgeCheck_DrawFrame(0);
        Options_DrawMenu(sel, flags);
        BgStage_AdvanceFrame();
    } while (done == 0);
    if ((done == 1) && (rows == 5) && (sOptionsSubtitles != Port_Subtitles_Enabled())) {
        Port_Subtitles_SetEnabled(sOptionsSubtitles); // PORT: kept by the port, not in the N64 save
    }
    if (done == 1) {
        diff = Save_GetOptionFlags() ^ flags;
        if (diff != 0) {
            if (diff & 1) {
                if (flags & 1) {
                    Audio_SetOutputMode(0);
                } else {
                    Audio_SetOutputMode(1);
                }
            }
            if (diff & 2) {
                if (flags & 2) {
                    Announcer_SetEnabled(0);
                } else {
                    Announcer_SetEnabled(1);
                }
            }
            Save_SetOptionFlags(flags);
            Save_CommitTypedRecord(0x14, 0);
        }
    }
}

/**
 * Loop of the cartridge check screen shown after the title: draws it each frame. A returns 1
 * (and makes the selected port the active one when there is a cursor), B returns 2, and
 * C-Right opens the options menu (Options_RunMenu).
 * Seen in play: with no Transfer Pak, the screen read 「ポケモンの カートリッジが セット されていません。…」 with an
 * 「オプション」 button, and C-Right opened the options menu. 2026-11-14: A went on to the main
 * menu (with and without paks) and B went back to the title screen.
 */
s32 CartridgeCheck_RunScreen(void) {
    s32 var_s0;

    var_s0 = 0;
    do {
        CartridgeCheck_DrawFrame(0);
        CartridgeCheck_DrawScreen();
        BgStage_AdvanceFrame();
        if (gPlayer1Controller->buttonPressed & 0x8000) {
            var_s0 = 1;
            Audio_PlaySystemSound(0x1B);
        } else if (gPlayer1Controller->buttonPressed & 0x4000) {
            Audio_PlaySystemSound(3);
            var_s0 = 2;
        } else if (gPlayer1Controller->buttonPressed & 1) {
            Audio_PlaySystemSound(2);
            Options_RunMenu();
        }
    } while (var_s0 == 0);
    if ((var_s0 == 1) && (D_88403358 >= 0)) {
        GbSave_SetActivePort(D_88403358);
    }
    return var_s0;
}

/**
 * Leaving the cartridge check screen with B: starts a fade (StageFade_Start), stops the music,
 * plays sound 0x23 and the camera move along D_88402E24/D_88402E08 until it ends.
 * Seen in play (2026-11-14): B on the screen ran this, then the title screen came back.
 */
void CartridgeCheck_PlayExitToTitle(void) {
    Obj80027168 sp74; // PORT: was s8[0x3C]; the host struct is larger
    Obj80027168 sp38; // PORT: was s8[0x3C]; the host struct is larger
    CameraNode* sp34; // PORT: the scene camera (was s32)

    sp34 = D_88403370->children;
    BezierPath_InitEased(&sp74, D_88402E24);
    BezierPath_InitEased(&sp38, D_88402E08);
    ModelAnim_SetSpeed(&D_88403648, 0xFFFF0000);
    ModelAnim_SetFrame(&D_88403648, 0x2E);
    D_88403648.node.renderFlags |= 0x20;
    func_80006998(0xFFFF);
    StageFade_Start(30);
    Audio_StopMusic(60);
    Audio_StopSoundByCategory(0x46, 0x3C);
    Audio_PlaySystemSound(0x23);
    while (BezierPath_Step(&sp74, &sp34->view.eyePosition)) {
        BezierPath_Step(&sp38, &sp34->view.targetPosition);
        CartridgeCheck_DrawFrame(0);
        BgStage_AdvanceFrame();
    }
}

/**
 * First part of going to the main menu: starts the console's animation and, when a port is
 * selected, creates the Game Boy model (D_88403378) for the number of Game Boy presents
 * received (plain, Doduo or Dodrio), then runs 20 frames.
 * Seen in play (2026-11-14): after A with a Green cartridge, a Poké Ball and a red Game Boy
 * dropped onto the console and the monitor rose.
 */
void CartridgeCheck_PlaySetCartridgeAnimation(void) {
    s8 sp1F;

    Save_GetGbPlayerSettings(&sp1F, 0);
    ModelAnim_BindEventTrack(&D_88403648, 1, &D_8F4889B8);
    ModelAnim_BindTransformCurve(&D_88403648, 1, &D_8F4014CC, 0x10000);
    ModelAnim_SetFrame(&D_88403648, 0x2E);
    D_88403648.node.renderFlags = (u8) (D_88403648.node.renderFlags | 0x20);
    if (D_88403358 >= 0) {
        Model_InitDisplayObject(&D_88403378, 0, 0, (ModelNodeView*)D_884037B0[sp1F]); // PORT: was a FAKE raw-offset read of D_884037B0 + i * 4
        ModelAnim_BindTransformCurve(&D_88403378, 1, D_88402D74[sp1F], 0x10000); // PORT: was a FAKE raw-offset read of D_88402D74 + i * 4
        ModelAnim_SetFrame(&D_88403378, 0x2E);
    }
    Audio_PlaySystemSound(0xB);
    func_80006828(20, &CartridgeCheck_DrawFrame);
}

/**
 * Second part of coming back from the main menu: plays 19 frames of the console's animation,
 * stops it and frees the Game Boy model when one was shown.
 * Seen in play (2026-11-14): after OFF > はい the camera pulled back from the monitor, and while
 * this ran the monitor went away, leaving the console as on the cartridge check screen.
 */
void CartridgeCheck_PlayLowerMonitor(void) {
    func_80006998(0xFFFF);
    func_80006828(19, &CartridgeCheck_DrawFrame);
    D_88403648.node.renderFlags = (u8) (D_88403648.node.renderFlags & 0xFFDF);
    if (D_88403358 >= 0) {
        func_800199E4(&D_88403378);
    }
}

/**
 * Last part of going to the main menu: the camera moves along D_88402E5C/D_88402E40 into the
 * monitor (「P-OS version 1.0 NOW LOADING」) until the path ends.
 * Seen in play (2026-11-14): ran after the Game Boy dropped in, until the main menu.
 */
void CartridgeCheck_PlayZoomIntoMonitor(void) {
    Obj80027168 sp7C; // PORT: was s8[0x3C]; the host struct is larger
    Obj80027168 sp40; // PORT: was s8[0x3C]; the host struct is larger
    CameraNode* temp_s0; // PORT: the scene camera (was s32)

    temp_s0 = D_88403370->children;
    Audio_StopMusic(50);
    BezierPath_InitEased(&sp7C, D_88402E5C);
    BezierPath_InitEased(&sp40, D_88402E40);
    if (BezierPath_Step(&sp7C, &temp_s0->view.eyePosition) != 0) {
        do {
            BezierPath_Step(&sp40, &temp_s0->view.targetPosition);
            if (D_88403360 < 25) {
                D_88403360 += 1;
            }
            CartridgeCheck_DrawFrame(0);
            BgStage_AdvanceFrame();
        } while (BezierPath_Step(&sp7C, &temp_s0->view.eyePosition) != 0);
    }
}

extern Key80027168 D_88402E78[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
extern Key80027168 D_88402E94[]; // PORT: Bezier path keys (was s8), so the lifter swaps the s16s
M2C_UNK Audio_PlaySystemSound(M2C_UNK);
/**
 * First part of coming back from the main menu: the camera moves along D_88402E94/D_88402E78
 * out of the monitor; on frame 15 it starts the console (and Game Boy) animation and plays sound 0x11.
 * Seen in play (2026-11-14): ran after OFF > はい on the main menu.
 */
void CartridgeCheck_PlayZoomOutOfMonitor(void) {
    Obj80027168 sp94; // PORT: was s8[0x3C]; the host struct is larger
    Obj80027168 sp58; // PORT: was s8[0x3C]; the host struct is larger
    s32 i = 0;
    CameraNode* temp_s1; // PORT: the scene camera (was s32)
    s32 pad; // FAKE: stack padding

    temp_s1 = D_88403370->children;
    BezierPath_InitEased(&sp94, D_88402E94);
    BezierPath_InitEased(&sp58, D_88402E78);
    if (BezierPath_Step(&sp94, &temp_s1->view.eyePosition) != 0) {
        do {
            if (i++ == 0xF) {
                D_88403648.node.renderFlags |= 0x20;
                if (D_88403358 >= 0) {
                    D_88403378.node.renderFlags |= 0x20;
                }
                Audio_PlaySystemSound(0x11);
            }
            BezierPath_Step(&sp58, &temp_s1->view.targetPosition);
            CartridgeCheck_DrawFrame(0);
            BgStage_AdvanceFrame();
        } while (BezierPath_Step(&sp94, &temp_s1->view.eyePosition) != 0);
    }
}

/** Empty function; nothing in the ROM calls it or holds its address (same scan as CartridgeCheck_UnusedNoop1). */
void CartridgeCheck_UnusedNoop3(void) {
}

/**
 * Plays the way into the main menu: CartridgeCheck_PlaySetCartridgeAnimation, then
 * CartridgeCheck_PlayZoomIntoMonitor, then the fade.
 * Seen in play (2026-11-14): after A on the cartridge check screen and at the end of the Game Boy present event.
 */
void CartridgeCheck_TransitionToMainMenu(void) {
    CartridgeCheck_PlaySetCartridgeAnimation();
    CartridgeCheck_PlayZoomIntoMonitor();
    func_80006998(1);
    func_80006828(2, &CartridgeCheck_DrawFrame);
    Audio_StopSoundByCategory(0x46, 0x1E);
    if (D_88403358 < 0 || D_88403310[D_88403358].saveState != 0) {
        func_80006828(20, &CartridgeCheck_DrawFrame);
    }
}

/**
 * CartridgeCheck_Main path after the title screen (fromMainMenu == 0, transitionFlags == 0),
 * with or without Transfer Paks: CartridgeCheck_PlayBootIntro, then the cartridge check
 * loop; A goes on to the main menu, B plays CartridgeCheck_PlayExitToTitle.
 * Seen in play (2026-11-14): ran after Start on the title, with no paks and with four paks.
 */
s32 CartridgeCheck_EnterFromBoot(void) {
    s32 sp1C;
    CameraNode* sp18; // PORT: the scene camera (was s32)
    s32 temp_v0;

    sp18 = D_88403370->children;
    D_88403360 = 0;
    CartridgeCheck_ReadInputs();
    StageLoader_RunFrames(1);
    Audio_PlayMusicIfChanged(0xA);
    StageFade_Start(30);
    Vec3f_FromVec3s(&sp18->view.targetPosition, &D_88402D98[0].a);
    Vec3f_FromVec3s(&sp18->view.eyePosition, &D_88402DB4[0].a);
    func_80006828(5, &CartridgeCheck_DrawFrame);
    CartridgeCheck_PlayBootIntro();
    temp_v0 = CartridgeCheck_RunScreen();
    sp1C = temp_v0;
    if (temp_v0 == 1) {
        CartridgeCheck_TransitionToMainMenu();
    } else {
        CartridgeCheck_PlayExitToTitle();
    }
    return sp1C;
}

/**
 * CartridgeCheck_Main path when transitionFlags is nonzero: keeps the active port, plays
 * CartridgeCheck_PlayCupWinIntro (with the Game Boy present event) and goes straight on to
 * the main menu without the cartridge check loop. Always returns 1.
 * Seen in play (2026-11-14) with transitionFlags poked to 0x0A at CartridgeCheck_Main entry.
 * @unverified "CupWin": transitionFlags 2 is set only in VsScene's win handler (Tournament_RecordCupClear),
 * and State_MainMenu skips this fragment when flag 1 is set without 2, so only a win should
 * reach here; a real cup win was not played.
 */
s32 CartridgeCheck_EnterAfterCupWin(void) {
    CameraNode* sp1C; // PORT: was s32

    sp1C = D_88403370->children;
    GbSave_SetActivePort(D_88403358);
    D_88403360 = 0;
    CartridgeCheck_ReadInputs();
    StageLoader_RunFrames(1);
    StageFade_Start(30);
    Vec3f_FromVec3s(&sp1C->view.targetPosition, &D_88402DD0[0].a);
    Vec3f_FromVec3s(&sp1C->view.eyePosition, &D_88402DEC[0].a);
    func_80006828(5, &CartridgeCheck_DrawFrame);
    CartridgeCheck_PlayCupWinIntro();
    CartridgeCheck_TransitionToMainMenu();
    return 1;
}

/**
 * CartridgeCheck_Main path for OFF on the main menu (fromMainMenu != 0): zooms out of the
 * monitor, lowers it and runs the cartridge check loop again.
 * Seen in play (2026-11-14): OFF > はい on the main menu ran this and returned to the cartridge check screen.
 */
s32 CartridgeCheck_EnterFromMainMenu(void) {
    s32 sp1C;
    CameraNode* sp18; // PORT: the scene camera (was s32)
    s32 temp_v0;

    sp18 = D_88403370->children;
    D_88403360 = 0;
    CartridgeCheck_ReadInputs();
    Audio_StopMusic(60);
    StageLoader_RunFrames(1);
    Vec3f_FromVec3s(&sp18->view.targetPosition, &D_88402E78[0].a);
    Vec3f_FromVec3s(&sp18->view.eyePosition, &D_88402E94[0].a);
    CartridgeCheck_PlayZoomOutOfMonitor();
    CartridgeCheck_PlayLowerMonitor();
    temp_v0 = CartridgeCheck_RunScreen();
    sp1C = temp_v0;
    if (temp_v0 == 1) {
        CartridgeCheck_TransitionToMainMenu();
    } else {
        CartridgeCheck_PlayExitToTitle();
    }
    return sp1C;
}

/**
 * Fills D_88403310 for the four ports from the Game Boy saves: trainer ID and name, which game
 * is in the pak and the save state shown on the port panels.
 * Seen in play (2026-11-14): the panels showed what it read (ID 12345 / サトシ from a crafted Green save).
 */
void CartridgeCheck_ReadPortSummaries(void) {
    s32 i;
    GbIdentityZ identity;

    for (i = 0; i < 4; i++) {
        GbSave_CopyPlayerIdentity(i, (GbSavePlayerIdentity*)&identity);
        Text_UntranscodeName(D_88403310[i].name, identity.name);
        D_88403310[i].trainerId = identity.trainerId;
        D_88403310[i].game = GbSave_GetPortGame(i);
        D_88403310[i].saveState = GbSave_GetSaveState(i);
    }
}

/**
 * Sets the cartridge cursor D_88403358: -1 when no Transfer Pak is present, otherwise the
 * active port, or the first port with a Pokémon cartridge.
 * Seen in play (2026-11-14): -1 without paks, 0 (1P) with paks in all four ports.
 */
void CartridgeCheck_InitCursor(void) {
    if (GbSave_GetPresentPortMask() == 0) {
        D_88403358 = -1;
        return;
    }
    D_88403358 = GbSave_GetActivePort();
    if ((D_88403358 < 0) || (D_88403310[D_88403358].game == 0)) {
        D_88403358 = 0;
        while (D_88403310[D_88403358].game == 0) {
            D_88403358++;
        }
    }
}

extern u8 D_88402EB0[];
extern u8 D_8F4A4240[];
extern u8 D_8F4B2AF0[];
extern u8 D_8F4959C8[];
extern u8 D_8F4886F8[];
extern u8 D_8E825FE8[];
extern u8 D_8E924770[];
// PORT: bytes, so the lifter keeps these model blobs (animation curves, then the models) in ROM byte order
extern u8 D_8E802B64[];
extern u8 D_8E902B64[];
extern u8 D_8F4014B0[];
extern Vec3f D_88403504;
extern Vec3f D_88403510;
extern s32 D_8840365C;
/**
 * Builds the scene's models: the stage, the console, the three Game Boy models (plain, Doduo,
 * Dodrio) and, for the present event, the Doduo or Dodrio Game Boy. Coming from the main menu
 * it starts the console in its raised-monitor pose, with the Game Boy for a selected port.
 * Seen in play (2026-11-14): ran on every entry (boot, OFF, the poked present event).
 */
void CartridgeCheck_InitModels(s32 from_main_menu) {
    s8 sp27;
    MemoryBlock* sp20;

    sp20 = MainPool_AllocState(MainPool_GetAvailable(), 0);
    ModelRenderer_InitDisplayRoots();
    ModelRenderer_AttachDisplayObject((DisplayObject*)&D_88403648);
    ModelRenderer_AttachDisplayObject((DisplayObject*)&D_88403378);
    ModelRenderer_AttachDisplayObject((DisplayObject*)&D_884034E0);
    D_88403370 = GeoLayout_Process(sp20, D_88402EB0);
    D_884037B0[0] = GeoLayout_Process(sp20, D_8F4A4240);
    D_884037B0[1] = GeoLayout_Process(sp20, D_8F4B2AF0);
    D_884037B0[2] = GeoLayout_Process(sp20, D_8F4959C8);
    Model_InitDisplayObject(&D_88403648, 0, 0, GeoLayout_Process(sp20, D_8F4886F8));
    if (gSessionContext.transitionFlags & 8) {
        // Doduo Game Boy prize model (fragment039, seen in play 2026-11-14).
        Model_InitDisplayObject(&D_884034E0, 1, 0, GeoLayout_Process(sp20, D_8E825FE8));
        ModelAnim_BindTransformCurve(&D_884034E0, 1, D_8E802B64, 0x10000);
        D_884034E0.node.flags &= ~1;
        Vec3f_SetComponentsDuplicate(&D_88403504, 0.0f, 275.0f, 750.0f);
        Vec3f_SetComponentsDuplicate(&D_88403510, 3.0f, 3.0f, 3.0f);
    } else if (gSessionContext.transitionFlags & 0x10) {
        // Dodrio Game Boy prize model (fragment040, seen in play 2026-11-14).
        Model_InitDisplayObject(&D_884034E0, 0, 0, GeoLayout_Process(sp20, D_8E924770));
        ModelAnim_BindTransformCurve(&D_884034E0, 1, D_8E902B64, 0x10000);
        D_884034E0.node.flags &= ~1;
        Vec3f_SetComponentsDuplicate(&D_88403504, 0.0f, 275.0f, 750.0f);
        Vec3f_SetComponentsDuplicate(&D_88403510, 3.0f, 3.0f, 3.0f);
    }
    MainPool_FinalizeAllocation(sp20);
    if (from_main_menu == 0) {
        ModelAnim_BindTransformCurve(&D_88403648, 1, D_8F4014B0, 0x10000);
        D_88403648.node.callbackArg = 0;
        D_88403648.node.renderFlags &= ~0x20;
        return;
    }
    ModelAnim_BindTransformCurve(&D_88403648, 1, &D_8F4014CC, 0xFFFF0000);
    ModelAnim_SetFrame(&D_88403648, 0x4A);
    D_88403648.node.renderFlags &= ~0x20;
    if (D_88403358 >= 0) {
        Save_GetGbPlayerSettings(&sp27, 0);
        Model_InitDisplayObject(&D_88403378, 0, 0, (ModelNodeView*)D_884037B0[sp27]); // PORT: was a FAKE raw-offset read of D_884037B0 + i * 4
        ModelAnim_BindTransformCurve(&D_88403378, 1, D_88402D74[sp27], 0xFFFF0000); // PORT: was a FAKE raw-offset read of D_88402D74 + i * 4
        ModelAnim_SetFrame(&D_88403378, 0x4A);
        D_88403378.node.renderFlags &= ~0x20;
    }
    D_8840365C = 1;
}

/**
 * Sets up the scene's 320x240 render targets in the new stage context (D_8840336C), sharing
 * the current context's buffers.
 * Seen in play (2026-11-14): ran first on every entry.
 * @unverified What the two 0x10-byte allocations hold was not checked.
 */
void CartridgeCheck_InitRenderTargets(void) {
    GfxImage* sp2C;

    sp2C = func_800051F0(0, 2, 320, 240, 0);
    // PORT: render targets by name (were raw N64 offsets 0x18/0x1C and GfxImage +8)
    D_8840336C->renderTargets[0] = MainPool_Alloc(sizeof(GfxImage), 0); // PORT: host size (0x10 on the N64)
    D_8840336C->renderTargets[1] = MainPool_Alloc(sizeof(GfxImage), 0);
    GfxImage_Initialize(D_8840336C->renderTargets[0], 0, 2, 0x140, 0xF0, (uintptr_t)D_88403368->renderTargets[0]->img_p);
    GfxImage_Initialize(D_8840336C->renderTargets[1], 0, 2, 0x140, 0xF0, (uintptr_t)D_88403368->renderTargets[1]->img_p);
    GfxImage_AttachDepthBuffer(D_8840336C->renderTargets[0], sp2C);
    GfxImage_AttachDepthBuffer(D_8840336C->renderTargets[1], sp2C);
}

/**
 * Entry point of the cartridge check scene, fragment006 (pool tag 'BCMP'): the console where
 * you pick a Game Boy cartridge after the title screen. Loads its segments, the console models
 * (0x8F400000) and the Doduo (0x8E800000) or Dodrio (0x8E900000) Game Boy for the present event,
 * reads the Game Boy saves and runs CartridgeCheck_EnterFromBoot, CartridgeCheck_EnterAfterCupWin
 * or CartridgeCheck_EnterFromMainMenu. When that returns 1 it queues the main menu fragment
 * (0x88000000) and returns 0x8040; otherwise (B) it returns 0x8010, back to the title.
 * Seen in play (2026-11-14): entered after Start on the title (fromMainMenu 0) and from OFF on
 * the main menu (fromMainMenu 1).
 */
s32 CartridgeCheck_Main(s32 from_main_menu, s32 arg1) {
    s32 result;
    s32 ret;

    MainPool_PushState('BCMP');

    D_88403368 = StageContext_GetCurrent();
    D_8840336C = StageContext_Allocate(0, 1, 2, 1, 2, 0);
    CartridgeCheck_InitRenderTargets();

    Asset_LoadToSegment(((uintptr_t)D_01000000 & 0x0F000000) >> 0x18, D_002E0BE0, D_002E2B90, 0);
    Asset_LoadToSegment(((uintptr_t)D_04000000 & 0x0F000000) >> 0x18, D_00400F70, D_004076E0, 0);
    Fragment_Load(((((uintptr_t)D_8F400000) & 0x0FF00000) >> 0x14) - 0x10, D_0038A540, D_003C5200);
    if (gSessionContext.transitionFlags & 8) {
        Fragment_Load(((((uintptr_t)D_8E800000) & 0x0FF00000) >> 0x14) - 0x10, D_003C5200, D_003CE090);
    }
    if (gSessionContext.transitionFlags & 0x10) {
        Fragment_Load(((((uintptr_t)D_8E900000) & 0x0FF00000) >> 0x14) - 0x10, D_003CE090, D_003D64A0);
    }

    Font_Init(0xF, 0);
    CartridgeCheck_ReadPortSummaries();
    CartridgeCheck_InitCursor();
    CartridgeCheck_InitModels(from_main_menu);
    StageLoader_UpdateSegments();
    StageContext_SaveAndSwitch((StageContext*)D_8840336C);
    Audio_PlayCommand(0x46, 3, 0);

    if (from_main_menu == 0) {
        if (gSessionContext.transitionFlags == 0) {
            result = CartridgeCheck_EnterFromBoot();
        } else {
            result = CartridgeCheck_EnterAfterCupWin();
        }
    } else {
        result = CartridgeCheck_EnterFromMainMenu();
    }

    StageContext_SaveAndSwitch((StageContext*)D_88403368);
    StageLoader_RunFrames(1);
    StageLoader_WaitForRetrace();
    Font_Free();

    MainPool_PopState('BCMP');

    if (result == 1) {
        Fragment_QueueCall(((((uintptr_t)D_88000000) & 0x0FF00000) >> 0x14) - 0x10, D_00235D70, D_00238BD0, 1, arg1);
        ret = 0x8040;
    } else {
        ret = 0x8010;
    }
    return ret;
}
