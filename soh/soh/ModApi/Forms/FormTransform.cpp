#include "FormTransform.h"

#include <string>
#include <unordered_set>

#include <spdlog/spdlog.h>
#include <libultraship/bridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/TimeControl/TimeControl.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "sfx.h"
extern PlayState* gPlayState;
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
f32 Camera_LERPCeilF(f32 target, f32 cur, f32 stepScale, f32 minDiff);
s16 Camera_LERPCeilS(s16 target, s16 cur, f32 stepScale, s16 minDiff);
void Camera_LERPCeilVec3f(Vec3f* target, Vec3f* cur, f32 yStepScale, f32 xzStepScale, f32 minDiff);
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

constexpr const char* TimeControlOwner = "forms.transform";
constexpr int32_t TimeControlPriority = 10000;
constexpr float PoseAnimSpeed = 2.0f / 3.0f;
constexpr float MaskFaceFrame = 12.0f;
constexpr float MaskClimaxFrame = 51.0f;
constexpr int32_t PutOnLastFrame = 0x53;
constexpr int32_t TakeOffLastFrame = 0x37;
constexpr int32_t SkipFirstFrame = 5;
constexpr int32_t SkipButtons = BTN_A | BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT;
constexpr int32_t FlashStep = 45;
constexpr int32_t PostFlashHold = 24;
constexpr int32_t EnvStageFirstFrame = 16;
constexpr float EnvStageRate = 10.0f / 100.0f;
constexpr int32_t EnvLightningFrame = 59;
constexpr int32_t EnvStage3Frame = 63;
constexpr float PutOnSfxFrames[] = { 2.0f, 4.0f, 11.0f, 20.0f, 30.0f };
constexpr float TakeOffMaskSfxFrame = 8.0f;
constexpr int32_t FaceChangeFrame = 15;

constexpr const char* MmSfxPlayService = "mm.sfx.play";
constexpr const char* MmSfxStopService = "mm.sfx.stop";
constexpr uint16_t MmSfxFreezeS = 0x0874;
constexpr uint16_t MmSfxPutOutItem = 0x0877;
constexpr uint16_t MmSfxFaceChange = 0x09A4;
constexpr uint16_t MmSfxTransformVoice = 0x09AA;
constexpr uint16_t MmSfxSetTransformMask = 0x1856;
constexpr uint16_t MmSfxTransformMaskBroken = 0x1858;
constexpr uint16_t MmSfxLightningHard = 0x2912;
constexpr uint16_t MmSfxTransformMaskFlash = 0x484F;
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);
typedef void (*MmSfxStopFunc)(uint16_t sfxId);

constexpr const char* FaceGlowDl = "__OTR__objects/gameplay_keep/gameplay_keep_DL_054C90";
constexpr int32_t FaceGlowSegment = 0x0B;
constexpr Vec3f LinkFaceGlowOffset = { -230.0f, -520.0f, 0.0f };

#define DEG_TO_BINANG_F(degrees) ((s16)(s32)((degrees)*182.04167f))

struct EnvKey {
    s16 fogNear;
    u8 fogColor[3];
    u8 ambientColor[3];
};

struct LightKey {
    Vec3f pos;
    u8 color[3];
    s16 radius;
};

constexpr EnvKey EnvKeys[3] = {
    { 650, { 0, 0, 0 }, { 10, 0, 30 } },
    { 300, { 200, 200, 255 }, { 0, 0, 0 } },
    { 600, { 0, 0, 0 }, { 0, 0, 200 } },
};

enum LightKeyBody {
    LinkBodyOnScreen,
    FormBodyOnScreen,
};

