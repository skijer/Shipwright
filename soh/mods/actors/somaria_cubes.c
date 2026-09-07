/**
 * Cane summon system — see somaria_cubes.h. Skijer's NEI
 *
 * The statue keeps the original En_Lightbox hijack (that actor does exist in OoT,
 * unlike in MM) and the original elegy-shell draw. What changed with the Dual Cane
 * rework is that it is no longer a liftable cube: an Elegy of Emptiness statue is
 * a fixture, so Actor_OfferCarry is gone and with it the held/thrown states. It
 * still presses switches — including the heavy Bg_Bdan_Switch ones that vanilla
 * OoT needs Ruto to stand on.
 *
 * Block and Platform are REAL vanilla actors (Obj_Oshihiki, Obj_Lift), so they
 * behave exactly like the ones the game already places — minus the parts that make
 * no sense for a summon, which are neutralised at their spawn sites.
 */

#include "somaria_cubes.h"
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Lightbox/z_en_lightbox.h"
#include "overlays/actors/ovl_Bg_Bdan_Switch/z_bg_bdan_switch.h"
#include "transformation_masks/transformation_masks.h"
#include "objects/object_d_lift/object_d_lift.h" // gCollapsingPlatformDL — the summoned platform

// z_scene.c — request an object into the scene's bank at runtime. Not declared in
// functions.h, so it is forward-declared here (same as the ExtInv/OTR helpers).
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void SomariaStatue_Update(Actor* thisx, PlayState* play);
static void SomariaStatue_Draw(Actor* thisx, PlayState* play);
static void SomariaStatue_DestroyFunc(Actor* thisx, PlayState* play);

static ActorFunc sOriginalDestroy = NULL;

// ============================================================================
// POOL
// ============================================================================

typedef struct {
    Actor* actor;
    u8 kind;
    u16 seq; // spawn order, so "the oldest of this kind" is answerable
} CaneSummonSlot;

static CaneSummonSlot sSummons[SOMARIA_MAX_CUBES] = { { 0 } };
static u16 sSummonSeq = 0;

static u8 CaneSummon_CapFor(CaneSummonKind kind) {
    switch (kind) {
        case CANE_SUMMON_BLOCK:
            return CANE_MAX_BLOCKS;
        case CANE_SUMMON_PLATFORM:
            return CANE_MAX_PLATFORMS;
        case CANE_SUMMON_STATUE:
        default:
            return CANE_MAX_STATUES;
    }
}

// ============================================================================
// ELEGY SHELL DISPLAY LISTS (indexed by form)
// ============================================================================

static const char* sShellDLists[ELEGY_FORM_MAX] = {
    gElegyShellHumanDL, // ELEGY_FORM_HUMAN
    gElegyShellGoronDL, // ELEGY_FORM_GORON
    gElegyShellZoraDL,  // ELEGY_FORM_ZORA
    gElegyShellDekuDL,  // ELEGY_FORM_DEKU
    gElegyShellHumanDL, // ELEGY_FORM_FD (Fierce Deity uses the human shell)
};

// MM's EnTorch2_Draw sets segment 0x0C to no-op DLists via Scene_SetRenderModeXlu.
// The elegy DLs call gsSPDisplayList(0x0C000000/0x0C000010), which then become
// no-ops. Setting real cull modes here instead would ADD cull bits on top of the
// DLs' own, leaving CULL_BACK+CULL_FRONT both active = nothing drawn.
static Gfx sSegment0xC_Noop[] = {
    gsSPEndDisplayList(), // offset 0x00 (called by 0x0C000000)
    gsSPEndDisplayList(), // offset 0x08
    gsSPEndDisplayList(), // offset 0x10 (called by 0x0C000010)
    gsSPEndDisplayList(), // offset 0x18
};

// ============================================================================
// COLLIDER (AC for hookshot only — no AT, no OC)
// ============================================================================

static ColliderCylinderInit sColliderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_NONE,
    },
    { SOMARIA_CYL_RADIUS, SOMARIA_CYL_HEIGHT, 0, { 0, 0, 0 } },
};

// Actor_Spawn only allocates sizeof(EnLightbox), so the statue CANNOT carry extra
// fields — its collider comes from this static pool instead.
typedef struct {
    ColliderCylinder collider;
    Actor* owner; // NULL = free
    u8 initialized;
} ColliderSlot;

