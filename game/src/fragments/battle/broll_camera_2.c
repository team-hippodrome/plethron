/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "battle.h"
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void BattleScene_BRollDollyIn(CameraNode* camera);
void BattleScene_BRollHoldFollow60Frames(CameraNode* camera);
void BattleScene_BRollHoldOrbit(CameraNode* camera);
void BattleScene_BRollRaiseRotationXRestoreFov(CameraNode* camera);
void BattleScene_BRollTiltUpHigh(CameraNode* camera);
void BattleScene_BRollTiltUpToHead(CameraNode* camera);
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

void Battle_ChooseActionsForMode(void);
void BattleMenu_ResetCommandTimer(void);

/**
 * Per-frame update while unk_1C is 1 (b-roll camera mode): runs the action selection phase and
 * dispatches to the update function of the current b-roll shot (odd cameraCounter values).
 */
void BattleScene_UpdateBRollCameraSequence(CameraNode* arg0) {
    if (sBattleScene.scene->mode == 1) {
        if ((sBattleScene.scene->transitionStep == 0) && (sActivePokemon[0]->thumbnail.camera->viewport.x == 0x17) &&
            (sActivePokemon[1]->thumbnail.camera->viewport.x == 0x103)) {
            Battle_ChooseActionsForMode();
        } else {
            BattleMenu_ResetCommandTimer();
        }
    }
    if (sBattleScene.scene->mode == 5) {
        Battle_ChooseActionsForMode();
    }
    switch (sBattleScene.scene->phaseStep) {
        case 1:
            BattleScene_BRollCameraUpdateArenaOrbit(arg0);
            break;
        case 3:
            BattleScene_BRollCameraUpdateRotatingArenaOverview(arg0);
            break;
        case 5:
            BattleScene_BRollTiltUpToHead(arg0);
            break;
        case 7:
            BattleScene_BRollHoldFollow60Frames(arg0);
            break;
        case 9:
            BattleScene_BRollHoldFollow60Frames(arg0);
            break;
        case 11:
            BattleScene_BRollTiltUpHigh(arg0);
            break;
        case 13:
            BattleScene_BRollHoldOrbit(arg0);
            break;
        case 15:
            BattleScene_BRollHoldOrbit(arg0);
            break;
        case 17:
            BattleScene_BRollRaiseRotationXRestoreFov(arg0);
            break;
        case 19:
            BattleScene_BRollRaiseRotationXRestoreFov(arg0);
            break;
        case 21:
            BattleScene_BRollDollyIn(arg0);
            break;
        case 23:
            BattleScene_BRollCameraUpdatePresetHold(arg0);
            break;
        case 50:
            BattleScene_SelectNextBRollCameraShot(arg0);
            break;
    }
}
