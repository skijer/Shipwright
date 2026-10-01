#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"

#define DEMISE_KEY "nei.demise_destruction"
#define DEMISE_MAGIC_COST 12
#define DEMISE_WINDUP_DURATION 70
#define DEMISE_FINISH_DURATION 5
#define DEMISE_COLLISION_RADIUS 400
#define DEMISE_COLLISION_HEIGHT 200
#define DEMISE_DAMAGE 40
#define DEMISE_DAMAGE_FLAGS (DMG_HAMMER_SWING | DMG_HAMMER_JUMP | DMG_BOOMERANG)
#define DEMISE_GIVE_SCALE 1.0f
#define DEMISE_DARKNESS 0.8f
#define DEMISE_DARKEN_FRAMES 20
#define DEMISE_STORM_AFTERMATH 60

typedef enum {
    DEMISE_IDLE,
    DEMISE_WINDUP,
    DEMISE_FINISH,
} DemisePhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconDemiseDestructionTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gDemiseDestructionNameTex";
static const ALIGN_ASSET(2) char sCastAnim[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_demise_destruction";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_magic_spell/gDemiseDestructionGiveDL";

static ColliderCylinderInit sColliderInit = { { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE,
                                                COLSHAPE_CYLINDER },
                                              { ELEMTYPE_UNK2,
                                                { DEMISE_DAMAGE_FLAGS, 0x00, DEMISE_DAMAGE },
                                                { 0, 0, 0 },
                                                TOUCH_ON | TOUCH_SFX_NORMAL,
                                                BUMP_NONE,
                                                OCELEM_NONE },
                                              { 100, 80, 0, { 0, 0, 0 } } };

static Color_RGBA8 sDustColor = { 60, 0, 0, 255 };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static ColliderCylinder sCollider;
static bool sIsColliderReady;
static u8 sPhase;
static s16 sTimer;
static s8 sPreviousInvincibility;
static f32 sDarkness;
static s16 sStormTimer;
static bool sOwnsStorm;
static s16 sSceneNum = -1;
static u32 sLastFrame;

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static void SpawnExplosionAt(PlayState* play, Vec3f* pos, f32 scale) {
    Vec3f velocity = { 0.0f, 8.0f * scale, 0.0f };
    Vec3f accel = { 0.0f, -0.3f, 0.0f };

    EffectSsBomb2_SpawnLayered(play, pos, &velocity, &accel, (s16)(120 * scale), (s16)(30 * scale));
}

static void SpawnExplosionRing(PlayState* play, Vec3f* center, f32 radius, u8 count) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        s16 angle = (s16)(i * (0x10000 / count));
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y, center->z + Math_CosS(angle) * radius };

        EffectSsBomb2_SpawnLayered(play, &pos, &zero, &zero, 80, 20);
    }
}

static void SpawnLightningRing(PlayState* play, Vec3f* center, u8 count) {
    Color_RGBA8 prim = { 255, 255, 255, 255 };
    Color_RGBA8 env = { 180, 0, 0, 255 };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 radius = 200.0f + Rand_ZeroOne() * 360.0f;
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y, center->z + Math_CosS(angle) * radius };

        EffectSsLightning_Spawn(play, &pos, &prim, &env, 80, (s16)(Rand_ZeroOne() * 0xFFFF), 6, 2);
        EffectSsBomb2_SpawnLayered(play, &pos, &zero, &zero, 15, 5);
    }
}

// Dust crawling outward along the ground, the tell that the blast is still growing.
static void SpawnGroundDust(PlayState* play, Vec3f* center, u8 count) {
    Color_RGBA8 env = { 0, 0, 0, 255 };

    for (u8 i = 0; i < count; i++) {
        s16 angle = (s16)(i * (0x10000 / count));
        f32 radius = 80.0f + Rand_ZeroOne() * 240.0f;
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y + 2.0f,
                      center->z + Math_CosS(angle) * radius };
        Vec3f velocity = { Math_SinS(angle) * 3.0f, 1.0f + Rand_ZeroOne() * 2.0f, Math_CosS(angle) * 3.0f };
        Vec3f accel = { -velocity.x * 0.1f, -0.1f, -velocity.z * 0.1f };

        func_8002829C(play, &pos, &velocity, &accel, &sDustColor, &env, 300, 10);
    }
}

static void ReleaseCamera(PlayState* play) {
    func_8005B1A4(Play_GetCamera(play, 0));
}

static void RequestQuake(PlayState* play, s16 speed, s16 strength, s16 duration) {
    s16 quake = Quake_Add(Play_GetCamera(play, 0), 3);

    Quake_SetSpeed(quake, speed);
    Quake_SetQuakeValues(quake, strength, 0, 0, 0);
    Quake_SetCountdown(quake, duration);
}

