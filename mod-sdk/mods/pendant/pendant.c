/**
 * Pendant of Memories: Mortal Draw and aerial ground pound on B; hop, roll, hop and a timed B after
 * Navi's cue launch the parry arc. A on the pendant's pause cell toggles the moves.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "regs.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define PENDANT_KEY "nei.equip.pendant"
#define MORTAL_DRAW_RANGE 200.0f
#define MORTAL_DRAW_MIN_RANGE 20.0f
#define MORTAL_DRAW_HITSTOP 10
#define MORTAL_DRAW_RECOVERY 15
#define MORTAL_DRAW_SLASH_FRAMES 12
#define MORTAL_DRAW_DAMAGE 0xFF
#define GPOUND_STALL_FRAMES 4
#define GPOUND_FALL_VELOCITY (-18.0f)
#define GPOUND_FALL_GRAVITY (-2.5f)
#define GPOUND_LANDING_FRAMES 25
#define GPOUND_SHOCKWAVE_FRAMES 6
#define GPOUND_BOUNCE_VELOCITY 8.0f
#define GPOUND_TIMEOUT 60
#define PARRY_ARC_FRAMES 22
#define PARRY_ARC_HEIGHT 80.0f
#define PARRY_LAND_DIST 120.0f
#define PARRY_LANDING_FRAMES 12
#define PARRY_TIMING_FRAMES 6
#define PARRY_HOP_WINDOW 60
#define DEFAULT_GRAVITY (-1.2f)
#define BGCHECK_ON_GROUND 0x0001
#define BGCHECK_TOUCHING_CEILING 0x0008

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconPendantOfMemoriesTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__item_name_static/gItemNamePendantOfMemoriesENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL";
static const ALIGN_ASSET(2) char sGroundPoundAnim[] = "__OTR__misc/link_animetion/nei_pendant_ground_pound";

int Player_IsZTargeting(Player* player);
void func_80837948(PlayState* play, Player* player, s32 meleeWeaponAnim);
void Rumble_Request(f32 distSq, u8 strength, u8 duration, u8 decreaseRate);
void func_80839FFC(Player* player, PlayState* play);

typedef enum {
    PENDANT_IDLE,
    PENDANT_DRAW_SLASH,
    PENDANT_DRAW_HITSTOP,
    PENDANT_DRAW_RECOVERY,
    PENDANT_GPOUND_STALL,
    PENDANT_GPOUND_FALLING,
    PENDANT_GPOUND_LANDING,
    PENDANT_PARRY_ARC,
    PENDANT_PARRY_LANDING,
} PendantState;

static const SOHModApi* sApi;

static ColliderCylinder sStrikeCollider;
static ColliderJntSph sParryCollider;
static ColliderJntSphElement sParryElement;
static bool sParryReady;
static bool sSecondHopPending;
static bool sComboLandingPending;
static bool sComboWasAirborne;
static f32 sSavedGravity;
static ColliderJntSphElementInit sParryElementInit = {
    { ELEMTYPE_UNK2, { DMG_JUMP_SLASH, 0x00, 0x08 }, { 0, 0, 0 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 0, { { 0, 0, 0 }, 36 }, 100 },
};
static ColliderJntSphInit sParryInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_JNTSPH }, 1, &sParryElementInit,
};

static ColliderCylinderInit sStrikeColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_SLASH, 0x00, 0x08 }, { 0, 0, 0 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_NONE },
    { 80, 80, 0, { 0, 0, 0 } },
};

static struct {
    PendantState state;
    s16 timer;
    Actor* target;
    Vec3f arcStart;
    Vec3f arcEnd;
    bool hasParryHit;
    bool isColliderReady;
    u8 hops;
    s16 hopTimer;
    bool isLeapArmed;
    bool wasHopping;
} sPendant;

static bool IsActive(void) {
    return CustomEquipRegistry_IsOwned(PENDANT_KEY) && CustomEquipRegistry_IsToggleOn(PENDANT_KEY) &&
           !CustomEquipRegistry_IsWorn("nei.equip.trident") && !CustomEquipRegistry_IsWorn("nei.equip.byrna");
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_DAMAGED |
                                   PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER | PLAYER_STATE1_TALKING)) != 0;
}

static bool IsBPressed(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySfxGeneral(sfxId, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void ChangeAnim(PlayState* play, Player* player, const char* anim, f32 playSpeed, f32 startFrame,
                       f32 endFrame, u8 mode, f32 morph) {
    LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)anim, playSpeed, startFrame, endFrame, mode,
                         morph);
}

static f32 GetLastFrame(const char* anim) {
    return Animation_GetLastFrame((void*)anim);
}

// The actor lists are the only proof a remembered pointer still names a live actor.
static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* listed = play->actorCtx.actorLists[category].head; listed != NULL; listed = listed->next) {
            if (listed == actor) {
                return actor->update != NULL;
            }
        }
    }
    return false;
}

static void ReturnToIdle(void) {
    sPendant.state = PENDANT_IDLE;
    sPendant.timer = 0;
    sPendant.target = NULL;
}

static void ResetMoves(void) {
    sSecondHopPending = false;
    sComboLandingPending = false;
    sComboWasAirborne = false;
    ReturnToIdle();
    sPendant.hops = 0;
    sPendant.hopTimer = 0;
    sPendant.isLeapArmed = false;
    sPendant.wasHopping = false;
}

static void EnsureStrikeCollider(PlayState* play, Player* player) {
    if (sPendant.isColliderReady) {
        return;
    }
    Collider_InitCylinder(play, &sStrikeCollider);
    Collider_SetCylinder(play, &sStrikeCollider, &player->actor, &sStrikeColliderInit);
    sPendant.isColliderReady = true;
}

static void EnsureParryCollider(PlayState* play, Player* player) {
    if (!sParryReady) {
        Collider_InitJntSph(play, &sParryCollider);
        Collider_SetJntSph(play, &sParryCollider, &player->actor, &sParryInit, &sParryElement);
        sParryReady = true;
    }
}

static void StrikeAround(PlayState* play, Player* player, Vec3f* center) {
    sStrikeCollider.dim.pos.x = center->x;
    sStrikeCollider.dim.pos.y = center->y;
    sStrikeCollider.dim.pos.z = center->z;
    sStrikeCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sStrikeCollider.base);
}

static bool ConsumeStrikeHit(void) {
    if (!(sStrikeCollider.base.atFlags & AT_HIT)) {
        return false;
    }
    sStrikeCollider.base.atFlags &= ~AT_HIT;
    return true;
}

static void ShakeCamera(PlayState* play, s16 speed, s16 y, s16 x, s16 zoom, s16 frames) {
    s16 quake = Quake_Add(Play_GetCamera(play, 0), 3);

    Quake_SetSpeed(quake, speed);
    Quake_SetQuakeValues(quake, y, x, zoom, 0);
    Quake_SetCountdown(quake, frames);
}

// ---- Mortal Draw ----

static Actor* FindMortalDrawTarget(Player* player, PlayState* play) {
    Actor* best = NULL;
    f32 closest = MORTAL_DRAW_RANGE;

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        f32 dist = Actor_WorldDistXZToActor(&player->actor, enemy);

        if (dist < closest && dist > MORTAL_DRAW_MIN_RANGE) {
            closest = dist;
            best = enemy;
        }
    }
    return best;
}

static bool CanMortalDraw(Player* player, PlayState* play) {
    return IsBPressed(play) && Player_GetMeleeWeaponHeld(player) == 0 && !Player_IsZTargeting(player) &&
           IsGrounded(player) && player->linearVelocity <= 1.0f;
}

static void StartMortalDraw(Player* player, PlayState* play, Actor* target) {
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    sPendant.state = PENDANT_DRAW_SLASH;
    sPendant.timer = 0;
    sPendant.target = target;
    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    player->linearVelocity = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    // The swing brings its own clip, so the iaijutsu draw goes on after it.
    func_80837948(play, player, PLAYER_MWA_JUMPSLASH_START);
    ChangeAnim(play, player, gPlayerAnim_link_fighter_power_kiru_start, 2.0f, 0.0f,
               GetLastFrame(gPlayerAnim_link_fighter_power_kiru_start), ANIMMODE_ONCE, -3.0f);
    player->meleeWeaponQuads[0].info.toucher.damage = MORTAL_DRAW_DAMAGE;
    player->meleeWeaponQuads[1].info.toucher.damage = MORTAL_DRAW_DAMAGE;
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateMortalDraw(Player* player, PlayState* play) {
    bool hasHit = (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
                  (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);

    sPendant.timer++;
    player->linearVelocity = 0.0f;
    if (hasHit) {
        sPendant.state = PENDANT_DRAW_HITSTOP;
        sPendant.timer = 0;
        ShakeCamera(play, 20000, 8, 0, 0, MORTAL_DRAW_HITSTOP);
        Rumble_Request(0.0f, 180, 14, 100);
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        return;
    }
    if (sPendant.timer > MORTAL_DRAW_SLASH_FRAMES) {
        sPendant.state = PENDANT_DRAW_RECOVERY;
        sPendant.timer = 0;
    }
}

// Link freezes for the hitstop; the enemy's own hit reaction freezes it.
static void UpdateHitstop(Player* player) {
    player->skelAnime.playSpeed = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    if (++sPendant.timer >= MORTAL_DRAW_HITSTOP) {
        sPendant.state = PENDANT_DRAW_RECOVERY;
        sPendant.timer = 0;
        player->skelAnime.playSpeed = 1.0f;
    }
}

// The sword stays drawn afterwards, as in Twilight Princess.
static void UpdateDrawRecovery(Player* player) {
    player->linearVelocity = 0.0f;
    if (++sPendant.timer >= MORTAL_DRAW_RECOVERY) {
        ReturnToIdle();
    }
}

// ---- Ground Pound ----

static bool CanGroundPound(Player* player, PlayState* play) {
    return !IsGrounded(player) && Player_GetMeleeWeaponHeld(player) != 0 && IsBPressed(play);
}

static void HoldStill(Player* player) {
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.gravity = 0.0f;
    player->linearVelocity = 0.0f;
}

static void PlantSwordPose(Player* player, PlayState* play, f32 playSpeed, f32 morph) {
    LinkAnimationHeader* clip = ResourceMgr_LoadPlayerAnimAsHeader(sGroundPoundAnim);
    if (clip == NULL) return;
    LinkAnimation_Change(play, &player->skelAnime, clip, playSpeed, 89.0f,
                         playSpeed < 0.0f ? 0.0f : 89.0f, playSpeed < 0.0f ? ANIMMODE_ONCE : ANIMMODE_LOOP, morph);
}

// MM's return-to-the-past clip is held on frame 89 until the landing starts its reverse playback.
static void StartGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_STALL;
    sPendant.timer = 0;
    sSavedGravity = player->actor.gravity;
    HoldStill(player);
    PlantSwordPose(player, play, 0.0f, -3.0f);
    EnsureStrikeCollider(play, player);
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateGroundPoundStall(Player* player, PlayState* play) {
    HoldStill(player);
    LinkAnimation_Update(play, &player->skelAnime);
    if (++sPendant.timer < GPOUND_STALL_FRAMES) {
        return;
    }
    sPendant.state = PENDANT_GPOUND_FALLING;
    sPendant.timer = 0;
    player->actor.velocity.y = GPOUND_FALL_VELOCITY;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    PlantSwordPose(player, play, 0.0f, 0.0f);
}

static void LandGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_LANDING;
    sPendant.timer = 0;
    StrikeAround(play, player, &player->actor.world.pos);
    Rumble_Request(0.0f, 255, 20, 150);
    ShakeCamera(play, 28000, 14, 2, 100, 16);
    PlaySfx(player, NA_SE_IT_HAMMER_HIT);
    PlantSwordPose(player, play, -1.0f, -3.0f);
    player->actor.gravity = sSavedGravity;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
}

static void UpdateGroundPoundFall(Player* player, PlayState* play) {
    sPendant.timer++;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    player->linearVelocity = 0.0f;
    // One held pose throughout the fall; playback begins only on impact, in reverse.
    PlantSwordPose(player, play, 0.0f, 0.0f);
    LinkAnimation_Update(play, &player->skelAnime);
    // Last frame's hit is read before the collider is registered again: a pogo off whatever it struck.
    if (ConsumeStrikeHit()) {
        player->actor.velocity.y = GPOUND_BOUNCE_VELOCITY;
        player->actor.gravity = sSavedGravity;
        sStrikeCollider.base.atFlags &= ~AT_ON;
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        ReturnToIdle();
        return;
    }
    StrikeAround(play, player, &player->actor.world.pos);
    if (IsGrounded(player)) {
        LandGroundPound(player, play);
    }
    if (sPendant.timer > GPOUND_TIMEOUT) {
        player->actor.gravity = sSavedGravity;
        ReturnToIdle();
    }
}

static void UpdateGroundPoundLanding(Player* player, PlayState* play) {
    player->linearVelocity = 0.0f;
    if (++sPendant.timer < GPOUND_SHOCKWAVE_FRAMES) {
        StrikeAround(play, player, &player->actor.world.pos);
        if (ConsumeStrikeHit()) {
            PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        }
    }
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        player->actor.gravity = sSavedGravity;
        ReturnToIdle();
    }
}

#include "pendant_combo.inc.c"

static void StartMoveFromIdle(Player* player, PlayState* play) {
    Actor* drawTarget;

    TrackSideHops(player);
    if (CanMortalDraw(player, play) && (drawTarget = FindMortalDrawTarget(player, play)) != NULL) {
        StartMortalDraw(player, play, drawTarget);
    } else if (CanGroundPound(player, play)) {
        StartGroundPound(player, play);
    }
}

static void ReleasePendantAction(Player* player);

static void TickPendant(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);
    if (!IsActive() || IsPlayerBusy(player)) {
        ReleasePendantAction(player);
        if (sPendant.state != PENDANT_IDLE) {
            player->cylinder.base.ocFlags1 |= OC1_ON;
            player->skelAnime.playSpeed = 1.0f;
        }
        ResetMoves();
        return;
    }
    if (sSecondHopPending && sPendant.state == PENDANT_IDLE) ConfigureSecondHopAnimation(player);
    switch (sPendant.state) {
        case PENDANT_IDLE:
            StartMoveFromIdle(player, play);
            break;
        case PENDANT_DRAW_SLASH:
            UpdateMortalDraw(player, play);
            break;
        case PENDANT_DRAW_HITSTOP:
            UpdateHitstop(player);
            break;
        case PENDANT_DRAW_RECOVERY:
            UpdateDrawRecovery(player);
            break;
        case PENDANT_GPOUND_STALL:
        case PENDANT_GPOUND_FALLING:
        case PENDANT_GPOUND_LANDING:
        case PENDANT_PARRY_ARC:
        case PENDANT_PARRY_LANDING:
            // These run in place of the native action, before physics, rather than fighting its poses afterward.
            break;
        default:
            ReturnToIdle();
            break;
    }
}

static bool OwnsPendantAction(void) {
    return sPendant.state >= PENDANT_GPOUND_STALL;
}

static void ReleasePendantAction(Player* player) {
    if (!OwnsPendantAction()) return;
    player->actor.gravity = sSavedGravity;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    player->skelAnime.playSpeed = 1.0f;
    if (sParryReady) sParryCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    if (sPendant.isColliderReady) sStrikeCollider.base.atFlags &= ~(AT_ON | AT_HIT);
}

static void DrivePendantAction(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;
    if (play == NULL || !OwnsPendantAction()) return;
    if (!IsActive() || IsPlayerBusy(player)) {
        ReleasePendantAction(player);
        ResetMoves();
        return;
    }
    *should = false;
    SilenceSwordQuads(player);
    switch (sPendant.state) {
        case PENDANT_GPOUND_STALL: UpdateGroundPoundStall(player,play); break;
        case PENDANT_GPOUND_FALLING: UpdateGroundPoundFall(player,play); break;
        case PENDANT_GPOUND_LANDING: UpdateGroundPoundLanding(player,play); break;
        case PENDANT_PARRY_ARC: UpdateParryArc(player,play); break;
        case PENDANT_PARRY_LANDING:
            LinkAnimation_Update(play,&player->skelAnime);
            UpdateParryLanding(player);
            break;
        default: break;
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.9f, 0.9f, 0.9f, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The collider is bound to the player actor, and a new scene brings a new one.
static void ForgetSceneState(int16_t sceneNum) {
    ResetMoves();
    sPendant.isColliderReady = false;
    sParryReady = false;
    sBoundsActor = NULL;
}

static void TurnOff(const char* key) {
    if (gPlayState != NULL) ReleasePendantAction(GET_PLAYER(gPlayState));
    ResetMoves();
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit", "OnPlayerResolveAnimSite",
    "OnPlayerFilterInput", "OnActorUpdate", "OnPlayerPostLimbDraw", "OnActorResolveMotionScale" };

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = PENDANT_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rPendant of Memories&%w%y\xA0%w near an enemy with your sword&"
                           "sheathed draws and strikes. %y\xA0%w in&mid air slams down.^"
                           "Locked on: %y\x9F%w, %y\x9F%w, %y\x9F%w.&Hop, roll, hop. At the Hey and&"
                           "sword glow, tap %y\xA0%w. No bosses.^"
                           "The timing window is brief.&%y\x9F%w on this slot turns it off.";
    definition.getItemText = "You got the %rPendant of&Memories%w! A keepsake from&Kafei, proof of his promise.^"
                             "It remembers the moves of&heroes from other tales, and&they answer to %y\xA0%w.";
    definition.slot = SOH_EQUIP_SLOT_UPGRADE;
    definition.page = 0;
    definition.row = 1;
    definition.column = 0;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.toggles = 1;
    definition.onUnequip = TurnOff;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickPendant);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveAnimSite, ResolveComboHop);
    SOH_REGISTER_HOOK(sApi, OnActorResolveMotionScale, ExtendComboHopTravel);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, ReadParryInput);
    SOH_REGISTER_HOOK(sApi, OnActorUpdate, SampleTargetBounds);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawParryCue);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DrivePendantAction);
}
