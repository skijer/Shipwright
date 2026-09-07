/**
 * kafei_landmine.cpp — SW97's bombchu (z64proto/sw97), a landmine rather than a homing mouse.
 *
 * Not doable from an actor hook: EnBomChu_Move steps the chu by rewriting world.pos and re-arms
 * speedXZ every frame, so braking it from outside is inert. This replaces that stepping instead.
 */

#include <libultraship/bridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
// OPEN_DISPS redeclares these inline at every call site, taking C++ linkage in a .cpp and leaving
// the linker hunting a mangled name. Force the C symbol at file scope. (Same as season_scene.cpp.)
extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"

u8 MmForm_IsKafeiFormActive(void);
}

#define KAFEI_LANDMINE_TIMER 500
#define KAFEI_LANDMINE_REARM 200
#define KAFEI_LANDMINE_FALL 8.0f
// The prototype's own model, converted by tools/sw97_landmine_to_o2r.py.
#define LANDMINE_SPHERE "__OTR__objects/object_landmine/gLandMineSphereDL"
#define LANDMINE_PLANE "__OTR__objects/object_landmine/gLandMinePlaneDL"
#define LANDMINE_SPIKES "__OTR__objects/object_landmine/gLandMineSphere004DL"

typedef struct {
    const char* dl;
    f32 scale;
    f32 pivot;
    u8 yawFromCamera;
    u8 pitchFromCamera;
} KafeiLandminePiece;

// Each piece is aimed differently in the prototype: the dome turns with the camera in yaw only, the
// band is a full billboard hung off a pivot 5 units up, and the spikes just follow the actor. The
// scales are the prototype's too, applied on top of the chu's own 0.01.
static const KafeiLandminePiece sLandminePieces[] = {
    { LANDMINE_SPHERE, 95.0f, 0.0f, true, false },
    { LANDMINE_PLANE, 105.0f, 5.0f, true, true },
    { LANDMINE_SPIKES, 1.05f, 0.0f, false, false },
};

static void KafeiLandmine_DrawPiece(PlayState* play, Actor* actor, const KafeiLandminePiece* piece, u8 fade) {
    Camera* camera = GET_ACTIVE_CAM(play);
    s16 yaw = piece->yawFromCamera ? Camera_GetCamDirYaw(camera) : actor->shape.rot.y;
    s16 pitch = piece->pitchFromCamera ? -Camera_GetCamDirPitch(camera) : actor->shape.rot.x;
    f32 scale = piece->scale;

    Matrix_Push();
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y + piece->pivot, actor->world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(pitch), MTXMODE_APPLY);
    Matrix_RotateZ(BINANG_TO_RAD(actor->shape.rot.z), MTXMODE_APPLY);
    Matrix_Translate(0.0f, -piece->pivot, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(actor->scale.x * scale, actor->scale.y * scale, actor->scale.z * scale, MTXMODE_APPLY);

    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_NOPUSH | G_MTX_LOAD);
    // Per piece, because the plane's own list forces PRIM black and would otherwise carry that into
    // whatever is drawn after it.
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, fade, fade, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)piece->dl);
    CLOSE_DISPS(play->state.gfxCtx);

    Matrix_Pop();
}

// colorIntensity is the actor's own fuse blink, reused so the mine still warns before it goes off.
extern "C" void KafeiLandmine_Draw(PlayState* play, Actor* actor, f32 colorIntensity) {
    u8 fade = (u8)(255.0f - (colorIntensity * 170.0f));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);

    for (size_t i = 0; i < ARRAY_COUNT(sLandminePieces); i++) {
        KafeiLandmine_DrawPiece(play, actor, &sLandminePieces[i], fade);
    }
}

extern "C" u8 KafeiLandmine_Settle(EnBomChu* chu, PlayState* play) {
    if (!MmForm_IsKafeiFormActive()) {
        return 0;
    }

    // Actor_MoveXYZ derives every velocity component from speedXZ and world.rot and applies no
    // gravity, so zeroing this both pins the mine down and leaves the drop below to do the work.
    chu->actor.speedXZ = 0.0f;
    chu->visualJitter = 0.0f;

    // Nothing reads the fuse for detonation any more, but the actor still blinks off it — so it is
    // recycled well above the range where that blink turns into the red "about to blow" warning.
    if (chu->timer < KAFEI_LANDMINE_REARM) {
        chu->timer = KAFEI_LANDMINE_TIMER;
    }

    Actor_UpdateBgCheckInfo(play, &chu->actor, 5.0f, 5.0f, 0.0f, 0x1F);

    // A constant drop, not real gravity: EnBomChu has no field to accumulate a fall speed in.
    if (chu->actor.world.pos.y > chu->actor.floorHeight) {
        chu->actor.world.pos.y -= KAFEI_LANDMINE_FALL;
        if (chu->actor.world.pos.y < chu->actor.floorHeight) {
            chu->actor.world.pos.y = chu->actor.floorHeight;
        }
    }

    return 1;
}

// Safe because OnActorInit runs after the actor's own Init (z_actor.c:1283), outliving its 120.
static void KafeiLandmine_OnInit(void* actorPtr) {
    if (!MmForm_IsKafeiFormActive()) {
        return;
    }
    ((EnBomChu*)actorPtr)->timer = KAFEI_LANDMINE_TIMER;
}

static void RegisterKafeiLandmineHooks() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_BOM_CHU, true, KafeiLandmine_OnInit);
}

static RegisterShipInitFunc initFuncKafeiLandmine(RegisterKafeiLandmineHooks, {});