// The Megaton Hammer's floor hit: tektites flip and every actor that listens for the hammer reacts near Link.
static void ShakeGroundLikeHammer(PlayState* play) {
    RequestQuake(play, 27767, 12, 30);
    play->actorCtx.unk_02 = 4;
}

// En_Okarina_Effect's storm without its Flags_SetEnv(5): the spell brings the weather, not Song of Storms' puzzles.
static void SummonStorm(PlayState* play) {
    sStormTimer = DEMISE_WINDUP_DURATION + DEMISE_STORM_AFTERMATH;
    if (sOwnsStorm || play->envCtx.gloomySkyMode != 0 || play->envCtx.lightningMode != LIGHTNING_MODE_OFF) {
        return;
    }
    play->envCtx.unk_F2[0] = 20;
    play->envCtx.gloomySkyMode = 1;
    if ((gWeatherMode != 0) || (play->envCtx.unk_17 != 0)) {
        play->envCtx.unk_DE = 1;
    }
    play->envCtx.lightningMode = LIGHTNING_MODE_ON;
    Environment_PlayStormNatureAmbience(play);
    sOwnsStorm = true;
}

static void CalmStorm(PlayState* play) {
    sOwnsStorm = false;
    play->envCtx.unk_F2[0] = 0;
    Environment_StopStormNatureAmbience(play);
    if ((gWeatherMode == 0) && (play->envCtx.gloomySkyMode == 1)) {
        play->envCtx.gloomySkyMode = 2;
    } else {
        play->envCtx.gloomySkyMode = 0;
        play->envCtx.unk_DE = 0;
    }
    play->envCtx.lightningMode = LIGHTNING_MODE_LAST;
}

// Same dimming En_M_Thunder applies around a magic spin; it has to be re-sent every frame until it is back at 0.
static void UpdateDarkness(PlayState* play) {
    f32 target = (sPhase == DEMISE_WINDUP) ? DEMISE_DARKNESS : 0.0f;

    if ((sDarkness == 0.0f) && (target == 0.0f)) {
        return;
    }
    Math_StepToF(&sDarkness, target, DEMISE_DARKNESS / DEMISE_DARKEN_FRAMES);
    Environment_AdjustLights(play, sDarkness, 850.0f, 0.2f, 0.0f);
}

static void UpdateStorm(PlayState* play) {
    if (sStormTimer <= 0) {
        return;
    }
    sStormTimer--;
    if ((sStormTimer == 0) && sOwnsStorm) {
        CalmStorm(play);
    }
}

// A scene load rebuilds envCtx and the lights, so nothing the spell set up belongs to the new scene.
static void ForgetPreviousScene(PlayState* play) {
    bool isNewScene = (play->sceneNum != sSceneNum) || (play->state.frames < sLastFrame);

    sSceneNum = play->sceneNum;
    sLastFrame = play->state.frames;
    if (!isNewScene) {
        return;
    }
    if (sPhase != DEMISE_IDLE) {
        Magic_Reset(play);
    }
    sPhase = DEMISE_IDLE;
    sTimer = 0;
    sDarkness = 0.0f;
    sStormTimer = 0;
    sOwnsStorm = false;
}

static void Stop(Player* player, PlayState* play) {
    if (sPhase == DEMISE_IDLE) {
        return;
    }
    // NEI left these set when the wind-up was cut short, which stranded Link with no input.
    if (sPhase == DEMISE_WINDUP) {
        player->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED);
        ReleaseCamera(play);
    }
    sCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sPhase = DEMISE_IDLE;
    sTimer = 0;
    // Consuming leaves the meter flashing, and Magic_RequestChange refuses every cast until the spell resets it.
    Magic_Reset(play);
}

static bool CanCast(Player* player, PlayState* play) {
    return sPhase == DEMISE_IDLE && !(player->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
           (player->actor.bgCheckFlags & 1);
}

// Two frames early: the player is still finishing its own update and would stomp the camera setting back.
static void Cast(PlayState* play, Player* player) {
    if (!Magic_RequestChange(play, DEMISE_MAGIC_COST, MAGIC_CONSUME_NOW)) {
        return;
    }
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sCollider);
        sIsColliderReady = true;
    }
    Collider_SetCylinder(play, &sCollider, &player->actor, &sColliderInit);
    sPhase = DEMISE_WINDUP;
    sTimer = -2;
    sPreviousInvincibility = player->invincibilityTimer;
}

static void PinPlayerDown(Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
}

