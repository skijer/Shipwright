/**
 * Pendant of Memories: three borrowed moves on B. From a sheathed stand near an enemy, a Mortal Draw that kills
 * in one cut; in mid air, a ground pound that bounces off what it hits; locked on after three side hops, a
 * parry leap over the enemy. A on its cell turns the moves off.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
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
#define PARRY_HOPS_TO_ARM 3
#define PARRY_HOP_WINDOW 60
#define DEFAULT_GRAVITY (-1.2f)
#define BGCHECK_ON_GROUND 0x0001
#define BGCHECK_TOUCHING_CEILING 0x0008

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconPendantOfMemoriesTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__item_name_static/gItemNamePendantOfMemoriesENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL";

int Player_IsZTargeting(Player* player);
void func_80837948(PlayState* play, Player* player, s32 meleeWeaponAnim);
void func_800AA000(f32 distSq, u8 strength, u8 duration, u8 decreaseRate);

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
    return CustomEquipRegistry_IsOwned(PENDANT_KEY) && CustomEquipRegistry_IsToggleOn(PENDANT_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

static bool IsBPressed(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySoundGeneral(sfxId, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
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
        func_800AA000(0.0f, 180, 14, 100);
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
    return (player->stateFlags3 & PLAYER_STATE3_MIDAIR) && Player_GetMeleeWeaponHeld(player) != 0 && IsBPressed(play);
}

static void HoldStill(Player* player) {
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.gravity = 0.0f;
    player->linearVelocity = 0.0f;
}

static void PlantSwordPose(Player* player, PlayState* play, f32 playSpeed, f32 morph) {
    ChangeAnim(play, player, gPlayerAnim_002840, playSpeed, GetLastFrame(gPlayerAnim_002840), 0.0f, ANIMMODE_ONCE,
               morph);
}

// The sword is raised with the pedestal plant played backwards and frozen on its last frame.
static void StartGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_STALL;
    sPendant.timer = 0;
    HoldStill(player);
    PlantSwordPose(player, play, 0.0f, -3.0f);
    EnsureStrikeCollider(play, player);
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateGroundPoundStall(Player* player, PlayState* play) {
    HoldStill(player);
    if (++sPendant.timer < GPOUND_STALL_FRAMES) {
        return;
    }
    sPendant.state = PENDANT_GPOUND_FALLING;
    sPendant.timer = 0;
    player->actor.velocity.y = GPOUND_FALL_VELOCITY;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    // The swing sets the sword's damage and takes over the animation, so the slam pose goes on after it.
    func_80837948(play, player, PLAYER_MWA_JUMPSLASH_START);
    PlantSwordPose(player, play, -2.0f, 0.0f);
}

static void LandGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_LANDING;
    sPendant.timer = 0;
    StrikeAround(play, player, &player->actor.world.pos);
    func_800AA000(0.0f, 255, 20, 150);
    ShakeCamera(play, 28000, 14, 2, 100, 16);
    PlaySfx(player, NA_SE_IT_HAMMER_HIT);
    ChangeAnim(play, player, gPlayerAnim_link_normal_landing, 1.0f, 0.0f, GetLastFrame(gPlayerAnim_link_normal_landing),
               ANIMMODE_ONCE, -6.0f);
    player->actor.gravity = -1.0f;
}

static void UpdateGroundPoundFall(Player* player, PlayState* play) {
    sPendant.timer++;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    player->linearVelocity = 0.0f;
    // The melee action keeps putting its own clip back.
    if (player->skelAnime.animation != (LinkAnimationHeader*)gPlayerAnim_002840) {
        PlantSwordPose(player, play, -2.0f, 0.0f);
    }
    // Last frame's hit is read before the collider is registered again: a pogo off whatever it struck.
    if (ConsumeStrikeHit()) {
        player->actor.velocity.y = GPOUND_BOUNCE_VELOCITY;
        player->actor.gravity = DEFAULT_GRAVITY;
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
        player->actor.gravity = DEFAULT_GRAVITY;
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
    if (sPendant.timer > GPOUND_LANDING_FRAMES) {
        ReturnToIdle();
    }
}

// ---- Parry Leap ----

static void TrackSideHops(Player* player) {
    bool isHopping = (player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0;

    if (!Player_IsZTargeting(player)) {
        sPendant.hops = 0;
        sPendant.isLeapArmed = false;
        sPendant.wasHopping = false;
        return;
    }
    if (isHopping && !sPendant.wasHopping) {
        sPendant.hops++;
        sPendant.hopTimer = 0;
        if (sPendant.hops >= PARRY_HOPS_TO_ARM && !sPendant.isLeapArmed) {
            sPendant.isLeapArmed = true;
            Sfx_PlaySfxCentered(NA_SE_SY_LOCK_ON);
        }
    }
    sPendant.wasHopping = isHopping;
    if (++sPendant.hopTimer > PARRY_HOP_WINDOW) {
        sPendant.hops = 0;
        sPendant.isLeapArmed = false;
    }
}

static bool CanParryLeap(Player* player, PlayState* play) {
    if (!sPendant.isLeapArmed || !IsBPressed(play)) {
        return false;
    }
    sPendant.isLeapArmed = false;
    sPendant.hops = 0;
    return player->focusActor != NULL;
}

static void PlayRollLoop(Player* player, PlayState* play) {
    ChangeAnim(play, player, gPlayerAnim_link_normal_landing_roll, 3.0f, 0.0f,
               GetLastFrame(gPlayerAnim_link_normal_landing_roll), ANIMMODE_LOOP, -3.0f);
}

// A negative invincibility timer is intangibility without the red flash.
static void StartParryLeap(Player* player, PlayState* play) {
    Actor* target = player->focusActor;
    f32 dx = target->world.pos.x - player->actor.world.pos.x;
    f32 dz = target->world.pos.z - player->actor.world.pos.z;
    f32 distXZ = MAX(sqrtf(SQ(dx) + SQ(dz)), 1.0f);
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    sPendant.state = PENDANT_PARRY_ARC;
    sPendant.timer = 0;
    sPendant.target = target;
    sPendant.hasParryHit = false;
    sPendant.arcStart = player->actor.world.pos;
    sPendant.arcEnd.x = target->world.pos.x + (dx / distXZ) * PARRY_LAND_DIST;
    sPendant.arcEnd.y = sPendant.arcStart.y;
    sPendant.arcEnd.z = target->world.pos.z + (dz / distXZ) * PARRY_LAND_DIST;
    HoldStill(player);
    player->actor.bgCheckFlags &= ~BGCHECK_ON_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->invincibilityTimer = -(PARRY_ARC_FRAMES + 5);
    func_80837948(play, player, PLAYER_MWA_JUMPSLASH_START);
    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    PlayRollLoop(player, play);
    EnsureStrikeCollider(play, player);
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void SilenceSwordQuads(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
    player->meleeWeaponState = 0;
}

// One hit only, on the enemy itself: the melee system would otherwise land a second one on the way down.
static void StrikeParryTarget(Player* player, PlayState* play) {
    if (sPendant.hasParryHit) {
        SilenceSwordQuads(player);
        return;
    }
    StrikeAround(play, player, &sPendant.target->world.pos);
    if (!ConsumeStrikeHit()) {
        return;
    }
    sPendant.hasParryHit = true;
    sStrikeCollider.base.atFlags &= ~AT_ON;
    SilenceSwordQuads(player);
    PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
}

static void EndParryArc(Player* player, PlayState* play) {
    sPendant.state = PENDANT_PARRY_LANDING;
    sPendant.timer = 0;
    player->actor.gravity = DEFAULT_GRAVITY;
}

static void UpdateParryArc(Player* player, PlayState* play) {
    f32 t;

    if (!IsActorAlive(play, sPendant.target)) {
        EndParryArc(player, play);
        return;
    }
    t = MIN((f32)++sPendant.timer / PARRY_ARC_FRAMES, 1.0f);
    if (player->skelAnime.animation != (LinkAnimationHeader*)gPlayerAnim_link_normal_landing_roll) {
        PlayRollLoop(player, play);
    }
    player->actor.world.pos.x = sPendant.arcStart.x + (sPendant.arcEnd.x - sPendant.arcStart.x) * t;
    player->actor.world.pos.z = sPendant.arcStart.z + (sPendant.arcEnd.z - sPendant.arcStart.z) * t;
    player->actor.world.pos.y = sPendant.arcStart.y + (sPendant.arcEnd.y - sPendant.arcStart.y) * t +
                                PARRY_ARC_HEIGHT * 4.0f * t * (1.0f - t);
    HoldStill(player);
    player->cylinder.base.ocFlags1 &= ~OC1_ON;
    StrikeParryTarget(player, play);
    player->actor.shape.rot.y = Actor_WorldYawTowardActor(&player->actor, sPendant.target);
    player->yaw = player->actor.shape.rot.y;

    if (sPendant.timer >= PARRY_ARC_FRAMES) {
        EndParryArc(player, play);
        ChangeAnim(play, player, gPlayerAnim_link_normal_landing, 1.5f, 0.0f,
                   GetLastFrame(gPlayerAnim_link_normal_landing), ANIMMODE_ONCE, -3.0f);
        PlaySfx(player, NA_SE_PL_WALK_GROUND);
    } else if (player->actor.bgCheckFlags & BGCHECK_TOUCHING_CEILING) {
        EndParryArc(player, play);
    }
}

static void UpdateParryLanding(Player* player) {
    if (!IsGrounded(player) && ++sPendant.timer <= PARRY_LANDING_FRAMES) {
        return;
    }
    player->linearVelocity = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    ReturnToIdle();
}

static void StartMoveFromIdle(Player* player, PlayState* play) {
    Actor* drawTarget;

    TrackSideHops(player);
    if (CanMortalDraw(player, play) && (drawTarget = FindMortalDrawTarget(player, play)) != NULL) {
        StartMortalDraw(player, play, drawTarget);
    } else if (CanGroundPound(player, play)) {
        StartGroundPound(player, play);
    } else if (CanParryLeap(player, play)) {
        StartParryLeap(player, play);
    }
}

static void TickPendant(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);
    if (!IsActive() || IsPlayerBusy(player)) {
        if (sPendant.state != PENDANT_IDLE) {
            player->cylinder.base.ocFlags1 |= OC1_ON;
            player->skelAnime.playSpeed = 1.0f;
        }
        ResetMoves();
        return;
    }
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
            UpdateGroundPoundStall(player, play);
            break;
        case PENDANT_GPOUND_FALLING:
            UpdateGroundPoundFall(player, play);
            break;
        case PENDANT_GPOUND_LANDING:
            UpdateGroundPoundLanding(player, play);
            break;
        case PENDANT_PARRY_ARC:
            UpdateParryArc(player, play);
            break;
        case PENDANT_PARRY_LANDING:
            UpdateParryLanding(player);
            break;
        default:
            ReturnToIdle();
            break;
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
}

static void TurnOff(const char* key) {
    ResetMoves();
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit" };

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
    definition.pauseText = "%rPendant of Memories&%w%y\xA0%w near an enemy with your sword sheathed draws and "
                           "strikes. %y\xA0%w in mid air slams down. Locked on, three side hops then %y\xA0%w leap "
                           "over the foe. %y\x9F%w turns it off.";
    definition.getItemText = "You got the %rPendant of Memories%w!&A keepsake from Kafei, proof of his promise. "
                             "It remembers the moves of heroes from other tales, and they answer to %y\xA0%w.";
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
}