static ColliderSlot sColliderPool[SOMARIA_MAX_COLLIDERS] = { 0 };

static s8 SomariaCube_GetColliderSlot(Actor* actor) {
    for (s8 i = 0; i < SOMARIA_MAX_COLLIDERS; i++) {
        if (sColliderPool[i].owner == actor) {
            return i;
        }
    }
    return -1;
}

static s8 SomariaCube_AllocCollider(PlayState* play, Actor* actor) {
    for (s8 i = 0; i < SOMARIA_MAX_COLLIDERS; i++) {
        if (sColliderPool[i].owner == NULL) {
            if (!sColliderPool[i].initialized) {
                Collider_InitCylinder(play, &sColliderPool[i].collider);
                sColliderPool[i].initialized = 1;
            }
            Collider_SetCylinder(play, &sColliderPool[i].collider, actor, &sColliderInit);
            sColliderPool[i].owner = actor;
            return i;
        }
    }
    // No free slot — reap dead owners. The Free path can be missed when a statue
    // is killed from elsewhere (scene unload, room transition): its slot keeps a
    // dangling owner pointer and would pin the slot forever, so after enough churn
    // allocation would silently fail and new statues would spawn with no collision.
    for (s8 i = 0; i < SOMARIA_MAX_COLLIDERS; i++) {
        if (sColliderPool[i].owner != NULL && sColliderPool[i].owner->update == NULL) {
            sColliderPool[i].owner = actor;
            Collider_SetCylinder(play, &sColliderPool[i].collider, actor, &sColliderInit);
            return i;
        }
    }
    return -1;
}

static void SomariaCube_FreeCollider(PlayState* play, Actor* actor) {
    s8 slot = SomariaCube_GetColliderSlot(actor);
    if (slot >= 0) {
        sColliderPool[slot].owner = NULL; // keep the initialized collider for reuse
    }
}

// ============================================================================
// HELPERS
// ============================================================================

void SomariaCube_PlaySound(Actor* actor, u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &actor->projectedPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

u8 SomariaCube_GetForm(Actor* actor) {
    if (actor == NULL) {
        return ELEGY_FORM_HUMAN;
    }
    s16 form = SOMARIA_GET_FORM(actor);
    if (form < 0 || form >= ELEGY_FORM_MAX) {
        return ELEGY_FORM_HUMAN;
    }
    return (u8)form;
}

u8 SomariaCube_IsSomariaCube(Actor* actor) {
    if (actor == NULL || actor->update == NULL) {
        return 0;
    }
    for (u8 i = 0; i < SOMARIA_MAX_CUBES; i++) {
        if (sSummons[i].actor == actor) {
            return 1;
        }
    }
    return 0;
}

u8 SomariaCube_IsSwitchable(Actor* actor) {
    return SomariaCube_IsSomariaCube(actor);
}

// The elegy shell for the player's current transformation (Fierce Deity puts down
// a human shell, exactly like the vanilla song does).
static u8 SomariaCube_GetCurrentForm(void) {
    if (!TransformMasks_IsTransformed()) {
        return ELEGY_FORM_HUMAN;
    }
    // MM form enum (FD=0, Goron=1, Zora=2, Deku=3, Human=4) -> Elegy form enum.
    switch (MmPlayer_GetForm()) {
        case 1:
            return ELEGY_FORM_GORON;
        case 2:
            return ELEGY_FORM_ZORA;
        case 3:
            return ELEGY_FORM_DEKU;
        case 0:
            return ELEGY_FORM_FD;
        default:
            return ELEGY_FORM_HUMAN;
    }
}

// Vanilla OoT gates YELLOW_HEAVY switches behind an actor heavy enough to hold
// them down (Ruto, a big block). A Somaria statue qualifies — that is exactly the
// puzzle use the Cane of Somaria is for.
static void SomariaCube_TryActivateHeavySwitch(Actor* cube, PlayState* play) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_SWITCH].head;

    while (actor != NULL) {
        if (actor->id == ACTOR_BG_BDAN_SWITCH) {
            u8 switchType = actor->params & 0xFF;

            if (switchType == YELLOW_HEAVY) {
                f32 dx = cube->world.pos.x - actor->world.pos.x;
                f32 dz = cube->world.pos.z - actor->world.pos.z;
                f32 distXZ = sqrtf((dx * dx) + (dz * dz));
                f32 dy = cube->world.pos.y - actor->world.pos.y;

                if (distXZ < 40.0f && dy >= 0.0f && dy < 50.0f) {
                    u8 switchFlag = (actor->params >> 8) & 0x3F;
                    if (!Flags_GetSwitch(play, switchFlag)) {
                        Flags_SetSwitch(play, switchFlag);
                        SomariaCube_PlaySound(cube, NA_SE_EV_FOOT_SWITCH);
                        Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    }
                }
            }
        }
        actor = actor->next;
    }
}

