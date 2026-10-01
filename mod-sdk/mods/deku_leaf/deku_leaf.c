#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"

#define DEKU_LEAF_KEY "nei.deku_leaf"
#define ROC_BOOTS_KEY "nei.equip.roc_boots"
#define DEKU_LEAF_BUTTON_COUNT 8
#define DEKU_LEAF_GIVE_SCALE 0.35f
#define DEKU_LEAF_CANOPY_SCALE 0.16f
#define DEKU_LEAF_CANOPY_LIFT 6.0f
#define DEKU_LEAF_HELD_SCALE 0.08f
#define DEKU_LEAF_SWEPT_SCALE 0.25f
#define DEKU_LEAF_FALL_SPEED -1.0f
#define DEKU_LEAF_GLIDE_SPEED 6.0f
#define DEKU_LEAF_GLIDE_ACCEL 0.5f
#define DEKU_LEAF_GLIDE_DRAIN_INTERVAL 7
#define DEKU_LEAF_GLIDE_MAGIC 1
#define DEKU_LEAF_SWING_MAGIC 1
#define DEKU_LEAF_SWING_SPEED 2.0f
// Frames of the 39-frame swing: it stalls at its farthest reach on 8, the leaf is drawn wide from 10, and the
// gust leaves it on 15.
#define DEKU_LEAF_APEX_FRAME 8.0f
#define DEKU_LEAF_APEX_HOLD 5
#define DEKU_LEAF_GUST_FRAME 15.0f
#define DEKU_LEAF_WIDE_FRAME_FIRST 10.0f
#define DEKU_LEAF_WIDE_FRAME_LAST 22.0f
#define DEKU_LEAF_GUST_SPEED 26.0f
#define DEKU_LEAF_GUST_LIFE 14
#define DEKU_LEAF_GUST_DAMAGE 2
#define DEKU_LEAF_GUST_KNOCKBACK 40.0f
#define DEKU_LEAF_GUST_LIFT 6.0f
#define DEKU_LEAF_GUST_START 40.0f
#define DEKU_LEAF_GUST_HEIGHT 25.0f
#define DEKU_LEAF_COLLIDER_RADIUS 55
#define DEKU_LEAF_COLLIDER_HEIGHT 60
#define DEKU_LEAF_WIND_MOTES 3
#define DEKU_LEAF_AIR_BALL_MOTES 6

