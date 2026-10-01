/**
 * Champion's Tunic: a sidehop or backflip past an incoming attack while locked on slows the world and offers a
 * flurry rush; swinging takes it. Aiming anything in mid air slows the world and holds Link up: bullet time.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_gi_clothes/object_gi_clothes.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define CHAMPION_TUNIC_KEY "nei.equip.champion_tunic"
#define TIME_OWNER CHAMPION_TUNIC_KEY
// The lowest claim on world speed: any hard stop wins over bullet time.
#define TIME_PRIORITY 1
#define SLOW_FACTOR 0.33f
#define OFFER_FRAMES 45
#define RUSH_FRAMES 100
#define CONNECT_FRAMES 45
#define HITS_ONE_HAND 7
#define HITS_TWO_HAND 4
#define DODGE_RANGE 140.0f
#define DODGE_COOLDOWN 20
#define TELEPORT_DIST 65.0f
#define MAX_TELEPORT 600.0f
#define ATTACK_SNAPSHOT_MAX 24
#define TARGET_COLLIDER_MAX 4
#define BULLET_FLOAT 1.15f
#define SCREEN_FLASH_FRAMES 5
#define TINT_OFFER_ALPHA 200
#define TINT_RUSH_ALPHA 120
#define TINT_SETTLED_ALPHA 50
#define TINT_BULLET_ALPHA 30
#define BGCHECK_ON_GROUND 0x0001

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconChampionsTunicTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gChampionsTunicNameTex";

int Player_IsZTargeting(Player* player);

typedef enum {
    CHAMPION_IDLE,
    CHAMPION_OFFER,
    CHAMPION_RUSH,
    CHAMPION_BULLET_TIME,
} ChampionState;

static const SOHModApi* sApi;

static struct {
    ChampionState state;
    s16 timer;
    s16 connectTimer;
    s16 cooldown;
    s16 flashTimer;
    u8 hits;
    s8 prevInvincibility;
    bool wasHitting;
    Actor* target;
    Collider* targetColliders[TARGET_COLLIDER_MAX];
    s32 targetColliderCount;
    Vec3f attackPos[ATTACK_SNAPSHOT_MAX];
    s32 attackCount;
} sChampion;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(CHAMPION_TUNIC_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECK_ON_GROUND) != 0;
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) != 0;
}

static bool IsAimableAction(s8 itemAction) {
    return (itemAction >= PLAYER_IA_BOW && itemAction <= PLAYER_IA_LONGSHOT) || itemAction == PLAYER_IA_BOOMERANG;
}

// heldItemAction lags itemAction while the airborne hand-off runs; either side keeps the permission alive.
static bool HoldsAimableItem(Player* player) {
    return IsAimableAction(player->heldItemAction) || IsAimableAction(player->itemAction);
}

static bool IsAiming(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_READY_TO_FIRE)) != 0;
}

// A dead actor's memory is freed, so its pointer is trusted only while it is still in the actor lists.
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

static Actor* GetLockedEnemy(Player* player) {
    Actor* target = player->focusActor;

    if (target == NULL || target->update == NULL) {
        return NULL;
    }
    if (target->category != ACTORCAT_ENEMY && target->category != ACTORCAT_BOSS) {
        return NULL;
    }
    return target;
}

static void SetScreenTint(PlayState* play, u8 alpha) {
    play->envCtx.fillScreen = alpha != 0;
    play->envCtx.screenFillColor[0] = 6;
    play->envCtx.screenFillColor[1] = 24;
    play->envCtx.screenFillColor[2] = 66;
    play->envCtx.screenFillColor[3] = alpha;
}

static void SlowWorld(bool isSlow) {
    if (!SOH_MOD_API_HAS(sApi, RequestTimeControl)) {
        return;
    }
    if (isSlow) {
        sApi->RequestTimeControl(TIME_OWNER, TIME_PRIORITY, SLOW_FACTOR, false);
    } else {
        sApi->ReleaseTimeControl(TIME_OWNER);
    }
}

static bool GetColliderCenter(Collider* collider, Vec3f* out) {
    switch (collider->shape) {
        case COLSHAPE_QUAD: {
            ColliderQuad* quad = (ColliderQuad*)collider;

            out->x = (quad->dim.quad[0].x + quad->dim.quad[1].x + quad->dim.quad[2].x + quad->dim.quad[3].x) * 0.25f;
            out->y = (quad->dim.quad[0].y + quad->dim.quad[1].y + quad->dim.quad[2].y + quad->dim.quad[3].y) * 0.25f;
            out->z = (quad->dim.quad[0].z + quad->dim.quad[1].z + quad->dim.quad[2].z + quad->dim.quad[3].z) * 0.25f;
            return true;
        }
        case COLSHAPE_CYLINDER: {
            ColliderCylinder* cylinder = (ColliderCylinder*)collider;

            out->x = cylinder->dim.pos.x;
            out->y = cylinder->dim.pos.y;
            out->z = cylinder->dim.pos.z;
            return true;
        }
        default:
            if (collider->actor == NULL) {
                return false;
            }
            *out = collider->actor->world.pos;
            return true;
    }
}

// The attack list is complete only between the actor updates and the next frame's wipe, which is where the draw
// runs. Positions are kept, never pointers, so nothing goes stale before the next update reads them.
static void SnapshotIncomingAttacks(PlayState* play, Player* player) {
    sChampion.attackCount = 0;
    for (s32 i = 0; i < play->colChkCtx.colATCount && sChampion.attackCount < ATTACK_SNAPSHOT_MAX; i++) {
        Collider* collider = play->colChkCtx.colAT[i];

        if (collider == NULL || !(collider->atFlags & AT_ON)) {
            continue;
        }
        if (collider->actor == &player->actor || (collider->actor != NULL && collider->actor->parent == &player->actor)) {
            continue;
        }
        if (GetColliderCenter(collider, &sChampion.attackPos[sChampion.attackCount])) {
            sChampion.attackCount++;
        }
    }
}

static bool IsAttackNearby(Player* player) {
    for (s32 i = 0; i < sChampion.attackCount; i++) {
        if (Math_Vec3f_DistXYZ(&sChampion.attackPos[i], &player->actor.world.pos) <= DODGE_RANGE) {
            return true;
        }
    }
    return false;
}

static void RememberTargetCollider(Collider* collider) {
    if (sChampion.state != CHAMPION_RUSH || collider->actor != sChampion.target) {
        return;
    }
    for (s32 i = 0; i < sChampion.targetColliderCount; i++) {
        if (sChampion.targetColliders[i] == collider) {
            return;
        }
    }
    if (sChampion.targetColliderCount < TARGET_COLLIDER_MAX) {
        sChampion.targetColliders[sChampion.targetColliderCount++] = collider;
    }
}

static bool IsRegisteredAC(PlayState* play, Collider* collider) {
    for (s32 i = 0; i < play->colChkCtx.colACCount; i++) {
        if (play->colChkCtx.colAC[i] == collider) {
            return true;
        }
    }
    return false;
}

// Enemies go untouchable between hits, which would eat every swing after the first. The victim's hurtboxes are
// put back for the next frame's attack pass, once each.
static void KeepTargetHittable(PlayState* play) {
    for (s32 i = 0; i < sChampion.targetColliderCount; i++) {
        Collider* collider = sChampion.targetColliders[i];

        if (IsRegisteredAC(play, collider)) {
            continue;
        }
        collider->acFlags |= AC_ON;
        CollisionCheck_SetAC(play, &play->colChkCtx, collider);
    }
}

// prevPos moves with world.pos and bgCheckFlags is cleared so the engine does not read the blink as a sweep and
// drag Link back. The height is the enemy's, so a flying target does not leave him standing on air.
static void BlinkToTarget(Player* player, Actor* target) {
    f32 dx = player->actor.world.pos.x - target->world.pos.x;
    f32 dz = player->actor.world.pos.z - target->world.pos.z;
    f32 distXZ = sqrtf(SQ(dx) + SQ(dz));
    Vec3f dest;

    if (distXZ > MAX_TELEPORT) {
        return;
    }
    if (distXZ < 1.0f) {
        dx = Math_SinS(target->shape.rot.y);
        dz = Math_CosS(target->shape.rot.y);
        distXZ = 1.0f;
    }
    dest.x = target->world.pos.x + (dx / distXZ) * TELEPORT_DIST;
    dest.y = target->world.pos.y;
    dest.z = target->world.pos.z + (dz / distXZ) * TELEPORT_DIST;
    player->actor.world.pos = dest;
    player->actor.prevPos = dest;
    player->actor.bgCheckFlags = 0;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->actor.shape.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &target->world.pos);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->yaw = player->actor.shape.rot.y;
}

static void PlayAttentionSfx(Player* player) {
    Audio_PlaySoundGeneral(NA_SE_SY_ATTENTION_ON, &player->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void ReturnToIdle(PlayState* play) {
    bool wasFlurry = sChampion.state == CHAMPION_OFFER || sChampion.state == CHAMPION_RUSH;

    sChampion.state = CHAMPION_IDLE;
    sChampion.timer = 0;
    sChampion.hits = 0;
    sChampion.connectTimer = 0;
    sChampion.target = NULL;
    sChampion.targetColliderCount = 0;
    sChampion.flashTimer = 0;
    if (wasFlurry) {
        sChampion.cooldown = DODGE_COOLDOWN;
    }
    SlowWorld(false);
    if (play != NULL) {
        SetScreenTint(play, 0);
    }
}

// The dodge only buys the slow motion: Link is not moved and stays vulnerable, and a hit cancels it.
static void OfferFlurry(Player* player, PlayState* play) {
    sChampion.state = CHAMPION_OFFER;
    sChampion.timer = OFFER_FRAMES;
    sChampion.hits = 0;
    sChampion.target = GetLockedEnemy(player);
    sChampion.flashTimer = SCREEN_FLASH_FRAMES;
    SlowWorld(true);
    SetScreenTint(play, TINT_OFFER_ALPHA);
    PlayAttentionSfx(player);
}

static void CommitFlurry(Player* player, PlayState* play) {
    sChampion.state = CHAMPION_RUSH;
    sChampion.timer = RUSH_FRAMES;
    sChampion.connectTimer = CONNECT_FRAMES;
    sChampion.hits = 0;
    sChampion.targetColliderCount = 0;
    BlinkToTarget(player, sChampion.target);
    SetScreenTint(play, TINT_RUSH_ALPHA);
    PlayAttentionSfx(player);
}

static void EnterBulletTime(Player* player, PlayState* play) {
    sChampion.state = CHAMPION_BULLET_TIME;
    SlowWorld(true);
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    SetScreenTint(play, TINT_BULLET_ALPHA);
}

static void UpdateIdle(Player* player, PlayState* play) {
    bool isHopping = (player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0;

    if (!IsGrounded(player) && HoldsAimableItem(player) && IsAiming(player)) {
        EnterBulletTime(player, play);
        return;
    }
    // Any frame of the hop counts: on its first frame the blade is usually still travelling toward Link.
    if (isHopping && sChampion.cooldown == 0 && Player_IsZTargeting(player) && IsAttackNearby(player)) {
        OfferFlurry(player, play);
    }
}

static void UpdateOffer(Player* player, PlayState* play, bool tookDamage) {
    if (tookDamage) {
        ReturnToIdle(play);
        return;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && IsActorAlive(play, sChampion.target)) {
        CommitFlurry(player, play);
        return;
    }
    if (--sChampion.timer <= 0) {
        ReturnToIdle(play);
    }
}

// Many enemies gate their invulnerability on the damage flash, so it is cut short every frame of the rush.
static void UpdateRush(PlayState* play, bool tookDamage) {
    if (tookDamage || !IsActorAlive(play, sChampion.target)) {
        ReturnToIdle(play);
        return;
    }
    sChampion.target->colorFilterTimer = 0;
    if (--sChampion.connectTimer <= 0 || --sChampion.timer <= 0) {
        ReturnToIdle(play);
    }
}

// The fall is suspended and nothing else: aiming is the game's own first-person aim.
static void UpdateBulletTime(Player* player, PlayState* play) {
    if (IsGrounded(player) || !HoldsAimableItem(player) || !IsAiming(player)) {
        ReturnToIdle(play);
        return;
    }
    player->actor.velocity.y = BULLET_FLOAT;
    SetScreenTint(play, TINT_BULLET_ALPHA);
}

static void CountFlurryHit(PlayState* play, Player* player);

static void SettleFlash(PlayState* play) {
    if (sChampion.flashTimer <= 0 || --sChampion.flashTimer > 0) {
        return;
    }
    if (sChampion.state == CHAMPION_OFFER || sChampion.state == CHAMPION_RUSH) {
        SetScreenTint(play, TINT_SETTLED_ALPHA);
    }
}

static void TickTunic(void) {
    PlayState* play = gPlayState;
    Player* player;
    bool tookDamage;

    if (play == NULL || !IsWorn()) {
        return;
    }
    player = GET_PLAYER(play);
    tookDamage = player->invincibilityTimer > 0 && sChampion.prevInvincibility == 0;
    sChampion.prevInvincibility = player->invincibilityTimer;
    if (IsPlayerBusy(player)) {
        if (sChampion.state != CHAMPION_IDLE) {
            ReturnToIdle(play);
        }
        return;
    }
    if (sChampion.cooldown > 0) {
        sChampion.cooldown--;
    }
    SettleFlash(play);
    CountFlurryHit(play, player);
    switch (sChampion.state) {
        case CHAMPION_IDLE:
            UpdateIdle(player, play);
            break;
        case CHAMPION_OFFER:
            UpdateOffer(player, play, tookDamage);
            break;
        case CHAMPION_RUSH:
            UpdateRush(play, tookDamage);
            break;
        case CHAMPION_BULLET_TIME:
            UpdateBulletTime(player, play);
            break;
        default:
            ReturnToIdle(play);
            break;
    }
}

static void WatchCollisionsAfterUpdate(void) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsWorn()) {
        return;
    }
    SnapshotIncomingAttacks(play, GET_PLAYER(play));
    if (sChampion.state == CHAMPION_RUSH && IsActorAlive(play, sChampion.target)) {
        KeepTargetHittable(play);
    }
}

// A blade quad keeps AT_HIT for the rest of its swing, so a landed blow is its rising edge.
static void CountFlurryHit(PlayState* play, Player* player) {
    bool isHitting = (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) ||
                     (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);
    bool isNewHit = isHitting && !sChampion.wasHitting;
    u8 limit = Player_HoldsTwoHandedWeapon(player) ? HITS_TWO_HAND : HITS_ONE_HAND;

    sChampion.wasHitting = isHitting;
    if (sChampion.state != CHAMPION_RUSH || !isNewHit) {
        return;
    }
    sChampion.connectTimer = CONNECT_FRAMES;
    if (++sChampion.hits >= limit) {
        ReturnToIdle(play);
    }
}

// Only in the air: on the ground vanilla keeps every one of its ways out of the aim.
static void AllowMidairAim(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);

    if (!IsWorn() || IsGrounded(player)) {
        return;
    }
    if (sChampion.state == CHAMPION_BULLET_TIME || HoldsAimableItem(player)) {
        *should = true;
    }
}

static void DyeTunic(bool* should, va_list args) {
    Color_RGB8* color;

    va_arg(args, void*);
    color = va_arg(args, Color_RGB8*);
    if (!IsWorn()) {
        return;
    }
    color->r = 0x38;
    color->g = 0xB6;
    color->b = 0xF1;
}

static void DrawGetItem(PlayState* play, GetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPGrayscale(POLY_OPA_DISP++, true);
    gDPSetGrayscaleColor(POLY_OPA_DISP++, 0, 120, 215, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiTunicCollarDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiTunicDL);
    gSPGrayscale(POLY_OPA_DISP++, false);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ForgetSceneState(int16_t sceneNum) {
    ReturnToIdle(NULL);
    sChampion.attackCount = 0;
    sChampion.prevInvincibility = 0;
}

static void ReleaseTunic(const char* key) {
    ReturnToIdle(gPlayState);
    sChampion.cooldown = 0;
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayDrawBegin", "OnCollisionRegisterAC",
                                              "OnSceneInit" };

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
    definition.key = CHAMPION_TUNIC_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rChampion's Tunic&%wLocked on, hop past an attack and swing %y\xA0%w for a flurry "
                           "rush. Aim in mid air to slow time.";
    definition.getItemText = "You got the %rChampion's Tunic%w!&The blue of a knight chosen to guard the princess. "
                             "Dodge an attack at the last moment or aim while airborne, and the world slows for you.";
    definition.slot = SOH_EQUIP_SLOT_TUNIC;
    definition.page = 0;
    definition.row = SOH_EQUIP_SLOT_TUNIC;
    definition.column = 1;
    definition.ageRequirement = SOH_CUSTOM_ITEM_AGE_ANY;
    definition.vanillaBase = EQUIP_VALUE_TUNIC_KOKIRI;
    definition.toggles = 1;
    definition.onUnequip = ReleaseTunic;
    definition.getItemEntry.drawFunc = DrawGetItem;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickTunic);
    SOH_REGISTER_HOOK(sApi, OnPlayDrawBegin, WatchCollisionsAfterUpdate);
    SOH_REGISTER_HOOK(sApi, OnCollisionRegisterAC, RememberTargetCollider);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, DyeTunic);
    sApi->RegisterVB(VB_PLAYER_ALLOW_MIDAIR_AIM, AllowMidairAim);
}