void CaneSummon_CleanupPool(void) {
    for (u8 i = 0; i < SOMARIA_MAX_CUBES; i++) {
        if (sSummons[i].actor != NULL && sSummons[i].actor->update == NULL) {
            sSummons[i].actor = NULL;
        }
    }
}

void CaneSummon_KillAll(PlayState* play) {
    for (u8 i = 0; i < SOMARIA_MAX_CUBES; i++) {
        if (sSummons[i].actor != NULL && sSummons[i].actor->update != NULL) {
            Actor_Kill(sSummons[i].actor);
        }
        sSummons[i].actor = NULL;
    }
    sSummonSeq = 0;
}

// Claim a slot for `kind`. Only that kind's own budget is consulted: going over it
// evicts the oldest summon OF THAT KIND, so stacking statues never costs you the
// block you carefully placed on a switch.
static s8 CaneSummon_TakeSlot(PlayState* play, CaneSummonKind kind) {
    u8 live = 0;
    s8 oldest = -1;
    u16 oldestSeq = 0xFFFF;
    s8 free = -1;

    CaneSummon_CleanupPool();

    for (u8 i = 0; i < SOMARIA_MAX_CUBES; i++) {
        if (sSummons[i].actor == NULL) {
            if (free < 0) {
                free = (s8)i;
            }
            continue;
        }
        if (sSummons[i].kind != (u8)kind) {
            continue;
        }
        live++;
        if (sSummons[i].seq < oldestSeq) {
            oldestSeq = sSummons[i].seq;
            oldest = (s8)i;
        }
    }

    if ((live >= CaneSummon_CapFor(kind)) && (oldest >= 0)) {
        if (sSummons[oldest].actor->update != NULL) {
            SomariaCube_PlaySound(sSummons[oldest].actor, NA_SE_EV_BLOCK_BOUND);
            Actor_Kill(sSummons[oldest].actor);
        }
        sSummons[oldest].actor = NULL;
        return oldest;
    }

    return free; // -1 only if the pool is somehow entirely full of other kinds
}

// ============================================================================
// STATUE (En_Lightbox hijack)
// ============================================================================

static void SomariaStatue_Update(Actor* thisx, PlayState* play) {
    s16 timer = SOMARIA_GET_TIMER(thisx);

    if (timer > 0) {
        SOMARIA_SET_TIMER(thisx, timer - 1);
        timer--;
    }

    if (SOMARIA_GET_STATE(thisx) == SOMARIA_STATUE_SPAWNING) {
        if (thisx->scale.x < SOMARIA_CUBE_SCALE) {
            thisx->scale.x += SOMARIA_CUBE_SCALE / SOMARIA_SPAWN_FRAMES;
            thisx->scale.y = thisx->scale.z = thisx->scale.x;
        }
        if (timer == 0) {
            Actor_SetScale(thisx, SOMARIA_CUBE_SCALE);
            SOMARIA_SET_STATE(thisx, SOMARIA_STATUE_IDLE);
        }
    }

    // A statue is a fixture: it settles onto the floor and stays there. It is
    // deliberately NOT offered for carry (that was the old cube's behaviour).
    // On the way down, lean toward a floor switch within reach. A statue placed near one settles
    // onto it — which is what these are for. Shared with Ultrahand and Stasis; see switch_magnet.c.
    if (!SwitchMagnet_Steer(play, thisx)) {
        Math_StepToF(&thisx->speedXZ, 0.0f, 1.0f);
    }
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 30.0f, 15.0f, 0.0f, 0x1D);
    // ...and once it is directly over one, it drops onto it square.
    SwitchMagnet_SnapOnto(play, thisx, 1);

    if (thisx->bgCheckFlags & BGCHECKFLAG_GROUND) {
        SomariaCube_TryActivateHeavySwitch(thisx, play);
    }

    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 15.0f;

    s8 slot = SomariaCube_GetColliderSlot(thisx);
    if (slot >= 0) {
        Collider_UpdateCylinder(thisx, &sColliderPool[slot].collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &sColliderPool[slot].collider.base);
    }
}