#define DEKU_LEAF_BLOCKING_STATES                                                                        \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS | \
     PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_HANGING_OFF_LEDGE |              \
     PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE |             \
     PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_IN_WATER)

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconDekuLeafTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gDekuLeafNameTex";
static const ALIGN_ASSET(2) char sLeafDL[] = "__OTR__objects/object_nei_deku_leaf/g_dekuleaf_dl";
static const ALIGN_ASSET(2) char sSwingAnimPath[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_dekuleaf_blow";
// Both hands held over the head is the carry pose, and it reads as holding the leaf up into the wind.
static const ALIGN_ASSET(2) char sGlideAnimPath[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_wait";

static const u16 sItemButtons[DEKU_LEAF_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                          BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const Color_RGBA8 sWindColor = { 220, 220, 220, 160 };
static const Color_RGBA8 sAirBallPrimColor = { 195, 225, 235, 160 };
static const Color_RGBA8 sAirBallEnvColor = { 150, 200, 220, 100 };

// OoT has no wind flag, and DMG_UNBLOCKABLE is a trap: no pot or crate accepts it, and a damage table is read
// at the HIGHEST flag set, which nearly every foe leaves at zero. The sword's is the flag they all answer to.
#define DEKU_LEAF_GUST_FLAGS (DMG_SLASH_KOKIRI | DMG_BOOMERANG)

static ColliderCylinderInit sGustColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { DEKU_LEAF_GUST_FLAGS, 0x00, DEKU_LEAF_GUST_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NONE,
      BUMP_NONE,
      OCELEM_NONE },
    { DEKU_LEAF_COLLIDER_RADIUS, DEKU_LEAF_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDrawEnd", "OnSceneInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sGustCollider;
static LinkAnimationHeader* sGlideAnim;
static bool sIsGliding;
static bool sHasGustLeft;
static bool sHasHeldApex;
static s16 sApexHold;
static Vec3f sGustPos;
static s16 sGustYaw;
static s16 sGustLife;
static s16 sDrainTimer;
static u8 sPreviousInvincibility;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void Player_ZeroSpeedXZ(Player* player);
void func_80839FFC(Player* player, PlayState* play);

static void SwingAction(Player* player, PlayState* play);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u16 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < DEKU_LEAF_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, DEKU_LEAF_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool HasMagic(s16 amount) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= amount;
}

// The meter's own consume state machine serves one request at a time and beeps at the rest, which a leaf
// that drains while it glides would trip on its next gust. A point off the meter is the whole cost here.
static bool SpendMagic(s16 amount) {
    if (!HasMagic(amount)) {
        return false;
    }
    gSaveContext.magic -= amount;
    return true;
}

static bool IsSwinging(Player* player) {
    return player->actionFunc == SwingAction;
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & 1) != 0;
}

// Only the resolved header can be told apart from whatever the falling action is playing, so the path is
// turned into a pointer once and that pointer is what both the comparison and the change use.
static LinkAnimationHeader* GlideAnimation(void) {
    if (sGlideAnim == NULL) {
        sGlideAnim = (LinkAnimationHeader*)ResourceMgr_LoadAnimByName(sGlideAnimPath);
    }
    return sGlideAnim;
}

static void SpawnWindMotes(PlayState* play, Vec3f* origin, s16 yaw) {
    for (u8 mote = 0; mote < DEKU_LEAF_WIND_MOTES; mote++) {
        s16 spread = (s16)Rand_CenteredFloat(0x2000);
        f32 reach = 10.0f + Rand_ZeroOne() * 20.0f;
        f32 speed = 15.0f + Rand_ZeroOne() * 10.0f;
        Vec3f pos = { origin->x + Math_SinS(yaw + spread) * reach, origin->y + Rand_CenteredFloat(15.0f),
                      origin->z + Math_CosS(yaw + spread) * reach };
        Vec3f velocity = { Math_SinS(yaw + spread) * speed, Rand_CenteredFloat(2.0f), Math_CosS(yaw + spread) * speed };
        Vec3f accel = { velocity.x * -0.05f, -0.1f, velocity.z * -0.05f };

        func_8002836C(play, &pos, &velocity, &accel, (Color_RGBA8*)&sWindColor, (Color_RGBA8*)&sWindColor, 80, 20, 10);
    }
}

// What the gust throws flies out wrapped in its own puff of wind.
static void SpawnAirBall(PlayState* play, Vec3f* pos) {
    for (u8 mote = 0; mote < DEKU_LEAF_AIR_BALL_MOTES; mote++) {
        s16 angle = (s16)Rand_CenteredFloat(65535.0f);
        f32 radius = 8.0f + Rand_ZeroFloat(14.0f);
        Vec3f motePos = { pos->x + Math_SinS(angle) * radius, pos->y + 10.0f + Rand_CenteredFloat(16.0f),
                          pos->z + Math_CosS(angle) * radius };
        Vec3f velocity = { Math_SinS(angle) * 3.0f, Rand_CenteredFloat(1.5f), Math_CosS(angle) * 3.0f };
        Vec3f accel = { 0.0f, 0.3f, 0.0f };

        func_8002836C(play, &motePos, &velocity, &accel, (Color_RGBA8*)&sAirBallPrimColor,
                      (Color_RGBA8*)&sAirBallEnvColor, 200, 25, 12);
    }
}

static void LaunchGust(Player* player) {
    sGustYaw = player->actor.shape.rot.y;
    sGustPos.x = player->actor.world.pos.x + Math_SinS(sGustYaw) * DEKU_LEAF_GUST_START;
    sGustPos.y = player->actor.world.pos.y + DEKU_LEAF_GUST_HEIGHT;
    sGustPos.z = player->actor.world.pos.z + Math_CosS(sGustYaw) * DEKU_LEAF_GUST_START;
    sGustLife = DEKU_LEAF_GUST_LIFE;
    sGustCollider.base.atFlags &= ~AT_HIT;
}

static void BurstGust(PlayState* play) {
    SpawnAirBall(play, &sGustPos);
    PlaySfxAt(NA_SE_EV_PLANT_BROKEN, &sGustPos);
    sGustLife = 0;
    sGustCollider.base.atFlags &= ~(AT_ON | AT_HIT);
}

// What the wind reaches is thrown the way the wind was already going, not away from where it came from.
static void ThrowHitActor(PlayState* play, Actor* hit) {
    hit->world.rot.y = sGustYaw;
    hit->speedXZ = DEKU_LEAF_GUST_KNOCKBACK;
    hit->velocity.x = Math_SinS(sGustYaw) * DEKU_LEAF_GUST_KNOCKBACK;
    hit->velocity.z = Math_CosS(sGustYaw) * DEKU_LEAF_GUST_KNOCKBACK;
    hit->velocity.y = DEKU_LEAF_GUST_LIFT;
    SpawnAirBall(play, &hit->world.pos);
}

static bool HasHitWall(PlayState* play, Vec3f* from, Vec3f* to) {
    CollisionPoly* poly = NULL;
    s32 bgId;
    Vec3f hit;

    if (!BgCheck_EntityLineTest1(&play->colCtx, from, to, &hit, &poly, true, true, true, true, &bgId)) {
        return false;
    }
    *to = hit;
    return true;
}

// The hit only shows up in the flags once the frame's collision pass has run, so it is read on the next tick,
// by which time whatever it was may already be gone.
static Actor* TakeGustHit(void) {
    Actor* hit = sGustCollider.base.at;

    if (!(sGustCollider.base.atFlags & AT_HIT)) {
        return NULL;
    }
    sGustCollider.base.atFlags &= ~AT_HIT;
    return hit != NULL && hit->update != NULL ? hit : NULL;
}

static void AdvanceGust(PlayState* play) {
    Vec3f previous = sGustPos;
    Actor* hit = TakeGustHit();

    if (hit != NULL) {
        ThrowHitActor(play, hit);
        BurstGust(play);
        return;
    }
    sGustPos.x += Math_SinS(sGustYaw) * DEKU_LEAF_GUST_SPEED;
    sGustPos.z += Math_CosS(sGustYaw) * DEKU_LEAF_GUST_SPEED;
    SpawnWindMotes(play, &sGustPos, sGustYaw);
    if (HasHitWall(play, &previous, &sGustPos)) {
        BurstGust(play);
        return;
    }
    sGustCollider.dim.pos.x = (s16)sGustPos.x;
    sGustCollider.dim.pos.y = (s16)sGustPos.y;
    sGustCollider.dim.pos.z = (s16)sGustPos.z;
    sGustCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sGustCollider.base);
    if (--sGustLife <= 0) {
        sGustLife = 0;
        sGustCollider.base.atFlags &= ~AT_ON;
    }
}

// The wind-up hangs at its farthest reach before the leaf sweeps, and the swing picks up where it left off.
static void HoldSwingAtApex(Player* player) {
    if (sApexHold > 0) {
        sApexHold--;
        player->skelAnime.playSpeed = 0.0f;
        return;
    }
    if (!sHasHeldApex && player->skelAnime.curFrame >= DEKU_LEAF_APEX_FRAME) {
        sHasHeldApex = true;
        sApexHold = DEKU_LEAF_APEX_HOLD;
        player->skelAnime.playSpeed = 0.0f;
        return;
    }
    player->skelAnime.playSpeed = DEKU_LEAF_SWING_SPEED;
}

static void SwingAction(Player* player, PlayState* play) {
    HoldSwingAtApex(player);
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        func_80839FFC(player, play);
        return;
    }
    Player_ZeroSpeedXZ(player);
    if (!sHasGustLeft && player->skelAnime.curFrame >= DEKU_LEAF_GUST_FRAME) {
        sHasGustLeft = true;
        SpendMagic(DEKU_LEAF_SWING_MAGIC);
        LaunchGust(player);
        PlaySfxAt(NA_SE_IT_SWORD_SWING, &player->actor.world.pos);
        // The effort lands on the beat the gust leaves, as it does on a jump slash.
        Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    }
}

