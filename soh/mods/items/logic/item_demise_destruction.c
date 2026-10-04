/**
 * item_demise_destruction.c - Demise's Destruction (super attack)
 *
 * Controls:
 *   C Button: Perform devastating magic attack (high MP cost)
 *
 * Features:
 *   - Large area explosion with lightning effects
 *   - Heavy damage to all enemies in radius
 *   - Ground only (cannot use in water or air)
 *   - Custom superhero landing animation
 */

#include "z64.h"
#include "item_demise_destruction.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/fx_helper.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "../anim/nei_anims.h" // animation now loads from soh.o2r — Skijer's NEI

static s8 sDemisePrevInvinc = 0;
static FX_Color sDemiseDustColor = { 60, 0, 0, 255 };
static Color_RGBA8 sDemiseLightningPrim = { 185, 65, 255, 255 };
static Color_RGBA8 sDemiseLightningEnv = { 14, 0, 24, 255 };

static void Demise_SpawnLightningRing(PlayState* play, Vec3f* center, f32 radius, s16 phase, u8 count, s16 scale,
                                      s16 branches) {
    s16 cameraYaw = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));

    for (u8 i = 0; i < count; ++i) {
        s16 angle = phase + (i * 0x10000 / count);
        Vec3f pos = { center->x + Math_SinS(angle) * radius, center->y + (branches ? 88.0f : 48.0f),
                      center->z + Math_CosS(angle) * radius };
        // Native "yaw" is a billboard roll. Horizontal roots keep the arcs above
        // the floor; choose the outward screen direction without changing camera behavior.
        s16 roll = Math_SinS(angle - cameraYaw) >= 0.0f ? -0x4000 : 0x4000;

        EffectSsLightning_Spawn(play, &pos, &sDemiseLightningPrim, &sDemiseLightningEnv, scale, roll, 8, branches);
    }
}

static void Demise_SpawnGroundLightning(PlayState* play, Vec3f* center, s16 wave) {
    // Three rings travel from the slam toward the existing damage radius. No
    // native recursion here: its screen-space branches can descend into the floor.
    Demise_SpawnLightningRing(play, center, 72.0f + wave * 144.0f, 0, 6, 180, 0);
}

static void Demise_Stop(Player* p, PlayState* play) {
    if (!ddActive)
        return;
    ddCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    ddActive = 0;
    ddState = DEMISE_STATE_IDLE;
    ddTimer = 0;
}