static void SomariaStatue_Draw(Actor* thisx, PlayState* play) {
    if (thisx->scale.x <= 0.001f) {
        return;
    }

    u8 form = SomariaCube_GetForm(thisx);
    if (form >= ELEGY_FORM_MAX) {
        form = ELEGY_FORM_HUMAN;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, sSegment0xC_Noop);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    Gfx_DrawDListOpa(play, (Gfx*)sShellDLists[form]);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void SomariaStatue_DestroyFunc(Actor* thisx, PlayState* play) {
    SomariaCube_FreeCollider(play, thisx);
    if (sOriginalDestroy != NULL) {
        sOriginalDestroy(thisx, play);
    }
}

static Actor* CaneSummon_SpawnStatue(PlayState* play, Vec3f* pos, s16 yaw) {
    Actor* statue = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHTBOX, pos->x, pos->y, pos->z, 0, yaw, 0, 0);

    if (statue == NULL) {
        return NULL;
    }

    EnLightbox* lightbox = (EnLightbox*)statue;

    if (sOriginalDestroy == NULL) {
        sOriginalDestroy = statue->destroy;
    }

    statue->update = SomariaStatue_Update;
    statue->draw = SomariaStatue_Draw;
    statue->destroy = SomariaStatue_DestroyFunc;

    // Drop En_Lightbox's own DynaPoly — the statue uses a cylinder collider.
    if (lightbox->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, lightbox->dyna.bgId);
        lightbox->dyna.bgId = BGACTOR_NEG_ONE;
    }

    SomariaCube_AllocCollider(play, statue);

    statue->gravity = SOMARIA_GRAVITY;
    statue->minVelocityY = SOMARIA_MIN_VEL_Y;
    statue->flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    statue->flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    statue->flags |= ACTOR_FLAG_SWITCHHOOKABLE;
    statue->shape.shadowDraw = NULL;
    statue->shape.shadowScale = 0.0f;
    statue->room = -1;

    SOMARIA_SET_FORM(statue, SomariaCube_GetCurrentForm());
    Actor_SetScale(statue, 0.0f);
    SOMARIA_SET_STATE(statue, SOMARIA_STATUE_SPAWNING);
    SOMARIA_SET_TIMER(statue, SOMARIA_SPAWN_FRAMES);

    return statue;
}

// ============================================================================
// PLACEMENT VALIDITY
// ============================================================================

static f32 CaneSummon_Radius(CaneSummonKind kind) {
    switch (kind) {
        case CANE_SUMMON_BLOCK:
            return CANE_BLOCK_HALF_WIDTH;
        case CANE_SUMMON_PLATFORM:
            return CANE_PLATFORM_RADIUS;
        case CANE_SUMMON_STATUE:
        default:
            return 25.0f;
    }
}

static f32 CaneSummon_Height(CaneSummonKind kind) {
    switch (kind) {
        case CANE_SUMMON_BLOCK:
            return CANE_BLOCK_HEIGHT;
        case CANE_SUMMON_PLATFORM:
            return CANE_PLATFORM_HEIGHT;
        case CANE_SUMMON_STATUE:
        default:
            return 60.0f;
    }
}