static bool CanSwingLeaf(Player* player, PlayState* play) {
    return sApi->GetActiveForm() == NULL && IsGrounded(player) &&
           !(player->stateFlags1 & DEKU_LEAF_BLOCKING_STATES) &&
           (player->meleeWeaponState == 0) && HasMagic(DEKU_LEAF_SWING_MAGIC);
}

// The gust is timed by the animation, so without the resource the leaf never swings at all.
static void SwingLeaf(PlayState* play, Player* player) {
    LinkAnimationHeader* swing = ResourceMgr_LoadPlayerAnimAsHeader(sSwingAnimPath);

    if (swing == NULL || IsSwinging(player) || !CanSwingLeaf(player, play)) {
        return;
    }
    Collider_InitCylinder(play, &sGustCollider);
    Collider_SetCylinder(play, &sGustCollider, &player->actor, &sGustColliderInit);
    sHasGustLeft = false;
    sHasHeldApex = false;
    sApexHold = 0;
    sGustLife = 0;
    Player_SetupAction(play, player, SwingAction, 0);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_PlayOnce(play, &player->skelAnime, swing);
    player->skelAnime.playSpeed = DEKU_LEAF_SWING_SPEED;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
}

static void StartGliding(Player* player) {
    sIsGliding = true;
    sDrainTimer = 0;
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

// The rustle is asked for frame by frame, so letting go of the glide is what silences it.
static void StopGliding(void) {
    sIsGliding = false;
}

static bool DrainGlideMagic(void) {
    if (++sDrainTimer < DEKU_LEAF_GLIDE_DRAIN_INTERVAL) {
        return true;
    }
    sDrainTimer = 0;
    return SpendMagic(DEKU_LEAF_GLIDE_MAGIC);
}

// Link's yaw already follows the stick in the air, so a steady forward drift is all the leaf has to add.
static void UpdateGlide(Player* player, PlayState* play) {
    LinkAnimationHeader* glide = GlideAnimation();

    if (glide != NULL && player->skelAnime.animation != glide) {
        LinkAnimation_Change(play, &player->skelAnime, glide, 1.0f, 0.0f, Animation_GetLastFrame(glide), ANIMMODE_LOOP,
                             -4.0f);
    }
    // Roc's Boots halve every fall; the leaf pins its own, so it is halved here.
    f32 fallSpeed = CustomEquipRegistry_IsWorn(ROC_BOOTS_KEY) ? DEKU_LEAF_FALL_SPEED * 0.5f : DEKU_LEAF_FALL_SPEED;

    if (player->actor.velocity.y < fallSpeed) {
        player->actor.velocity.y = fallSpeed;
    }
    if (player->linearVelocity < DEKU_LEAF_GLIDE_SPEED) {
        Math_StepToF(&player->linearVelocity, DEKU_LEAF_GLIDE_SPEED, DEKU_LEAF_GLIDE_ACCEL);
    }
    // The rustle of the bean plant's own leaf, asked for every frame the way that plant asks for it.
    func_8002F974(&player->actor, NA_SE_PL_PLANT_MOVE - SFX_FLAG);
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return isHurt;
}

static bool CanKeepGliding(Player* player, PlayState* play, u16 buttonMask) {
    return !IsGrounded(player) && (play->state.input[0].cur.button & buttonMask) &&
           !(player->stateFlags1 & DEKU_LEAF_BLOCKING_STATES) && !WasJustHurt(player);
}

// Both the swing and the glide read the button here, not through vanilla's item path: that one only runs on the
// frames the current action func updates the upper body, so a press made on any other frame was simply lost.
// The gust flies on from here too, so it outlives the swing that threw it.
static void UpdateDekuLeaf(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);
    u16 buttonMask = FindEquippedButtonMask();

    if (play == NULL || player == NULL) {
        StopGliding();
        return;
    }
    // A transformation which allows the leaf owns its form-specific behavior.
    if (sApi->GetActiveForm() != NULL) {
        StopGliding();
        return;
    }
    if (sGustLife > 0) {
        AdvanceGust(play);
    }
    if (buttonMask == 0) {
        StopGliding();
        return;
    }
    if (sIsGliding) {
        if (!CanKeepGliding(player, play, buttonMask) || !DrainGlideMagic()) {
            StopGliding();
            return;
        }
        UpdateGlide(player, play);
        return;
    }
    sPreviousInvincibility = player->invincibilityTimer;
    if (!(play->state.input[0].press.button & buttonMask) || IsSwinging(player) || sApi->GetWorldSpeed() == 0.0f ||
        (player->stateFlags1 & DEKU_LEAF_BLOCKING_STATES)) {
        return;
    }
    if (IsGrounded(player)) {
        SwingLeaf(play, player);
        return;
    }
    if (HasMagic(DEKU_LEAF_GLIDE_MAGIC)) {
        StartGliding(player);
    }
}