static void Demise_Start(Player* p, PlayState* play) {
    if (ddActive)
        return;
    if (!ItemMagic_HasEnough(play, DEMISE_MAGIC_COST)) {
        Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    if (!(p->actor.bgCheckFlags & BGCHECKFLAG_GROUND))
        return;

    ddActive = 1;
    ddState = DEMISE_STATE_WINDUP;
    ddTimer = -2;
    ItemMagic_Consume(play, DEMISE_MAGIC_COST);
}

static void Demise_FinalExplosion(PlayState* play, Player* p) {
    // Ring of explosions + center explosion
    FX_SpawnRadialExplosion(play, &p->actor.world.pos, 400.0f, 12, 1.0f);
    FX_SpawnExplosion(play, &p->actor.world.pos, 1.0f);
    Demise_SpawnGroundLightning(play, &p->actor.world.pos, 0);

    Audio_PlaySoundGeneral(NA_SE_IT_HAMMER_HIT, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    Audio_PlaySoundGeneral(NA_SE_IT_BOMB_EXPLOSION, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    Rumble_Request(800.0f, 0xFF, 0x28, 0xC8);
}

static void Demise_StateWindup(Player* p, PlayState* play) {
    ddTimer++;

    if (ddTimer == -1) {
        Camera_RequestSetting(Play_GetCamera(play, 0), CAM_SET_TURN_AROUND);
        Camera_SetCameraData(Play_GetCamera(play, 0), 4, NULL, NULL, 10, 0, 0);
        p->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED;
    }

    p->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED;
    p->linearVelocity = 0.0f;
    p->actor.speedXZ = 0.0f;
    p->actor.velocity.x = p->actor.velocity.y = p->actor.velocity.z = 0.0f;

    if (ddTimer == 0) {
        // Loaded from soh.o2r; skip the anim change if the resource is missing (the timer-driven
        // effect below still runs). Skijer's NEI
        LinkAnimationHeader* anim = NeiAnim_Load(NEI_ANIM_DEMISE_DESTRUCTION);

        if (anim != NULL) {
            LinkAnimation_Change(play, &p->skelAnime, anim, 0.65f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_ONCE,
                                 -8.0f);
        }
    }

    if (ddTimer >= 0) {
        LinkAnimation_Update(play, &p->skelAnime);
    }

    if (ddTimer == 20) {
        Audio_PlaySoundGeneral(NA_SE_EN_GANON_AT_RETURN, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    if (ddTimer > 20) {
        if ((ddTimer < 50 && (ddTimer - 20) % 8 == 0) || (ddTimer >= 50 && ddTimer % 4 == 0)) {
            // One native branch per root keeps charging readable without filling
            // the 85-slot effect pool. The shared helper's bonus explosions are omitted.
            Demise_SpawnLightningRing(play, &p->actor.world.pos, 80.0f + (ddTimer - 20) * 1.5f, ddTimer * 0x1300, 2,
                                      130, 1);
        }
        if (play->gameplayFrames % 4 == 0)
            FX_SpawnRadialDust(play, &p->actor.world.pos, 80.0f, 320.0f, 4, &sDemiseDustColor);
    }

    if (ddTimer > 50) {
        if (ddTimer % 4 == 0) {
            Audio_PlaySoundGeneral(NA_SE_EV_LIGHTNING, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        if (play->gameplayFrames % 3 == 0)
            FX_SpawnRadialDust(play, &p->actor.world.pos, 80.0f, 320.0f, 8, &sDemiseDustColor);
        if (ddTimer % 8 == 0) {
            Audio_PlaySoundGeneral(NA_SE_EV_EARTHQUAKE, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        if (ddTimer % 2 == 0)
            Rumble_Request(200.0f, 180, 20, 10);
    }

    if (ddTimer == DEMISE_WINDUP_DURATION) {
        Demise_FinalExplosion(play, p);
        p->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED);
        func_8005B1A4(Play_GetCamera(play, 0));
        ddState = DEMISE_STATE_FINISH;
        ddTimer = 0;
    }
}

static void Demise_StateFinish(Player* p, PlayState* play) {
    if (ddTimer == 1 || ddTimer == 3) {
        Demise_SpawnGroundLightning(play, &p->actor.world.pos, (ddTimer + 1) / 2);
    }

    ddCollider.dim.pos.x = (s16)p->actor.world.pos.x;
    ddCollider.dim.pos.y = (s16)p->actor.world.pos.y;
    ddCollider.dim.pos.z = (s16)p->actor.world.pos.z;
    ddCollider.dim.radius = (s16)DEMISE_COLLISION_RADIUS;
    ddCollider.dim.height = DEMISE_COLLISION_HEIGHT;
    ddCollider.info.toucher.dmgFlags = DEMISE_DAMAGE_FLAGS;
    ddCollider.info.toucher.damage = DEMISE_DAMAGE;
    ddCollider.info.toucher.effect = 1;
    ddCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER;

    CollisionCheck_SetAT(play, &play->colChkCtx, &ddCollider.base);

    if (ddCollider.base.atFlags & AT_HIT) {
        Audio_PlaySoundGeneral(NA_SE_IT_HAMMER_HIT, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }

    ddTimer++;
    if (ddTimer >= DEMISE_FINISH_DURATION)
        Demise_Stop(p, play);
}

void Handle_DemiseDestruction(Player* p, PlayState* play) {
    if (ddCollider.base.shape != COLSHAPE_CYLINDER) {
        Player_InitDemiseDestructionIA(play, p);
    }

    ItemInputState in;
    ItemInput_Update(&in, ITEM_DEMISE_DESTRUCTION, p, play);

    if (!in.wasEquipped) {
        if (ddActive)
            Demise_Stop(p, play);
        return;
    }
    if (ItemInput_CheckDamage(p, &sDemisePrevInvinc)) {
        Demise_Stop(p, play);
        return;
    }
    if (in.otherButtonPressed) {
        Demise_Stop(p, play);
        return;
    }

    // Cannot use in water
    if (p->stateFlags1 & PLAYER_STATE1_IN_WATER)
        return;

    if (!ddActive) {
        if (ItemInput_IsBlocked(p, play))
            return;
        if (in.isPressed)
            Demise_Start(p, play);
        return;
    }

    switch (ddState) {
        case DEMISE_STATE_WINDUP:
            Demise_StateWindup(p, play);
            break;
        case DEMISE_STATE_FINISH:
            Demise_StateFinish(p, play);
            break;
        default:
            Demise_Stop(p, play);
            break;
    }
}

void CustomItems_DrawDemiseDestruction(Player* p, PlayState* play) {
}

s32 Player_UpperAction_DemiseDestruction(Player* p, PlayState* play) {
    return 0;
}

void Player_InitDemiseDestructionIA(PlayState* play, Player* p) {
    // Only initialize if collider not already set up (prevents resetting active state)
    if (ddCollider.base.shape != COLSHAPE_CYLINDER) {
        ddActive = 0;
        ddTimer = 0;
        ddState = DEMISE_STATE_IDLE;
        Collider_InitCylinder(play, &ddCollider);
        Collider_SetCylinder(play, &ddCollider, &p->actor, &sDemiseColInit);
    }
}
