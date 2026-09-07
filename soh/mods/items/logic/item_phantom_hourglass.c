/**
 * item_phantom_hourglass.c — Phantom Hourglass (Skijer's NEI): Recall, as in Tears of the Kingdom.
 * C raises it: time stops, first-person aim, whatever the reticle rests on drains to grey and shows
 * its path. C again: time flows, the world drains to grey instead, and the target walks its path
 * backwards, pose included (rewind_helper). C, B or any other button lets go.
 * Kept 1:1 with the 2ship copy; the per-game lines are the macros marked "OoT:".
 */

#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/extended_inventory.h"
#include "../helpers/equip_helper.h"
#include "../helpers/camera_helper.h"
#include "../helpers/timestop_helper.h"
#include "../helpers/rewind_helper.h"
#include "../helpers/target_select_helper.h"

// Its own cues, mixed on the audio thread. Included here (not globbed) so it shares this
// translation unit and needs no header — the arrangement the cane and the slate use for theirs.
#include "hourglass_sfx.inc.c"

extern u16 ExtButton_GetItem(s32 btn);
// cane_pacci.c, unity-included ahead of this file. Ultrahand drives a body's transform from its
// carry every frame, so the two must never be pointed at the same actor.
extern Actor* Pacci_GetUltrahandHeld(void);

#define HOURGLASS_BUTTON_ITEM(btn) ExtButton_GetItem(btn)        // OoT: flat store, MM's is per-form
#define HOURGLASS_LAST_BUTTON 7                                  // OoT: the ext store spans the D-pad (MM: 3)
#define HOURGLASS_UPPER_SKEL(player) (&(player)->upperSkelAnime) // OoT: MM names it skelAnimeUpper
#define HOURGLASS_ACTIVE_CAM(play) ((play)->cameraPtrs[(play)->activeCamera]) // OoT: MM names it activeCamId
#define HOURGLASS_AT_DAMAGE(c) ((c).info.toucher.damage) // OoT: MM spells this elem.atDmgInfo.damage

#define HOURGLASS_MAGIC_ACTIVATION 4
#define HOURGLASS_DRAIN_INTERVAL 10 // frames per HOURGLASS_DRAIN_COST — the Permafrost rate
#define HOURGLASS_DRAIN_COST 1
#define HOURGLASS_MAX_FRAMES REWIND_FRAMES
#define HOURGLASS_MIN_HISTORY 10
#define HOURGLASS_CAST_POSE_FRAMES 18
#define HOURGLASS_RIBBON_STEP 4 // recorded frames per ribbon quad
#define HOURGLASS_RIBBON_MAX_QUADS (REWIND_FRAMES / HOURGLASS_RIBBON_STEP)
#define HOURGLASS_RIBBON_HALF_WIDTH 6.0f
// A body retracing its path is as solid as one thrown: past this much ground per tick it hits for
// a Master Sword's worth. Below it the recall is a nudge and passes through.
#define HOURGLASS_IMPACT_SPEED 8.0f
#define HOURGLASS_IMPACT_DAMAGE 1
// Close enough to pick a block you have just set down. The switch hook's 30 was tuned for a shot
// that has to travel; the glass reaches for things at your feet.
#define HOURGLASS_MIN_DIST 8.0f
// Re-applied every frame of the self-rewind, so its exact length only has to outlast one frame.
#define HOURGLASS_SELF_IFRAMES 20

// Everything ITEM_BLOCK_STATE1 covers except the cutscene flag. A cutscene is exactly when a
// temple elevator descends or a king steps aside, and being able to rewind that is the point.
#define HOURGLASS_BLOCK_STATE1 (ITEM_BLOCK_STATE1 & ~PLAYER_STATE1_IN_CUTSCENE)

typedef enum {
    HOURGLASS_STATE_IDLE,
    HOURGLASS_STATE_AIMING,
    HOURGLASS_STATE_RECALLING,
} HourglassState;

static const u16 sHourglassButtonMasks[8] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                              BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static HourglassState sHourglassState = HOURGLASS_STATE_IDLE;
static Actor* sHourglassOffer = NULL;  // under the reticle while aiming
static Actor* sHourglassTarget = NULL; // being recalled
static s16 sHourglassFrames = 0;
static s16 sHourglassCastTimer = 0;
static s8 sHourglassPrevInvinc = 0;
static u8 sHourglassSelfRewinding = 0;
static Vtx sHourglassRibbonVtx[HOURGLASS_RIBBON_MAX_QUADS * 4];