u8 CaneSummon_PlacementValid(PlayState* play, CaneSummonKind kind, Vec3f* pos) {
    Player* player = GET_PLAYER(play);
    f32 radius = CaneSummon_Radius(kind);
    f32 height = CaneSummon_Height(kind);

    if (player == NULL) {
        return 0;
    }

    // The platform goes ANYWHERE (user-locked): no ground needed, no clearance
    // needed, and geometry in the way is not a reason to refuse. Its whole purpose
    // is reaching places the level does not offer a floor for, and half-embedding
    // it in a wall to make a ledge is a legitimate use rather than a mistake.
    if (kind == CANE_SUMMON_PLATFORM) {
        return 1;
    }

    // Never place inside Link himself — the block's dynapoly would appear around
    // him and shove him through the floor.
    f32 dx = pos->x - player->actor.world.pos.x;
    f32 dz = pos->z - player->actor.world.pos.z;
    f32 dy = pos->y - player->actor.world.pos.y;
    if (((dx * dx) + (dz * dz)) < ((radius + 22.0f) * (radius + 22.0f)) && (dy > -height) && (dy < height)) {
        return 0;
    }

    for (u8 i = 0; i < SOMARIA_MAX_CUBES; i++) {
        Actor* other = sSummons[i].actor;
        if (other == NULL || other->update == NULL) {
            continue;
        }
        f32 odx = pos->x - other->world.pos.x;
        f32 odz = pos->z - other->world.pos.z;
        f32 ody = pos->y - other->world.pos.y;
        f32 minDist = radius + CaneSummon_Radius((CaneSummonKind)sSummons[i].kind);
        if (((odx * odx) + (odz * odz)) < (minDist * minDist) && (ody > -height) && (ody < height)) {
            return 0;
        }
    }

    // Reject placement through geometry (across a fence, inside a pillar). Open
    // air IS legal for the platform — a floating platform is the whole point — so
    // only the block demands ground, which the caller resolves by raycast.
    Vec3f from = player->actor.world.pos;
    Vec3f to = *pos;
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;

    from.y += 40.0f;
    to.y += (height * 0.5f);
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
        return 0;
    }

    return 1;
}

// ============================================================================
// PLACEMENT PREVIEW
// ============================================================================

// A self-contained unit cube (+-1 on every axis) so the preview needs no asset
// from any object bank. Scaled per summon kind at draw time.
static Vtx sPreviewCubeVtx[] = {
    VTX(-1, -1, -1, 0, 0, 0, 0, 0, 255), VTX(1, -1, -1, 0, 0, 0, 0, 0, 255), VTX(1, -1, 1, 0, 0, 0, 0, 0, 255),
    VTX(-1, -1, 1, 0, 0, 0, 0, 0, 255),  VTX(-1, 1, -1, 0, 0, 0, 0, 0, 255), VTX(1, 1, -1, 0, 0, 0, 0, 0, 255),
    VTX(1, 1, 1, 0, 0, 0, 0, 0, 255),    VTX(-1, 1, 1, 0, 0, 0, 0, 0, 255),
};

static Gfx sPreviewCubeDL[] = {
    gsSPVertex(sPreviewCubeVtx, 8, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0), // bottom
    gsSP2Triangles(4, 6, 5, 0, 4, 7, 6, 0), // top
    gsSP2Triangles(0, 5, 1, 0, 0, 4, 5, 0), // -Z
    gsSP2Triangles(1, 6, 2, 0, 1, 5, 6, 0), // +X
    gsSP2Triangles(2, 7, 3, 0, 2, 6, 7, 0), // +Z
    gsSP2Triangles(3, 4, 0, 0, 3, 7, 4, 0), // -X
    gsSPEndDisplayList(),
};

void CaneSummon_DrawPreview(PlayState* play, CaneSummonKind kind, Vec3f* pos, s16 yaw, u8 valid) {
    f32 radius = CaneSummon_Radius(kind);
    f32 height = CaneSummon_Height(kind);
    // Gentle breathing pulse so the ghost never reads as a real placed object.
    f32 pulse = 0.94f + (0.06f * Math_SinS((s16)(play->gameplayFrames * 1500)));

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    Matrix_Translate(pos->x, pos->y + (height * 0.5f), pos->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(yaw), MTXMODE_APPLY);
    Matrix_Scale(radius * pulse, (height * 0.5f) * pulse, radius * pulse, MTXMODE_APPLY);

    // Components spelled out: MSVC hands a multi-value #define to a function-like
    // macro as ONE argument, so gDPSetPrimColor would not expand.
    if (valid) {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 90, 170, 255, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 20, 60, 180, 110);
    } else {
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 70, 70, 110);
        gDPSetEnvColor(POLY_XLU_DISP++, 150, 0, 0, 110);
    }
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, sPreviewCubeDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ============================================================================
// BLOCK / PLATFORM (real vanilla actors)
// ============================================================================

// The real pushable block. Its object (gameplay_dangeon_keep) is only resident in
// dungeons, so outside one we request it and let the next press land.
//
// ObjOshihiki_Draw picks its env colour from a per-scene table, and for any scene
// that is NOT one of the eight vanilla block dungeons it falls through to
// gDPSetEnvColor(mREG(13), mREG(14), mREG(15), 255). Those debug registers are 0
// everywhere else, so a summoned block outside a dungeon renders pure black and
// reads as "untextured" (the texture itself is fine — params & 0xF == 0 selects
// gPushBlockSilverTex). Wrapping the draw to fill those three registers first is
// the minimal fix: it feeds the vanilla path exactly the value it is asking for,
// and inside a real dungeon the scene branch wins so this is inert.
static ActorFunc sBlockOriginalDraw = NULL;