// The leaf's own display list clears lighting for everything drawn after it, so the mode is put back by hand.
static void DrawLeaf(PlayState* play, Vec3f* pos, f32 yaw, f32 scale) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_RotateY(yaw + M_PI, MTXMODE_APPLY);
    Matrix_RotateX(-M_PI / 2.0f, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLeafDL);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);
    CLOSE_DISPS(play->state.gfxCtx);
}

// A canopy over both hands, not a leaf at his chest: anchored anywhere lower it cuts through Link as he falls.
static void DrawCanopy(Player* player, PlayState* play) {
    Vec3f* leftHand = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f* rightHand = &player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    Vec3f canopy = { (leftHand->x + rightHand->x) * 0.5f, (leftHand->y + rightHand->y) * 0.5f + DEKU_LEAF_CANOPY_LIFT,
                     (leftHand->z + rightHand->z) * 0.5f };

    DrawLeaf(play, &canopy, BINANG_TO_RAD(player->actor.shape.rot.y), DEKU_LEAF_CANOPY_SCALE);
}

static void DrawLeafInHand(Player* player, PlayState* play) {
    Vec3f* forearm = &player->bodyPartsPos[PLAYER_BODYPART_L_FOREARM];
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    f32 yaw = Math_FAtan2F(hand.x - forearm->x, hand.z - forearm->z);
    bool isSweeping = player->skelAnime.curFrame >= DEKU_LEAF_WIDE_FRAME_FIRST &&
                      player->skelAnime.curFrame <= DEKU_LEAF_WIDE_FRAME_LAST;

    DrawLeaf(play, &hand, yaw, isSweeping ? DEKU_LEAF_SWEPT_SCALE : DEKU_LEAF_HELD_SCALE);
}