// The recalled body's own strike. AT only — it deals hits, it does not take them; the body is
// frozen and its own colliders are still submitted by the freeze pass. Modelled on the Stasis
// launch collider, dmgFlags and all. OoT: MM spells these COL_MATERIAL_NONE / ELEM_MATERIAL_UNK0 /
// ATELEM_ON | ATELEM_SFX_NORMAL.
static ColliderCylinderInit sHourglassColliderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 40, 60, 0, { 0, 0, 0 } },
};

static ColliderCylinder sHourglassCollider;
static u8 sHourglassColliderReady = 0;

static u16 Hourglass_EquippedButtonMask(void) {
    u16 mask = 0;

    for (s32 btn = 1; btn <= HOURGLASS_LAST_BUTTON; btn++) {
        if (HOURGLASS_BUTTON_ITEM(btn) == EXT_ITEM_PHANTOM_HOURGLASS) {
            mask |= sHourglassButtonMasks[btn];
        }
    }
    return mask;
}

/**
 * The glass is a pocket watch, not a weapon: it comes out whenever Link is actually free.
 * Deliberately NOT ItemInput_IsBlocked — that one also refuses while a shield is up, a blade is
 * mid-swing, or the held item is being put away, and the last of those is what made the glass
 * unopenable right after any other item had been in Link's hand.
 */
static u8 Hourglass_IsLinkBusy(Player* player) {
    return (player->stateFlags1 & HOURGLASS_BLOCK_STATE1) != 0;
}

static s32 Hourglass_IsRecallable(Actor* actor) {
    return (actor != Pacci_GetUltrahandHeld()) && Rewind_HasHistory(actor, HOURGLASS_MIN_HISTORY);
}

static Actor* Hourglass_ScanTarget(PlayState* play, s16 aimYaw) {
    s32 numCats;
    const u8* cats = Rewind_TrackedCats(&numCats);

    return TargetSelect_ScanCatsFromYaw(play, cats, numCats, Hourglass_IsRecallable, REWIND_TRACK_RANGE,
                                        HOURGLASS_MIN_DIST, TARGETSEL_DEFAULT_CONE, aimYaw);
}

static Actor* Hourglass_PathActor(void) {
    switch (sHourglassState) {
        case HOURGLASS_STATE_AIMING:
            return sHourglassOffer;
        case HOURGLASS_STATE_RECALLING:
            return sHourglassTarget;
        default:
            return NULL;
    }
}

static void Hourglass_PlayError(Player* player) {
    Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void Hourglass_SpawnSand(PlayState* play, Actor* target) {
    Vec3f still = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 255, 200, 80, 255 };
    Color_RGBA8 envColor = { 180, 110, 20, 255 };

    for (s32 i = 0; i < 2; i++) {
        Vec3f pos;

        pos.x = target->world.pos.x + Rand_CenteredFloat(30.0f);
        pos.y = target->world.pos.y + 10.0f + Rand_ZeroFloat(30.0f);
        pos.z = target->world.pos.z + Rand_CenteredFloat(30.0f);
        EffectSsKiraKira_SpawnFocused(play, &pos, &still, &still, &primColor, &envColor, 500, 12);
    }
}

/**
 * Arm the body's strike for this frame, if it covered enough ground to be worth one.
 * @param moved how far the scrub just carried it
 */
static void Hourglass_TickImpact(PlayState* play, f32 moved) {
    if (moved < HOURGLASS_IMPACT_SPEED) {
        return;
    }
    if (!sHourglassColliderReady) {
        Collider_InitCylinder(play, &sHourglassCollider);
        sHourglassColliderReady = 1;
    }
    Collider_SetCylinder(play, &sHourglassCollider, sHourglassTarget, &sHourglassColliderInit);
    HOURGLASS_AT_DAMAGE(sHourglassCollider) = HOURGLASS_IMPACT_DAMAGE;
    Collider_UpdateCylinder(sHourglassTarget, &sHourglassCollider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sHourglassCollider.base);
}

static void Hourglass_CastPose(PlayState* play, Player* player) {
    LinkAnimation_PlayOnce(play, HOURGLASS_UPPER_SKEL(player), &gPlayerAnim_link_hook_shot_ready);
    sHourglassCastTimer = HOURGLASS_CAST_POSE_FRAMES;
}