static void UpdateWindupEffects(Player* player, PlayState* play) {
    Vec3f* pos = &player->actor.world.pos;

    if (sTimer == 20) {
        PlaySfxAt(NA_SE_EN_GANON_AT_RETURN, pos);
    }
    if (sTimer > 20) {
        if (((sTimer - 20) % 8) == 0) {
            SpawnLightningRing(play, pos, 1);
        }
        if ((play->gameplayFrames % 4) == 0) {
            SpawnGroundDust(play, pos, 4);
        }
    }
    if (sTimer <= 50) {
        return;
    }
    if ((sTimer % 4) == 0) {
        SpawnLightningRing(play, pos, 2);
        PlaySfxAt(NA_SE_EV_LIGHTNING, pos);
    }
    if ((play->gameplayFrames % 3) == 0) {
        SpawnGroundDust(play, pos, 8);
    }
    if ((sTimer % 8) == 0) {
        PlaySfxAt(NA_SE_EV_EARTHQUAKE, pos);
        RequestQuake(play, 20000, 3, 8);
    }
    if ((sTimer % 2) == 0) {
        func_800AA000(200.0f, 180, 20, 10);
    }
}

static void Detonate(Player* player, PlayState* play) {
    Vec3f* pos = &player->actor.world.pos;

    SpawnExplosionRing(play, pos, DEMISE_COLLISION_RADIUS, 12);
    SpawnExplosionAt(play, pos, 1.0f);
    PlaySfxAt(NA_SE_IT_HAMMER_HIT, pos);
    PlaySfxAt(NA_SE_IT_BOMB_EXPLOSION, pos);
    func_800AA000(800.0f, 0xFF, 0x28, 0xC8);
    ShakeGroundLikeHammer(play);
}

static void UpdateWindup(Player* player, PlayState* play) {
    sTimer++;

    if (sTimer == -1) {
        Camera* camera = Play_GetCamera(play, 0);

        Camera_ChangeSetting(camera, CAM_SET_TURN_AROUND);
        Camera_SetCameraData(camera, 4, NULL, NULL, 10, 0, 0);
    }

    PinPlayerDown(player);

    if (sTimer == 0) {
        SummonStorm(play);
        LinkAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(sCastAnim);

        if (anim != NULL) {
            LinkAnimation_Change(play, &player->skelAnime, anim, 0.65f, 0.0f, Animation_GetLastFrame(anim),
                                 ANIMMODE_ONCE, -8.0f);
        }
    }
    if (sTimer >= 0) {
        LinkAnimation_Update(play, &player->skelAnime);
    }

    UpdateWindupEffects(player, play);

    if (sTimer < DEMISE_WINDUP_DURATION) {
        return;
    }
    Detonate(player, play);
    player->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED);
    ReleaseCamera(play);
    sPhase = DEMISE_FINISH;
    sTimer = 0;
}

// The blast is one cylinder of hammer-class damage, held live for a few frames after the flash.
static void UpdateFinish(Player* player, PlayState* play) {
    sCollider.dim.pos.x = (s16)player->actor.world.pos.x;
    sCollider.dim.pos.y = (s16)player->actor.world.pos.y;
    sCollider.dim.pos.z = (s16)player->actor.world.pos.z;
    sCollider.dim.radius = DEMISE_COLLISION_RADIUS;
    sCollider.dim.height = DEMISE_COLLISION_HEIGHT;
    sCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;

    CollisionCheck_SetAT(play, &play->colChkCtx, &sCollider.base);

    if (sCollider.base.atFlags & AT_HIT) {
        PlaySfxAt(NA_SE_IT_HAMMER_HIT, &player->actor.world.pos);
    }

    sTimer++;
    if (sTimer >= DEMISE_FINISH_DURATION) {
        Stop(player, play);
    }
}

static bool WasJustHurt(Player* player) {
    bool hurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return hurt;
}

static void UpdateSpell(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    ForgetPreviousScene(play);
    UpdateDarkness(play);
    UpdateStorm(play);
    if (sPhase == DEMISE_IDLE) {
        return;
    }
    if (WasJustHurt(player)) {
        Stop(player, play);
        return;
    }
    if (sPhase == DEMISE_WINDUP) {
        UpdateWindup(player, play);
    } else {
        UpdateFinish(player, play);
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(DEMISE_GIVE_SCALE, DEMISE_GIVE_SCALE, DEMISE_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomItemDefinition demise = Z64Items_Define(DEMISE_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&demise, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&demise, 0, 5, 0);
    Z64Items_SetTextbox(&demise, "You got %rDemise Destruction%w!&The Demon King's storm, bound&into a spell you can "
                                 "call.^"
                                 "Press %y\xA1%w with both feet on the&ground. The sky darkens, and&then the earth "
                                 "splits around you.^"
                                 "The blast breaks what the&%rMegaton Hammer%w breaks and&leaves the rest reeling. "
                                 "Uses&%gmagic%w.");
    Z64Items_SetPauseText(&demise, "%rDemise Destruction&%wPress %y\xA1%w on the ground to call&down the storm. Uses "
                                   "magic.");
    Z64Items_SetCanUse(&demise, CanCast);
    Z64Items_SetAction(&demise, Cast, NULL);
    demise.flags |= SOH_CUSTOM_ITEM_INSTANT;
    demise.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &demise)) {
        return;
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateSpell);
}