static void CaneSummon_BlockDraw(Actor* thisx, PlayState* play) {
    mREG(13) = 150; // a warm stone grey with a red cast: a Somaria construct,
    mREG(14) = 120; // not a dungeon block
    mREG(15) = 120;

    if (sBlockOriginalDraw != NULL) {
        sBlockOriginalDraw(thisx, play);
    }
}

static Actor* CaneSummon_SpawnBlock(PlayState* play, Vec3f* pos, s16 yaw) {
    if (Object_GetIndex(&play->objectCtx, OBJECT_GAMEPLAY_DANGEON_KEEP) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_GAMEPLAY_DANGEON_KEEP);
        return NULL; // not resident yet this frame
    }

    Actor* block =
        Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_OSHIHIKI, pos->x, pos->y, pos->z, 0, yaw, 0, CANE_BLOCK_PARAMS);
    if (block == NULL) {
        return NULL;
    }

    if (sBlockOriginalDraw == NULL) {
        sBlockOriginalDraw = block->draw;
    }
    block->draw = CaneSummon_BlockDraw;
    block->room = -1;
    return block;
}

// The floating platform. Two actors were tried before this one: Obj_Ice_Poly has
// no dynapoly at all (it is the ice that encases a frozen enemy, so it could never
// be stood on), and Bg_Ice_Shelter's red ice worked but is a lumpy ice chunk, not
// a platform. Obj_Lift IS a platform: a square slab with a real texture
// (gCollapsingPlatformDL) over gCollapsingPlatformCol dynapoly.
//
// Two things have to be neutralised for it to serve as a SUMMON:
//
//   1. It collapses. ObjLift_Wait watches for the player standing on it and then
//      shakes and drops the slab. Replacing `update` with a no-op freezes it as a
//      permanent platform — the dynapoly stays registered as long as the actor is
//      alive, and the bg system reads its transform straight off the actor, so
//      nothing else has to run.
//
//   2. ObjLift_Init kills itself when the switch flag in (params >> 2) & 0x3F is
//      already set, and there is no scene switch behind a summoned platform. That
//      kill is survivable here: Init registers the dynapoly BEFORE the flag check,
//      and Actor_Kill only NULLs update/draw — the actor is not freed until the
//      update loop sees a NULL update. Since we overwrite update and draw on the
//      very next line, the platform lives. The scale is re-applied for the same
//      reason: Actor_SetScale runs AFTER the kill point in Init, so on that path
//      it never ran.
#define CANE_PLATFORM_SCALE 0.1f // ObjLift sScales[0]

static void CaneSummon_PlatformUpdate(Actor* thisx, PlayState* play) {
    // Deliberately empty: a summoned platform neither collapses nor moves.
}

static void CaneSummon_PlatformDraw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    // Keep the slab's own texture, push it red so it still reads as a Somaria
    // construct. Components spelled out — a multi-value #define does not survive
    // MSVC's function-like macro expansion.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 210, 70, 70, 255);
    CLOSE_DISPS(play->state.gfxCtx);

    Gfx_DrawDListOpa(play, gCollapsingPlatformDL);
}

static Actor* CaneSummon_SpawnPlatform(PlayState* play, Vec3f* pos, s16 yaw) {
    if (Object_GetIndex(&play->objectCtx, OBJECT_D_LIFT) < 0) {
        Object_Spawn(&play->objectCtx, OBJECT_D_LIFT);
        return NULL; // not resident yet this frame
    }

    Actor* plat = Actor_Spawn(&play->actorCtx, play, ACTOR_OBJ_LIFT, pos->x, pos->y, pos->z, 0, yaw, 0, 0);

    if (plat == NULL) {
        return NULL;
    }

    plat->update = CaneSummon_PlatformUpdate;
    plat->draw = CaneSummon_PlatformDraw;
    Actor_SetScale(plat, CANE_PLATFORM_SCALE);
    plat->room = -1;
    return plat;
}

// ============================================================================
// SPAWN
// ============================================================================