static void DrawDekuLeaf(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) {
        return;
    }
    if (sIsGliding) {
        DrawCanopy(player, play);
        return;
    }
    if (IsSwinging(player)) {
        DrawLeafInHand(player, play);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(DEKU_LEAF_GIVE_SCALE, DEKU_LEAF_GIVE_SCALE, DEKU_LEAF_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sLeafDL);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The scene rebuilds the player, and the collider still points at the old one until the next swing sets it.
static void DropGlideOnSceneChange(int16_t sceneNum) {
    sIsGliding = false;
    sGustLife = 0;
    sPreviousInvincibility = 0;
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    SOHCustomItemDefinition leaf = Z64Items_Define(DEKU_LEAF_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&leaf, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&leaf, 0, 6, 0);
    Z64Items_SetAge(&leaf, SOH_CUSTOM_ITEM_AGE_CHILD);
    Z64Items_SetTextbox(&leaf, "You got the %rDeku Leaf%w!&A leaf of the Great Deku Tree,&wide enough to ride the "
                               "wind.^"
                               "On the ground, %y\xA1%w beats it into a&%ggust%w that stuns what stands&ahead and "
                               "hurls it away.^"
                               "In the air, hold %y\xA1%w to open it&and %gglide%w. Every moment aloft&spends a little "
                               "%gmagic%w.");
    Z64Items_SetPauseText(&leaf, "%rDeku Leaf&%wPress %y\xA1%w to fan a gust. Hold it&in midair to glide. As Deku: flower.");
    Z64Items_SetCanUse(&leaf, CanSwingLeaf);
    leaf.flags |= SOH_CUSTOM_ITEM_INSTANT;
    leaf.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &leaf)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateDekuLeaf);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, DropGlideOnSceneChange);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawDekuLeaf);
}