constexpr LightKey LightKeys[2][3] = {
    {
        { { -40.0f, 20.0f, -10.0f }, { 120, 200, 255 }, 1000 },
        { { 0.0f, -10.0f, 0.0f }, { 255, 255, 255 }, 5000 },
        { { -10.0f, 4.0f, 3.0f }, { 200, 200, 255 }, 5000 },
    },
    {
        { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
        { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
        { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
    },
};

constexpr u8 Black[3] = { 0, 0, 0 };

enum TransformPhase {
    PhasePose,
    PhaseFlashIn,
    PhaseFlashOut,
};

enum TransformDistortion {
    DistortNone,
    DistortWobble,
    DistortHold,
};

enum PutOnCameraShot {
    PutOnStart,
    PutOnFollowMaskToFace,
    PutOnSettle,
    PutOnPushIntoFace,
    PutOnReturn,
};

enum TakeOffCameraShot {
    TakeOffStart,
    TakeOffHoldOnHands,
    TakeOffRollAway,
    TakeOffReturn,
};

bool sActive = false;
bool sPendingStart = false;
bool sFromForm = false;
bool sIsMaskCutscene = false;
TransformPhase sPhase = PhasePose;
int32_t sFrame = 0;
int32_t sFlashAlpha = 0;
int32_t sHoldTimer = 0;
std::string sTargetKey;
const char* sMaskDl = nullptr;
const char* sMaskClimaxDl = nullptr;
LinkAnimationHeader* sPoseAnim = nullptr;
uint16_t sVoiceSfx = 0;
bool sMaskClimax = false;
std::unordered_set<std::string> sSeenForms;

float sEnvStage = 0.0f;
float sLightStage = 0.0f;
float sMaskSquashZ = 0.0f;
float sMaskSquashY = 0.0f;
s16 sFaceGlowAlpha = 0;
Vec3f sFaceGlowOffset = LinkFaceGlowOffset;
float sFaceGlowScale = 1.0f;

LightInfo sLightInfo;
LightNode* sLightNode = nullptr;

s16 sSubCamId = SUBCAM_NONE;
int32_t sCamState = 0;
int32_t sCamTimer = 0;
float sCamRoll = 0.0f;
float sCamSeed = 0.0f;
float sCamFovStep = 0.0f;
float sCamTakeOffRadius = 0.0f;
s16 sCamYaw = 0;
VecSph sCamStartGeo;
float sCamStartFov = 60.0f;

uint8_t sDistortType = DistortNone;
bool sDistortNeedsSetup = false;
bool sDistortApplied = false;
s16 sDistortTimer = 0;
s16 sDistortDuration = 1;
s16 sDepthPhase = 0;
s16 sPlanePhase = 0;

Player* GetPlayer() {
    return gPlayState == nullptr ? nullptr : GET_PLAYER(gPlayState);
}

bool IsInstantTransform() {
    return CVarGetInteger(CVAR_ENHANCEMENT("Forms.InstantTransform"), 0) != 0;
}

void StepMaskRamps() {
    if (sFromForm) {
        if (sFrame >= 20) {
            Math_StepToS(&sFaceGlowAlpha, 255, 20);
        }
        return;
    }
    if (sFrame >= 58) {
        Math_StepToS(&sFaceGlowAlpha, 255, 50);
    }
    if (sFrame >= 64) {
        Math_StepToF(&sMaskSquashZ, 0.0f, 0.015f);
    } else if (sFrame >= 14) {
        Math_StepToF(&sMaskSquashZ, 0.3f, 0.3f);
    }
    if (sFrame > 65) {
        Math_StepToF(&sMaskSquashY, 0.0f, 0.02f);
    } else if (sFrame >= 16) {
        Math_StepToF(&sMaskSquashY, -0.1f, 0.1f);
    }
}

void PlaySfx(uint16_t mmSfx, uint16_t ootSfx, Vec3f* pos) {
    auto playMmSfx = reinterpret_cast<MmSfxPlayFunc>(ModApi_Get()->FindService(MmSfxPlayService));
    if (playMmSfx != nullptr && playMmSfx(mmSfx, pos)) {
        return;
    }
    if (ootSfx == 0) {
        return;
    }
    if (pos == nullptr) {
        Sfx_PlaySfxCentered(ootSfx);
    } else {
        Sfx_PlaySfxAtPos(pos, ootSfx);
    }
}

void StopSfx(uint16_t mmSfx, uint16_t ootSfx) {
    auto stopMmSfx = reinterpret_cast<MmSfxStopFunc>(ModApi_Get()->FindService(MmSfxStopService));
    if (stopMmSfx != nullptr) {
        stopMmSfx(mmSfx);
    }
    if (ootSfx != 0) {
        Audio_StopSfxById(ootSfx);
    }
}

void StepEnvAndLightStages() {
    if (sFrame < EnvStageFirstFrame) {
        return;
    }
    if (sFrame < EnvLightningFrame) {
        Math_StepToF(&sEnvStage, 1.0f, EnvStageRate);
    } else if (sFrame < EnvStage3Frame) {
        if (sFrame == EnvLightningFrame) {
            PlaySfx(MmSfxLightningHard, NA_SE_EV_LIGHTNING, nullptr);
        }
        Math_StepToF(&sEnvStage, 2.0f, 0.5f);
    } else {
        Math_StepToF(&sEnvStage, 3.0f, 0.2f);
    }
    if (sFrame < 64) {
        Math_StepToF(&sLightStage, 1.0f, 0.2f);
    } else {
        Math_StepToF(&sLightStage, 3.0f, 0.55f);
    }
}

s16 InterpolateColor(u8 to, u8 from, u8 base, float t) {
    return (s16)((s32)((to - from) * t) + from - base);
}

void ApplyEnvStage(PlayState* play, float stage) {
    EnvLightSettings* base = &play->envCtx.lightSettings;
    EnvKey current;
    current.fogNear = base->fogNear;
    for (s32 i = 0; i < 3; i++) {
        current.fogColor[i] = base->fogColor[i];
        current.ambientColor[i] = base->ambientColor[i];
    }

    const EnvKey* from = &current;
    const EnvKey* to = &EnvKeys[0];
    const u8* light1From = base->light1Color;
    const u8* light1To = Black;
    float t = stage;

    if (stage > 3.0f) {
        t = stage - 3.0f;
        from = &EnvKeys[2];
        to = &current;
        light1From = Black;
        light1To = base->light1Color;
    } else if (stage > 2.0f) {
        t = stage - 2.0f;
        from = &EnvKeys[1];
        to = &EnvKeys[2];
        light1From = Black;
        light1To = Black;
    } else if (stage > 1.0f) {
        t = stage - 1.0f;
        from = &EnvKeys[0];
        to = &EnvKeys[1];
        light1From = Black;
        light1To = Black;
    }

    play->envCtx.adjFogNear = (s16)((s32)((to->fogNear - from->fogNear) * t) + from->fogNear - base->fogNear);
    for (s32 i = 0; i < 3; i++) {
        play->envCtx.adjFogColor[i] = InterpolateColor(to->fogColor[i], from->fogColor[i], base->fogColor[i], t);
        play->envCtx.adjAmbientColor[i] =
            InterpolateColor(to->ambientColor[i], from->ambientColor[i], base->ambientColor[i], t);
        play->envCtx.adjLight1Color[i] = InterpolateColor(light1To[i], light1From[i], base->light1Color[i], t);
    }
}

void ClearEnvAdjust(PlayState* play) {
    play->envCtx.adjFogNear = 0;
    for (s32 i = 0; i < 3; i++) {
        play->envCtx.adjFogColor[i] = 0;
        play->envCtx.adjAmbientColor[i] = 0;
        play->envCtx.adjLight1Color[i] = 0;
    }
}

void ApplyPointLight(Player* player, float stage) {
    const LightKey* keys = LightKeys[sFromForm ? FormBodyOnScreen : LinkBodyOnScreen];
    const LightKey* key = &keys[0];
    s16 yaw = player->actor.shape.rot.y;

    if (stage > 2.0f) {
        stage -= 2.0f;
        key = &keys[2];
    } else if (stage > 1.0f) {
        stage -= 1.0f;
        key = &keys[1];
    }
    Vec3f pos;
    pos.x = player->actor.world.pos.x + (key->pos.x * Math_CosS(yaw)) + (key->pos.z * Math_SinS(yaw));
    pos.y = player->actor.world.pos.y + key->pos.y;
    pos.z = player->actor.world.pos.z - (key->pos.x * Math_SinS(yaw)) + (key->pos.z * Math_CosS(yaw));
    Lights_PointNoGlowSetInfo(&sLightInfo, pos.x, pos.y, pos.z, key->color[0], key->color[1], key->color[2],
                              (s16)(key->radius * stage));
}

void RequestDistortion(uint8_t type, s16 duration) {
    sDistortType = type;
    sDistortNeedsSetup = true;
    sDistortTimer = duration;
}

void RemoveDistortion() {
    sDistortType = DistortNone;
}

void StepDistortion(PlayState* play) {
    if (sDistortType == DistortNone) {
        if (sDistortApplied) {
            View_ClearDistortion(&play->view);
            sDistortApplied = false;
        }
        return;
    }

    float depthStep;
    float planeStep;
    float rotZ;
    float xScale;
    float yScale;
    float speed;
    float factor;

    if (sDistortType == DistortHold) {
        sDistortTimer = 2;
        sDepthPhase = 0x3F0;
        sPlanePhase = 0x156;
        depthStep = 0.0f;
        planeStep = 170.0f;
        rotZ = 0.0f;
        xScale = -0.01f;
        yScale = 0.01f;
        speed = 0.6f;
        factor = sDistortTimer / 60.0f;
    } else {
        if (sDistortNeedsSetup) {
            sDistortDuration = sDistortTimer;
            sDepthPhase = 0x1FC;
            sPlanePhase = 0x156;
        }
        depthStep = -5.0f;
        planeStep = 5.0f;
        rotZ = 2.0f;
        xScale = 0.3f;
        yScale = 0.3f;
        speed = 0.1f;
        factor = ((float)sDistortDuration - sDistortTimer) / (float)sDistortDuration;
    }
    sDistortNeedsSetup = false;
    sDistortApplied = true;

    sDepthPhase += DEG_TO_BINANG_F(depthStep);
    sPlanePhase += DEG_TO_BINANG_F(planeStep);
    View_SetDistortionOrientation(&play->view, 0.0f, 0.0f, (f32)(Math_SinS(sPlanePhase) * (DEG_TO_RAD(rotZ) * factor)));
    View_SetDistortionScale(&play->view, (Math_SinS(sPlanePhase) * (xScale * factor)) + 1.0f,
                            (Math_CosS(sPlanePhase) * (yScale * factor)) + 1.0f, 1.0f);
    View_SetDistortionSpeed(&play->view, speed);

    if (sDistortTimer != 0) {
        sDistortTimer--;
        if (sDistortTimer == 0) {
            sDistortType = DistortNone;
        }
    }
}

void CommitCameraEye(Camera* cam, VecSph* atToEye) {
    Vec3f offset;
    OLib_VecSphGeoToVec3f(&offset, atToEye);
    cam->eyeNext.x = cam->at.x + offset.x;
    cam->eyeNext.y = cam->at.y + offset.y;
    cam->eyeNext.z = cam->at.z + offset.z;
    cam->eye = cam->eyeNext;
    cam->dist = atToEye->r;
}

void StepCameraPutOn(Player* player, Camera* cam) {
    Vec3f* playerPos = &player->actor.world.pos;
    s16 playerYaw = player->actor.shape.rot.y;
    VecSph atToEye;
    PosRot focus;
    float t;

    OLib_Vec3fDiffToVecSphGeo(&atToEye, &cam->at, &cam->eye);
    Actor_GetFocus(&focus, &player->actor);

    switch (sCamState) {
        case PutOnStart:
            sCamState = PutOnFollowMaskToFace;
            sCamTimer = 0;
            if (atToEye.r > 40.0f) {
                atToEye.r = 40.0f;
            }
            cam->fov = 80.0f;
            sCamSeed = (Rand_ZeroOne() - 0.5f) * 40.0f;
            [[fallthrough]];
        case PutOnFollowMaskToFace:
            if (sCamTimer >= 12) {
                sCamRoll = (sCamTimer - 12) * (135.0f / 13.0f);
                sCamRoll = ((sCamSeed < 0.0f) ? -1.0f : 1.0f) * Math_SinF((f32)DEG_TO_RAD(sCamRoll));
                if (sCamTimer == 12) {
                    RequestDistortion(DistortWobble, 26);
                }
            } else {
                sCamRoll = 0.0f;
            }
            t = sCamTimer * (6.0f / 19.0f);
            sCamYaw = playerYaw + 0x4000;
            focus.pos.x = (Math_SinS(sCamYaw) * t * sCamRoll) + playerPos->x;
            focus.pos.z = (Math_CosS(sCamYaw) * t * sCamRoll) + playerPos->z;
            focus.pos.y -= (focus.pos.y - playerPos->y) * 0.1f;
            Camera_LERPCeilVec3f(&focus.pos, &cam->at, 0.2f, 0.2f, 0.1f);
            t = sCamTimer * (30.0f / 19.0f);
            cam->roll = DEG_TO_BINANG_F(t * sCamRoll);
            t = 1.0f / (38 - sCamTimer);
            sCamTimer++;
            atToEye.r = Camera_LERPCeilF(30.0f, atToEye.r, t, 0.1f);
            atToEye.pitch = 0;
            if (sCamTimer >= 38) {
                sCamTimer = 24;
                sCamState = PutOnSettle;
                sCamFovStep = (32.0f - cam->fov) / 24.0f;
                RequestDistortion(DistortHold, 2);
            }
            break;

        case PutOnSettle:
            if (sCamTimer == 24) {
                cam->at.x = (Math_SinS(playerYaw) * -7.0f) + playerPos->x;
                cam->at.y = focus.pos.y - ((focus.pos.y - playerPos->y) * 0.1f);
                cam->at.z = (Math_CosS(playerYaw) * -7.0f) + playerPos->z;
            } else {
                focus.pos.x = (Math_SinS(playerYaw) * -7.0f) + playerPos->x;
                focus.pos.y -= (focus.pos.y - playerPos->y) * 0.1f;
                focus.pos.z = (Math_CosS(playerYaw) * -7.0f) + playerPos->z;
                Camera_LERPCeilVec3f(&focus.pos, &cam->at, 0.25f, 0.25f, 0.1f);
            }
            if (sCamTimer > 0) {
                cam->fov += sCamFovStep;
            }
            sCamTimer--;
            atToEye.r = 35.0f;
            atToEye.pitch = 0x2000;
            cam->roll = Camera_LERPCeilS(0, cam->roll, 0.1f, 5);
            if (sCamTimer <= 0) {
                sCamTimer = 0;
                sCamFovStep = (60.0f - cam->fov) / 630.0f;
                sCamState = PutOnPushIntoFace;
            }
            break;

        case PutOnPushIntoFace:
            focus.pos.x = playerPos->x;
            focus.pos.y -= (focus.pos.y - playerPos->y) * 0.1f;
            focus.pos.z = playerPos->z;
            Camera_LERPCeilVec3f(&focus.pos, &cam->at, 0.25f, 0.25f, 0.1f);
            cam->roll = Camera_LERPCeilS(0, cam->roll, 0.1f, 5);
            sCamTimer++;
            cam->fov += sCamFovStep * sCamTimer;
            atToEye.pitch = 0x2000;
            atToEye.r = 35.0f;
            if (sCamTimer >= 35) {
                RemoveDistortion();
                sCamState = PutOnReturn;
            }
            break;

        case PutOnReturn:
        default:
            focus.pos.y -= (focus.pos.y - playerPos->y) * 0.1f;
            Camera_LERPCeilVec3f(&focus.pos, &cam->at, 0.1f, 0.1f, 0.1f);
            atToEye = sCamStartGeo;
            cam->fov = sCamStartFov;
            cam->roll = 0;
            break;
    }
    CommitCameraEye(cam, &atToEye);
}

void StepCameraTakeOff(Player* player, Camera* cam) {
    Vec3f* playerPos = &player->actor.world.pos;
    VecSph atToEye;
    PosRot focus;
    float t;

    OLib_Vec3fDiffToVecSphGeo(&atToEye, &cam->at, &cam->eye);
    Actor_GetFocus(&focus, &player->actor);

    switch (sCamState) {
        case TakeOffStart:
            sCamState = TakeOffHoldOnHands;
            atToEye.pitch = 0;
            sCamTimer = 18;
            sCamTakeOffRadius = 80.0f;
            atToEye.r = 30.0f;
            cam->fov = 80.0f;
            sCamSeed = (Rand_ZeroOne() - 0.5f) * 40.0f;
            cam->roll = 0;
            focus.pos.x = playerPos->x;
            focus.pos.z = playerPos->z;
            cam->at = focus.pos;
            [[fallthrough]];
        case TakeOffHoldOnHands:
            sCamTimer--;
            if (sCamTimer <= 0) {
                sCamState = TakeOffRollAway;
                sCamYaw = player->actor.shape.rot.y + 0x4000;
                sCamTimer = 46;
                RequestDistortion(DistortWobble, 46);
            }
            break;

        case TakeOffRollAway:
            sCamRoll = sCamTimer * (180.0f / 23.0f);
            sCamRoll = ((sCamSeed < 0.0f) ? -1.0f : 1.0f) * Math_SinF((f32)DEG_TO_RAD(sCamRoll));
            t = (46 - sCamTimer) * (5.0f / 46.0f);
            focus.pos.x = (Math_SinS(sCamYaw) * t * sCamRoll) + playerPos->x;
            focus.pos.z = (Math_CosS(sCamYaw) * t * sCamRoll) + playerPos->z;
            focus.pos.y -= (focus.pos.y - playerPos->y) * 0.2f;
            Camera_LERPCeilVec3f(&focus.pos, &cam->at, 0.1f, 0.1f, 0.1f);
            t = sCamTimer * (10.0f / 23.0f);
            cam->roll = DEG_TO_BINANG_F(sCamRoll * t);
            t = 1.0f / sCamTimer;
            atToEye.r = Camera_LERPCeilF(sCamTakeOffRadius, atToEye.r, t, 0.1f);
            sCamTimer--;
            atToEye.pitch = 0;
            if (sCamTimer <= 0) {
                sCamState = TakeOffReturn;
                RemoveDistortion();
            }
            break;

        case TakeOffReturn:
        default:
            focus.pos.y -= (focus.pos.y - playerPos->y) * 0.1f;
            Camera_LERPCeilVec3f(&focus.pos, &cam->at, 0.1f, 0.1f, 0.1f);
            cam->roll = 0;
            atToEye = sCamStartGeo;
            cam->fov = sCamStartFov;
            break;
    }
    CommitCameraEye(cam, &atToEye);
}

void StepCamera(PlayState* play, Player* player) {
    if (sSubCamId == SUBCAM_NONE) {
        return;
    }
    Camera* cam = play->cameraPtrs[sSubCamId];
    if (sFromForm) {
        StepCameraTakeOff(player, cam);
    } else {
        StepCameraPutOn(player, cam);
    }
}

void EndCameraAnim() {
    sCamState = sFromForm ? TakeOffReturn : PutOnReturn;
    RemoveDistortion();
}

void ReleaseCamera(PlayState* play) {
    if (sSubCamId == SUBCAM_NONE) {
        return;
    }
    Play_CopyCamera(play, MAIN_CAM, sSubCamId);
    Play_ChangeCameraStatus(play, sSubCamId, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_ACTIVE);
    Play_ClearCamera(play, sSubCamId);
    sSubCamId = SUBCAM_NONE;
}

void TakeCamera(PlayState* play) {
    if (sSubCamId != SUBCAM_NONE) {
        return;
    }
    s16 camId = Play_CreateSubCamera(play);
    if (camId <= MAIN_CAM) {
        return;
    }
    Camera* activeCam = GET_ACTIVE_CAM(play);
    Camera* subCam = play->cameraPtrs[camId];
    sSubCamId = camId;
    subCam->target = NULL;
    subCam->at = activeCam->at;
    subCam->eye = subCam->eyeNext = activeCam->eye;
    subCam->fov = activeCam->fov;
    subCam->roll = 0;
    OLib_Vec3fDiffToVecSphGeo(&sCamStartGeo, &subCam->at, &subCam->eye);
    sCamStartFov = subCam->fov;
    Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, sSubCamId, CAM_STAT_ACTIVE);
}

bool WantsSkip(PlayState* play) {
    if (sFrame < SkipFirstFrame || !(sFromForm || sSeenForms.contains(sTargetKey))) {
        return false;
    }
    return CHECK_BTN_ANY(play->state.input[0].press.button, SkipButtons);
}

void PlayPoseSfx(Player* player) {
    Vec3f* pos = &player->actor.projectedPos;
    SkelAnime* skelAnime = &player->skelAnime;

    if (!sIsMaskCutscene || skelAnime->animation != sPoseAnim) {
        return;
    }
    if (sFromForm) {
        if (LinkAnimation_OnFrame(skelAnime, TakeOffMaskSfxFrame)) {
            PlaySfx(MmSfxSetTransformMask, NA_SE_PL_CHANGE_ARMS, pos);
        }
        if (sFrame == FaceChangeFrame) {
            PlaySfx(MmSfxFaceChange, NA_SE_EN_TWINROBA_TRANSFORM, pos);
        }
        return;
    }
    if (LinkAnimation_OnFrame(skelAnime, PutOnSfxFrames[0])) {
        PlaySfx(MmSfxPutOutItem, NA_SE_PL_PUT_OUT_ITEM, pos);
    }
    if (LinkAnimation_OnFrame(skelAnime, PutOnSfxFrames[1])) {
        PlaySfx(MmSfxSetTransformMask, NA_SE_PL_CHANGE_ARMS, pos);
    }
    if (LinkAnimation_OnFrame(skelAnime, PutOnSfxFrames[2])) {
        PlaySfx(MmSfxFreezeS, NA_SE_PL_FREEZE_S, pos);
    }
    if (LinkAnimation_OnFrame(skelAnime, PutOnSfxFrames[3])) {
        PlaySfx(MmSfxTransformMaskBroken, NA_SE_EN_TWINROBA_TRANSFORM, pos);
    }
    if (LinkAnimation_OnFrame(skelAnime, PutOnSfxFrames[4])) {
        PlaySfx(MmSfxTransformVoice, sVoiceSfx, pos);
    }
}

void StopPoseSfx() {
    if (sFromForm) {
        StopSfx(MmSfxFaceChange, NA_SE_EN_TWINROBA_TRANSFORM);
        return;
    }
    StopSfx(MmSfxTransformVoice, sVoiceSfx);
    StopSfx(MmSfxTransformMaskBroken, NA_SE_EN_TWINROBA_TRANSFORM);
}

bool StepBeforeFlash(PlayState* play, Player* player) {
    bool startFlash = false;
    int32_t lastFrame = sFromForm ? TakeOffLastFrame : PutOnLastFrame;

    StepMaskRamps();
    PlayPoseSfx(player);
    if (sFrame > lastFrame) {
        startFlash = true;
    } else if (WantsSkip(play)) {
        startFlash = true;
        EndCameraAnim();
        StopPoseSfx();
    }
    if (startFlash) {
        PlaySfx(MmSfxTransformMaskFlash, NA_SE_EV_WHITE_OUT, &player->actor.projectedPos);
    }
    sFrame++;

    StepEnvAndLightStages();
    ApplyEnvStage(play, sEnvStage);
    ApplyPointLight(player, sLightStage);
    StepDistortion(play);
    StepCamera(play, player);
    return startFlash;
}

void StepUnderFlash(PlayState* play, Player* player) {
    ApplyEnvStage(play, sEnvStage);
    ApplyPointLight(player, sLightStage);
    StepDistortion(play);
    StepCamera(play, player);
}

void StepAfterFlash(PlayState* play, Player* player) {
    Math_StepToF(&sLightStage, 4.0f, 0.2f);
    ApplyPointLight(player, sLightStage);
    StepDistortion(play);
    StepCamera(play, player);
}

void Stop(PlayState* play, Player* player) {
    sActive = false;
    sPendingStart = false;
    sMaskDl = nullptr;
    sMaskClimaxDl = nullptr;
    sFlashAlpha = 0;
    ClearEnvAdjust(play);
    if (sDistortApplied) {
        View_ClearDistortion(&play->view);
        sDistortApplied = false;
    }
    sDistortType = DistortNone;
    if (sLightNode != nullptr) {
        LightContext_RemoveLight(play, &play->lightCtx, sLightNode);
        sLightNode = nullptr;
    }
    ReleaseCamera(play);
    TimeControl_Release(TimeControlOwner);
    if (player != nullptr) {
        player->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE);
    }
}

void Forget(int16_t sceneNum) {
    sActive = false;
    sPendingStart = false;
    sMaskDl = nullptr;
    sMaskClimaxDl = nullptr;
    sFlashAlpha = 0;
    sSubCamId = SUBCAM_NONE;
    sLightNode = nullptr;
    sDistortType = DistortNone;
    sDistortApplied = false;
    TimeControl_Release(TimeControlOwner);
}

void ApplyFormAtFlashPeak(PlayState* play) {
    sLightStage = 3.0f;
    sMaskClimax = false;
    sMaskDl = nullptr;
    sFaceGlowAlpha = 0;
    ClearEnvAdjust(play);
    FormRegistry_ApplyActive(sTargetKey.empty() ? nullptr : sTargetKey.c_str());
}

void TransformAction(Player* player, PlayState* play) {
    player->linearVelocity = 0.0f;
    player->actor.velocity.y = 0.0f;

    switch (sPhase) {
        case PhasePose:
            LinkAnimation_Update(play, &player->skelAnime);
            if (StepBeforeFlash(play, player)) {
                sPhase = PhaseFlashIn;
                sFlashAlpha = 0;
                sSeenForms.insert(sTargetKey);
            }
            break;

        case PhaseFlashIn:
            LinkAnimation_Update(play, &player->skelAnime);
            StepUnderFlash(play, player);
            sFlashAlpha += FlashStep;
            if (sFlashAlpha >= 255) {
                sFlashAlpha = 255;
                ApplyFormAtFlashPeak(play);
                sPhase = PhaseFlashOut;
                sHoldTimer = 0;
            }
            break;

        default:
            sFlashAlpha -= FlashStep;
            if (sFlashAlpha < 0) {
                sFlashAlpha = 0;
            }
            sHoldTimer++;
            StepAfterFlash(play, player);
            if (sFlashAlpha == 0 && sHoldTimer >= PostFlashHold) {
                Stop(play, player);
                func_80839FFC(player, play);
            }
            break;
    }
}

void StartPendingTransform(PlayState* play, Player* player) {
    sPendingStart = false;
    player->actor.shape.rot.y = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
    player->yaw = player->actor.shape.rot.y;
    TakeCamera(play);
    if (sLightNode == nullptr) {
        Lights_PointNoGlowSetInfo(&sLightInfo, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
        sLightNode = LightContext_InsertLight(play, &play->lightCtx, &sLightInfo);
    }
    TimeControl_Request(TimeControlOwner, TimeControlPriority, 0.0f, true);

    Player_SetupAction(play, player, TransformAction, 0);
    player->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
    player->skelAnime.playSpeed = 1.0f;
    if (sPoseAnim != nullptr) {
        LinkAnimation_Change(play, &player->skelAnime, sPoseAnim, PoseAnimSpeed, 0.0f,
                             Animation_GetLastFrame(sPoseAnim), ANIMMODE_ONCE, -6.0f);
    }
}

void UpdateTransform() {
    if (!sActive || gPlayState == nullptr) {
        return;
    }
    Player* player = GetPlayer();
    if (player == nullptr) {
        return;
    }
    if (sPendingStart) {
        StartPendingTransform(gPlayState, player);
        return;
    }
    if (player->actionFunc == TransformAction) {
        return;
    }
    const bool isFormPending = sPhase != PhaseFlashOut;
    const std::string target = sTargetKey;
    Stop(gPlayState, player);
    if (isFormPending) {
        FormRegistry_ApplyActive(target.empty() ? nullptr : target.c_str());
    }
}

void DrawFlash() {
    if (!sActive || sFlashAlpha <= 0 || gPlayState == nullptr) {
        return;
    }
    PlayState* play = gPlayState;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_44Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 220, 220, 220, (u8)sFlashAlpha);
    gDPFillRectangle(POLY_XLU_DISP++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    CLOSE_DISPS(play->state.gfxCtx);
}

void DrawMask(PlayState* play, Player* player) {
    if (sMaskDl == nullptr || player->skelAnime.curFrame < MaskFaceFrame) {
        return;
    }
    const char* dl = sMaskDl;
    if (sMaskClimaxDl != nullptr && (sMaskClimax || player->skelAnime.curFrame >= MaskClimaxFrame)) {
        sMaskClimax = true;
        dl = sMaskClimaxDl;
    }
    Color_RGB8 tunic = Player_GetTunicColor(player->currentTunic);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Scale(1.0f, 1.0f - sMaskSquashY, 1.0f - sMaskSquashZ, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    gDPSetEnvColor(POLY_OPA_DISP++, tunic.r, tunic.g, tunic.b, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

bool HasFaceGlowDl() {
    static const bool hasDl = ResourceMgr_FileExists(FaceGlowDl);
    return hasDl;
}

void DrawFaceGlow(PlayState* play) {
    if (sFaceGlowAlpha == 0 || !HasFaceGlowDl()) {
        return;
    }
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, FaceGlowSegment,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, (u32)(-(s32)frames), 0, 16, 16, 1, frames, frames * 2,
                                           16, 16));
    Matrix_Push();
    Matrix_Translate(sFaceGlowOffset.x, sFaceGlowOffset.y, sFaceGlowOffset.z, MTXMODE_APPLY);
    Matrix_Scale(sFaceGlowScale, sFaceGlowScale, sFaceGlowScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 255, (u8)sFaceGlowAlpha);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)FaceGlowDl);

    CLOSE_DISPS(play->state.gfxCtx);
}

void DrawHead(PlayState* play, Player* player, int32_t limbIndex) {
    if (!sActive || !sIsMaskCutscene || limbIndex != PLAYER_LIMB_HEAD) {
        return;
    }
    DrawMask(play, player);
    DrawFaceGlow(play);
}

} // namespace

