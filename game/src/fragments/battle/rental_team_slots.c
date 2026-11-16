/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "battle.h"
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void BattleScene_BRollDollyIn(CameraNode* camera);
void BattleScene_BRollHoldFollow60Frames(CameraNode* camera);
void BattleScene_BRollHoldOrbit(CameraNode* camera);
void BattleScene_BRollRaiseRotationXRestoreFov(CameraNode* camera);
void BattleScene_BRollStepOrbitUntilEnd(CameraNode* camera);
void BattleScene_BRollTiltUpHigh(CameraNode* camera);
void BattleScene_BRollTiltUpToHead(CameraNode* camera);
void BattleScene_BRollWait440Frames(s32 camera);
void BattleScene_BRollWaitForButtonToExit(CameraNode* camera);

extern f32 D_FLT_8218935C[][4];
extern Vec3f D_8218B630;
extern Vec3f D_8218B63C;
void BattleScene_ClearCameraOffsetVectors(void); // PORT: signature from the definition (fragments/battle/scene_camera.c)
void BattleScene_SetCameraClipPlanes(CameraNode* camera_node, f32 near_plane, f32 far_plane); // PORT: signature from the definition (fragments/battle/scene_camera.c)
void BattleScene_SetActivePokemonIndex(s32);
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
s32 BattleScene_AdvanceCameraFrameCounter(M2C_UNK);
void BattleScene_SetCameraEyeFromPosition(CameraNode* arg0, Vec3f arg1, s16 arg2, s16 arg3, f32 arg4, f32 arg5); // PORT: signature from the definition (fragments/battle/scene_camera.c)
extern f32 D_FLT_821960A8;
void BattleScene_EaseCameraTargetToParticipant(CameraNode*, ActivePokemon*);
void BattleScene_ResetCameraFrameCounter(void); // PORT: signature from the definition (fragments/battle/scene_camera.c)
extern u8* D_800603C0;

void BattleScene_BRollCameraUpdateArenaOrbit(CameraNode*);
void BattleScene_BRollCameraUpdateRotatingArenaOverview(CameraNode*);
void BattleCamera_StartNextBRollShot(CameraNode*);
/**
 * Not reached in play (2026-11-13): no move in the 165-move battle sweep called it (each move used
 * once, from the command menu to the end of its turn).
 * @unverified Name was previously used only by callers of this address; the definition was still func_.
 */
void Rental_TeamSlotsUpdate(CameraNode* arg0) {
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
        case 25:
            BattleScene_BRollStepOrbitUntilEnd(arg0);
            break;
        case 27:
            BattleScene_BRollWait440Frames((uintptr_t)arg0);
            break;
        case 29:
            BattleScene_BRollWaitForButtonToExit(arg0);
            break;
        case 50:
            BattleCamera_StartNextBRollShot(arg0);
            break;
    }
}