static void Hourglass_TickCastPose(PlayState* play, Player* player) {
    if (sHourglassCastTimer <= 0) {
        return;
    }
    sHourglassCastTimer--;
    LinkAnimation_Update(play, HOURGLASS_UPPER_SKEL(player));
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

/**
 * The reach is the hookshot's ready pose, but nothing is in that hand.
 *
 * Read from the DRAW (ItemEquip_HoldsEmptyHand), never written into the Player: the hand's DL table
 * and its type are chosen together there, and poking the type alone leaves them out of step — a
 * shield table indexed with a non-shield type resolves a NULL display list and takes Link's draw
 * down with it.
 */
u8 Hourglass_WantsEmptyHand(void) {
    return sHourglassState != HOURGLASS_STATE_IDLE;
}

/** Leave the self-rewind, handing the aim camera back. Idempotent. */
static void Hourglass_EndSelfRewind(PlayState* play, Player* player, u8 backToAim) {
    if (!sHourglassSelfRewinding) {
        return;
    }
    Rewind_End(&player->actor, 0);
    sHourglassSelfRewinding = 0;
    HourglassSfx_StopLoop();
    // Re-based too, or the aim's damage watch reads letting go as a fresh hit.
    player->invincibilityTimer = 0;
    sHourglassPrevInvinc = 0;
    if (backToAim) {
        FirstPerson_Init(player, play);
    }
}

static void Hourglass_Release(Player* player) {
    if (sHourglassState != HOURGLASS_STATE_RECALLING) {
        return;
    }
    Rewind_End(sHourglassTarget, 0);
    HourglassSfx_StopLoop();
    HourglassSfx_PlayCue(HOURGLASS_CUE_EXIT);
    Rumble_Request(200.0f, 100, 15, 40);
    sHourglassTarget = NULL;
    sHourglassFrames = 0;
    sHourglassCastTimer = 0;
    sHourglassState = HOURGLASS_STATE_IDLE;
}

static void Hourglass_Stow(PlayState* play, Player* player) {
    if (sHourglassState == HOURGLASS_STATE_RECALLING) {
        Hourglass_Release(player);
        return;
    }
    if (sHourglassState != HOURGLASS_STATE_AIMING) {
        return;
    }
    Hourglass_EndSelfRewind(play, player, 0);
    FirstPerson_Exit(player, play);
    TimeCtl_Release(TIMECTL_OWNER_HOURGLASS);
    sHourglassOffer = NULL;
    sHourglassState = HOURGLASS_STATE_IDLE;
    HourglassSfx_PlayCue(HOURGLASS_CUE_EXIT);
}

static void Hourglass_Raise(PlayState* play, Player* player) {
    sHourglassState = HOURGLASS_STATE_AIMING;
    sHourglassOffer = NULL;
    sHourglassPrevInvinc = player->invincibilityTimer;
    // NO LinkAnimation_PlayLoop here. Every item that loops the upper skeleton owns a real
    // PlayerItemAction, so the engine takes the upper body back when that action changes; an EXT
    // item never has one, and the loop would run for the rest of the session — arm stuck out,
    // sword undrawable. First person poses Link to aim on its own, which is all this needs.
    FirstPerson_Init(player, play);
    TimeCtl_Request(TIMECTL_OWNER_HOURGLASS, 0.0f, 1);
    HourglassSfx_PlayCue(HOURGLASS_CUE_ACTIVATE);
}

static void Hourglass_Begin(PlayState* play, Player* player, Actor* target) {
    if (!ItemMagic_HasEnough(play, HOURGLASS_MAGIC_ACTIVATION) || !Rewind_Begin(target)) {
        Hourglass_PlayError(player);
        return;
    }
    ItemMagic_Consume(play, HOURGLASS_MAGIC_ACTIVATION);
    FirstPerson_Exit(player, play);
    // Time flows again for everyone but the target, which Rewind_Scrub keeps held on its own.
    TimeCtl_Release(TIMECTL_OWNER_HOURGLASS);
    sHourglassOffer = NULL;
    sHourglassTarget = target;
    sHourglassFrames = 0;
    sHourglassState = HOURGLASS_STATE_RECALLING;
    Hourglass_CastPose(play, player);
    // The pick is announced, then the bed comes up under it and holds for the whole rewind.
    HourglassSfx_PlayCue(HOURGLASS_CUE_TARGET);
    HourglassSfx_StartLoop();
    Rumble_Request(300.0f, 150, 20, 60);
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

static u8 Hourglass_ShouldDrain(void) {
    return (sHourglassFrames % HOURGLASS_DRAIN_INTERVAL) == 0;
}

static void Hourglass_TickRecall(PlayState* play, Player* player, u16 btnMask) {
    Vec3f before;

    Rewind_Validate(play);
    if (!Rewind_IsScrubbing(sHourglassTarget) || Hourglass_IsLinkBusy(player) ||
        ItemInput_CheckDamage(player, &sHourglassPrevInvinc)) {
        Hourglass_Release(player);
        return;
    }
    Hourglass_TickCastPose(play, player);

    sHourglassFrames++;
    if (Hourglass_ShouldDrain()) {
        if (!ItemMagic_HasEnough(play, HOURGLASS_DRAIN_COST)) {
            Hourglass_Release(player);
            return;
        }
        ItemMagic_Consume(play, HOURGLASS_DRAIN_COST);
    }
    before = sHourglassTarget->world.pos;
    if ((sHourglassFrames >= HOURGLASS_MAX_FRAMES) || !Rewind_Scrub(sHourglassTarget, -1)) {
        Hourglass_Release(player);
        return;
    }
    // Its recorded velocity is not the speed it is travelling NOW — the scrub is what moves it.
    Hourglass_TickImpact(play, Math_Vec3f_DistXYZ(&before, &sHourglassTarget->world.pos));
    Hourglass_SpawnSand(play, sHourglassTarget);

    if (play->state.input[0].press.button & btnMask) {
        Hourglass_Release(player);
    }
}

/**
 * Hold R and Link walks his own path backwards, watched from third person — the aim camera is
 * handed back for the duration, because the point is to see it happen.
 * @return non-zero while it owns the frame.
 */
static u8 Hourglass_TickSelfRewind(PlayState* play, Player* player) {
    Actor* link = &player->actor;

    if (!(play->state.input[0].cur.button & BTN_R)) {
        Hourglass_EndSelfRewind(play, player, 1);
        return 0;
    }
    if (!sHourglassSelfRewinding) {
        if (!ItemMagic_HasEnough(play, HOURGLASS_MAGIC_ACTIVATION) || !Rewind_Begin(link)) {
            return 0; // nowhere to go back to; R stays the shield
        }
        ItemMagic_Consume(play, HOURGLASS_MAGIC_ACTIVATION);
        sHourglassSelfRewinding = 1;
        sHourglassFrames = 0;
        sHourglassOffer = NULL;
        FirstPerson_Exit(player, play);
        HourglassSfx_StartLoop();
    }

    sHourglassFrames++;
    if (Hourglass_ShouldDrain()) {
        if (!ItemMagic_HasEnough(play, HOURGLASS_DRAIN_COST)) {
            Hourglass_EndSelfRewind(play, player, 1);
            return 0;
        }
        ItemMagic_Consume(play, HOURGLASS_DRAIN_COST);
    }
    // Negative is the engine's invulnerable-without-the-red-flash, as the Pendant's parry uses.
    player->invincibilityTimer = -HOURGLASS_SELF_IFRAMES;
    // The recorded path owns where he goes: his own walking would otherwise be added on top of it.
    Player_ZeroSpeedXZ(player);
    Rewind_Scrub(link, -1);
    Hourglass_SpawnSand(play, link);
    return 1;
}

static void Hourglass_TickAim(PlayState* play, Player* player, u16 btnMask) {
    Input* input = &play->state.input[0];

    if (Hourglass_IsLinkBusy(player) || ItemInput_CheckDamage(player, &sHourglassPrevInvinc)) {
        Hourglass_Stow(play, player);
        return;
    }
    if (Hourglass_TickSelfRewind(play, player)) {
        return; // R is ours while it is down, camera included
    }
    FirstPerson_Update(player, play);
    sHourglassOffer = Hourglass_ScanTarget(play, FirstPerson_GetAimYaw(player));

    if ((input->press.button & BTN_B) || ItemInput_CheckOtherButtons(btnMask, input)) {
        Hourglass_Stow(play, player);
        return;
    }
    if (!(input->press.button & btnMask)) {
        return;
    }
    if (sHourglassOffer == NULL) {
        Hourglass_PlayError(player);
        return;
    }
    Hourglass_Begin(play, player, sHourglassOffer);
}

void Hourglass_TickInput(PlayState* play, Player* player) {
    u16 btnMask = Hourglass_EquippedButtonMask();

    // Unconditional: this is also what tells the mixer gameplay is still running, so a paused game
    // silences the bed by simply not getting here.
    HourglassSfx_Tick();

    // History must exist BEFORE the press, so the recorder runs whenever the hourglass sits on a button.
    Rewind_SetEnabled(btnMask != 0);
    if (btnMask == 0) {
        Hourglass_Stow(play, player);
        return;
    }

    switch (sHourglassState) {
        case HOURGLASS_STATE_RECALLING:
            Hourglass_TickRecall(play, player, btnMask);
            break;
        case HOURGLASS_STATE_AIMING:
            Hourglass_TickAim(play, player, btnMask);
            break;
        case HOURGLASS_STATE_IDLE:
            if ((play->state.input[0].press.button & btnMask) && !Hourglass_IsLinkBusy(player)) {
                Hourglass_Raise(play, player);
            }
            break;
        default:
            sHourglassState = HOURGLASS_STATE_IDLE;
            break;
    }
}

// ---------------------------------------------------------------------------
// Draw: the grey world (dispatched from Actor_Draw / Room_Draw) and the path ribbon
// ---------------------------------------------------------------------------

/** NULL asks about the scene itself. Aiming greys only the offer; recalling greys all but Link and the target. */
u8 Hourglass_ShouldDrawGray(Actor* actor) {
    // Link is the one being pulled back, so he is the one that keeps his colour.
    if (sHourglassSelfRewinding) {
        return (actor == NULL) || (actor->category != ACTORCAT_PLAYER);
    }

    switch (sHourglassState) {
        case HOURGLASS_STATE_AIMING:
            return (actor != NULL) && (actor == sHourglassOffer);
        case HOURGLASS_STATE_RECALLING:
            return (actor == NULL) || ((actor->category != ACTORCAT_PLAYER) && (actor != sHourglassTarget));
        default:
            return 0;
    }
}

void Hourglass_PushGray(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 255, 255, 255);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    gSPGrayscale(POLY_XLU_DISP++, true);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Hourglass_PopGray(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Hourglass_SetRibbonVtx(Vtx* v, f32 x, f32 y, f32 z) {
    v->v.ob[0] = (s16)x;
    v->v.ob[1] = (s16)y;
    v->v.ob[2] = (s16)z;
    v->v.flag = 0;
    v->v.tc[0] = 0;
    v->v.tc[1] = 0;
    v->v.cn[0] = 255;
    v->v.cn[1] = 255;
    v->v.cn[2] = 255;
    v->v.cn[3] = 255;
}

// One quad per HOURGLASS_RIBBON_STEP recorded frames, from where the recall stands to the oldest
// frame, each turned to face the camera so the ribbon reads from any angle and follows the path in 3D.
static s32 Hourglass_BuildRibbon(PlayState* play, Actor* target) {
    Vec3f eye = HOURGLASS_ACTIVE_CAM(play)->eye;
    s32 back = Rewind_GetPathStart(target);
    s32 quads = 0;
    Vec3f a;

    if (!Rewind_GetPathPos(target, back, &a)) {
        return 0;
    }
    while (quads < HOURGLASS_RIBBON_MAX_QUADS) {
        Vec3f b;
        Vec3f dir;
        Vec3f toCam;
        Vec3f side;
        f32 len;
        Vtx* v;

        back += HOURGLASS_RIBBON_STEP;
        if (!Rewind_GetPathPos(target, back, &b)) {
            break;
        }
        dir.x = b.x - a.x;
        dir.y = b.y - a.y;
        dir.z = b.z - a.z;
        toCam.x = eye.x - a.x;
        toCam.y = eye.y - a.y;
        toCam.z = eye.z - a.z;
        side.x = (dir.y * toCam.z) - (dir.z * toCam.y);
        side.y = (dir.z * toCam.x) - (dir.x * toCam.z);
        side.z = (dir.x * toCam.y) - (dir.y * toCam.x);
        len = sqrtf(SQ(side.x) + SQ(side.y) + SQ(side.z));
        if (len < 0.001f) {
            a = b;
            continue;
        }
        side.x *= HOURGLASS_RIBBON_HALF_WIDTH / len;
        side.y *= HOURGLASS_RIBBON_HALF_WIDTH / len;
        side.z *= HOURGLASS_RIBBON_HALF_WIDTH / len;

        v = &sHourglassRibbonVtx[quads * 4];
        Hourglass_SetRibbonVtx(&v[0], a.x + side.x, a.y + side.y, a.z + side.z);
        Hourglass_SetRibbonVtx(&v[1], a.x - side.x, a.y - side.y, a.z - side.z);
        Hourglass_SetRibbonVtx(&v[2], b.x + side.x, b.y + side.y, b.z + side.z);
        Hourglass_SetRibbonVtx(&v[3], b.x - side.x, b.y - side.y, b.z - side.z);
        quads++;
        a = b;
    }
    return quads;
}

void Hourglass_Draw(PlayState* play) {
    Actor* target = Hourglass_PathActor();
    s32 quads;

    if (target == NULL) {
        return;
    }
    quads = Hourglass_BuildRibbon(play, target);
    if (quads == 0) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_25);
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH | G_LIGHTING);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 220, 40, 110);
    for (s32 i = 0; i < quads; i++) {
        gSPVertex(POLY_XLU_DISP++, &sHourglassRibbonVtx[i * 4], 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