void FormTransform_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(DrawFlash);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerPostLimbDraw>(DrawHead);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(UpdateTransform);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(Forget);
}

bool FormTransform_IsActive(void) {
    return sActive;
}

bool FormTransform_Begin(const SOHFormDefinition* from, const SOHFormDefinition* to) {
    PlayState* play = gPlayState;
    Player* player = GetPlayer();
    if (play == nullptr || player == nullptr || sActive || IsInstantTransform()) {
        return false;
    }

    sFromForm = from != nullptr;
    const SOHFormDefinition* wearer = sFromForm ? from : to;
    if (wearer == nullptr) {
        return false;
    }

    sActive = true;
    sPhase = PhasePose;
    sTargetKey = to == nullptr ? "" : to->key;
    sIsMaskCutscene = wearer->transformMask != nullptr;
    sMaskDl = sFromForm ? nullptr : wearer->transformMask;
    sMaskClimaxDl = sFromForm ? nullptr : wearer->transformMaskClimax;
    sPoseAnim = (sFromForm && wearer->transformOffAnim != nullptr) ? wearer->transformOffAnim : wearer->transformAnim;
    sVoiceSfx = wearer->transformVoiceSfx;
    const Vec3f& glowOffset = wearer->transformGlowOffset;
    const bool hasGlowOffset = glowOffset.x != 0.0f || glowOffset.y != 0.0f || glowOffset.z != 0.0f;
    sFaceGlowOffset = (sFromForm && hasGlowOffset) ? glowOffset : LinkFaceGlowOffset;
    sFaceGlowScale = (sFromForm && wearer->transformGlowScale > 0.0f) ? wearer->transformGlowScale : 1.0f;
    sFaceGlowAlpha = 0;
    sMaskClimax = false;
    sPendingStart = true;
    sFrame = 0;
    sFlashAlpha = 0;
    sHoldTimer = 0;
    sEnvStage = 0.0f;
    sLightStage = 0.0f;
    sMaskSquashZ = 0.0f;
    sMaskSquashY = 0.0f;
    sCamState = sFromForm ? TakeOffStart : PutOnStart;
    RemoveDistortion();
    return true;
}
