/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "battle.h"
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void BattleScene_BRollShotAimPokemon0(CameraNode* camera);
void BattleScene_BRollShotAimPokemon1(CameraNode* camera);
void BattleScene_BRollShotFrontHigh(CameraNode* camera);
void BattleScene_BRollShotFrontLow(CameraNode* camera);
void BattleScene_BRollShotFrontWide(CameraNode* camera);
void BattleScene_BRollShotRandomPokemon0(CameraNode* camera);
void BattleScene_BRollShotRandomPokemon1(CameraNode* camera);
void BattleScene_BRollShotWideFovRotatedX(CameraNode* camera);
void BattleScene_BRollStepNop(void);
void BattleScene_BRollCameraUpdateRotatingArenaOverview(CameraNode* arg0);
void BattleScene_SelectNextBRollCameraShot(CameraNode* arg0);
extern f32 D_FLT_8218935C[][4];
extern Vec3f D_8218B630;
extern Vec3f D_8218B63C;
void BattleScene_ClearCameraOffsetVectors(void); // PORT: signature from the definition (fragments/battle/scene_camera.c)
void BattleScene_SetCameraClipPlanes(CameraNode* camera_node, f32 near_plane, f32 far_plane); // PORT: signature from the definition (fragments/battle/scene_camera.c)
void BattleScene_SetActivePokemonIndex(s32);
void BattleCamera_StartSpeciesPath(CameraNode*, ActivePokemon*);
s32 BattleScene_SkipUnavailableBRollShot(ActivePokemon*, CameraNode*);
void BattleScene_EnterStateRow(ActivePokemon* active_pokemon, s32 row_index); // PORT: signature from the definition (fragments/battle/move_animation.c)
void BattleCamera_CaptureModelOrbit(ActivePokemon*, CameraNode*);
extern f32 D_FLT_82196040;
void BattleCamera_CaptureModelOrbit(ActivePokemon*, CameraNode*);
void BattleScene_BRollShotFrontClose(CameraNode*);
extern const f64 D_DBL_821960A0;
extern f32 D_FLT_82189358[][4];
void BattleScene_SelectNextBRollCameraShot(CameraNode*);
void BattleScene_SelectNextBRollCameraShot(CameraNode*);
void BattleScene_EaseCameraTargetFromSavedEye(CameraNode*, ActivePokemon*);
s32 BattleScene_AdvanceCameraFrameCounter(M2C_UNK);
extern f32 D_FLT_82189358[][4];
void BattleScene_SelectNextBRollCameraShot(CameraNode*);
void BattleScene_SelectNextBRollCameraShot(CameraNode*);
s32 BattleCamera_StepPath(CameraNode*, ActivePokemon*);
s32 BattleScene_AdvanceCameraFrameCounter(M2C_UNK);
void BattleScene_SetCameraEyeFromPosition(CameraNode* arg0, Vec3f arg1, s16 arg2, s16 arg3, f32 arg4, f32 arg5); // PORT: signature from the definition (fragments/battle/scene_camera.c)
extern f32 D_FLT_821960A8;
f32 BattleScene_ScaleCameraDistance(ActivePokemon*, f32, f32);
void BattleScene_EaseCameraTargetToParticipant(CameraNode*, ActivePokemon*);
s32 BattleCamera_StepPath(CameraNode*, ActivePokemon*);
void BattleScene_ResetCameraFrameCounter(void); // PORT: signature from the definition (fragments/battle/scene_camera.c)
extern u8* D_800603C0;
void Audio_FadeCategoriesAndStreams(s32 arg0);

extern u8 D_8219A4F4;
extern u8* D_8218B2B8[];
void BattleScene_InitializeBattlePresentation(CameraNode*);
void Battle_ResetModelToIdleAnim(ActivePokemon* active_pokemon); // PORT: signature from the definition (fragments/battle/participants.c)

/**
 * Selects the next B-roll camera shot from the shot list for D_8219A4F4 (wrapping at 0xFF)
 * and starts it; list 5 jumps straight to phase step 24.
 * @unverified Name guessed from the code.
 */
void BattleCamera_StartNextBRollShot(CameraNode* camera) {
    if (D_8219A4F4 == 5) {
        sBattleScene.scene->phaseStep = 24;
        BattleScene_InitializeBattlePresentation(camera);
        return;
    }
    if (D_8218B2B8[D_8219A4F4][sBattleScene.scene->bRollShotIndex] == 0xFF) {
        sBattleScene.scene->bRollShotIndex = 0;
    }
    sBattleScene.scene->phaseStep = D_8218B2B8[D_8219A4F4][sBattleScene.scene->bRollShotIndex];
    sBattleScene.scene->bRollShotIndex++;
    Battle_ResetModelToIdleAnim(sActivePokemon[0]);
    Battle_ResetModelToIdleAnim(sActivePokemon[1]);
    BattleScene_ResetCameraFrameCounter();
    switch (sBattleScene.scene->phaseStep) {
        case 0:
            BattleScene_BRollCameraSetupArenaOrbit(camera);
            break;
        case 2:
            BattleScene_BRollCameraSetupRotatingArenaOverview(camera);
            break;
        case 4:
            BattleScene_BRollShotFrontHigh(camera);
            break;
        case 6:
            BattleScene_BRollShotRandomPokemon0(camera);
            break;
        case 8:
            BattleScene_BRollShotRandomPokemon1(camera);
            break;
        case 10:
            BattleScene_BRollShotFrontLow(camera);
            break;
        case 12:
            BattleScene_BRollShotAimPokemon0(camera);
            break;
        case 14:
            BattleScene_BRollShotAimPokemon1(camera);
            break;
        case 16:
            BattleScene_BRollShotWideFovRotatedX(camera);
            break;
        case 18:
            BattleScene_BRollShotWideFovRotatedX(camera);
            break;
        case 20:
            BattleScene_BRollShotFrontWide(camera);
            break;
        case 22:
            BattleScene_BRollShotFrontClose(camera);
            break;
    }
    BattleScene_UpdateParticipantsInView(camera);
}