Actor* CaneSummon_Spawn(PlayState* play, CaneSummonKind kind, Vec3f* pos, s16 yaw) {
    Actor* summon = NULL;

    switch (kind) {
        case CANE_SUMMON_STATUE:
            summon = CaneSummon_SpawnStatue(play, pos, yaw);
            break;
        case CANE_SUMMON_BLOCK:
            summon = CaneSummon_SpawnBlock(play, pos, yaw);
            break;
        case CANE_SUMMON_PLATFORM:
            summon = CaneSummon_SpawnPlatform(play, pos, yaw);
            break;
        default:
            return NULL;
    }

    if (summon == NULL) {
        return NULL;
    }

    // Only take a pool slot once the actor really exists — the "object not
    // resident" path above returns NULL and must not evict a live summon.
    s8 slot = CaneSummon_TakeSlot(play, kind);
    if (slot < 0) {
        Actor_Kill(summon);
        return NULL;
    }
    sSummons[slot].actor = summon;
    sSummons[slot].kind = (u8)kind;
    sSummons[slot].seq = sSummonSeq++;

    // NA_SE_PL_MAGIC_SOUL_NORMAL was the sustained soul-magic LOOP, so it started
    // and never stopped. _BALL is the one-shot burst of the same magic.
    SomariaCube_PlaySound(summon, NA_SE_PL_MAGIC_SOUL_BALL);
    return summon;
}

// ============================================================================
// REMOTE STATUES (Harpoon multiplayer)
// ============================================================================
// A remote statue is a pure visual: its transform comes from the network snapshot
// every frame, so it needs no physics and no update logic beyond keeping its
// collider (which is what lets a local player hookshot another player's statue)
// in sync. It deliberately does NOT enter the summon pool — the pool is the LOCAL
// player's three-summon budget, and remote statues must not evict local ones.

static void SomariaStatue_UpdateRemote(Actor* thisx, PlayState* play) {
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 30.0f;

    s8 slot = SomariaCube_GetColliderSlot(thisx);
    if (slot >= 0) {
        Collider_UpdateCylinder(thisx, &sColliderPool[slot].collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &sColliderPool[slot].collider.base);
    }
}

static ActorFunc sSomariaStatueUpdateRemote = SomariaStatue_UpdateRemote;

u8 SomariaCube_IsRemoteCube(Actor* actor) {
    if (actor == NULL || actor->update == NULL) {
        return 0;
    }
    return (actor->update == sSomariaStatueUpdateRemote);
}

Actor* SomariaCube_SpawnRemote(PlayState* play, Vec3f* pos, s16 yaw, u8 form) {
    Actor* cube = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHTBOX, pos->x, pos->y, pos->z, 0, yaw, 0, 0);

    if (cube == NULL) {
        return NULL;
    }

    EnLightbox* lightbox = (EnLightbox*)cube;

    if (sOriginalDestroy == NULL) {
        sOriginalDestroy = cube->destroy;
    }

    cube->update = SomariaStatue_UpdateRemote;
    cube->draw = SomariaStatue_Draw;
    cube->destroy = SomariaStatue_DestroyFunc;

    if (lightbox->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, lightbox->dyna.bgId);
        lightbox->dyna.bgId = BGACTOR_NEG_ONE;
    }

    SomariaCube_AllocCollider(play, cube);

    cube->gravity = SOMARIA_GRAVITY;
    cube->minVelocityY = SOMARIA_MIN_VEL_Y;
    cube->flags |= ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER;
    cube->flags |= ACTOR_FLAG_SWITCHHOOKABLE;
    cube->shape.shadowDraw = NULL;
    cube->shape.shadowScale = 0.0f;
    cube->room = -1;

    if (form >= ELEGY_FORM_MAX) {
        form = ELEGY_FORM_HUMAN;
    }
    SOMARIA_SET_FORM(cube, form);
    SOMARIA_SET_STATE(cube, SOMARIA_STATUE_IDLE);
    Actor_SetScale(cube, SOMARIA_CUBE_SCALE);

    return cube;
}

void SomariaCube_UpdateRemotePos(Actor* cube, Vec3f* pos, f32 scale, s16 rotY) {
    if (cube == NULL) {
        return;
    }
    Math_Vec3f_Copy(&cube->world.pos, pos);
    cube->shape.rot.y = rotY;
    Actor_SetScale(cube, scale);
}
