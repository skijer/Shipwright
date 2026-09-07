/**
 * item_ballchain.c - Ball and Chain from Twilight Princess
 *
 * Controls:
 *   Hold C Button: Spin ball overhead (charging)
 *   Release C:     Throw ball in aimed direction
 *   During throw:  Ball returns automatically after hitting or max range
 *
 * Features:
 *   - Heavy damage to enemies and destructible objects
 *   - Breaks ice walls and armored enemies
 *   - Can activate heavy switches
 *   - Uses skeletal animation for swing poses
 *   - Destroys Goron City pot (drops ALL rewards at once)
 *   - Destroys Shadow Temple pots (drops collectibles/keys)
 */

#include "z64.h"
#include "item_ballchain.h"
#include "../custom_items.h"
#include "../helpers/camera_helper.h"
#include "../helpers/equip_helper.h"
#include "../helpers/combat_helper.h"
#include "../helpers/item_voice.h"
#include "../anim/ballchain/ballchain_anim_data.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_Bg_Ice_Shelter/z_bg_ice_shelter.h"
#include "overlays/actors/ovl_Bg_Jya_Ironobj/z_bg_jya_ironobj.h"
#include "overlays/actors/ovl_Bg_Spot18_Basket/z_bg_spot18_basket.h"
#include "overlays/actors/ovl_Bg_Haka_Tubo/z_bg_haka_tubo.h"
#include "objects/object_haka_objects/object_haka_objects.h"
#include "overlays/actors/ovl_Bg_Ice_Turara/z_bg_ice_turara.h"
#include "overlays/actors/ovl_En_Fz/z_en_fz.h"
// Non-static in their .c but not exposed in their headers. Skijer's NEI
extern void EnFz_SetupMelt(EnFz* this);
extern void BgIceTurara_Break(BgIceTurara* this, PlayState* play, f32 arg2);

// =============================================================================
// Static Data
// =============================================================================

// Initial flags only; BallChain_UpdateCollider rewrites toucher.dmgFlags every frame
// to switch between DMG_HAMMER_SWING (overhead) and DMG_HAMMER_JUMP (ground level).
static ColliderCylinderInit sBallChainColInit = { { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER, AC_NONE,
                                                    OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
                                                  { ELEMTYPE_UNK2,
                                                    { DMG_HAMMER_SWING, 0, BALLCHAIN_DAMAGE },
                                                    { 0, 0, 0 },
                                                    TOUCH_ON | TOUCH_SFX_NORMAL,
                                                    BUMP_NONE,
                                                    OCELEM_NONE },
                                                  { BALLCHAIN_COL_RADIUS, BALLCHAIN_COL_HEIGHT, 0, { 0, 0, 0 } } };

static u8 sBallChainColInitialized = 0;
static s8 sBallChainPrevInvinc = 0;
static u8 sBallChainThrownFirstFrame = 0;

// =============================================================================
// Pose Functions
// =============================================================================

static void BallChain_ResetPose(Player* p) {
    p->upperLimbRot.x = 0;
    p->upperLimbRot.y = 0;
    p->upperLimbRot.z = 0;
}

static void BallChain_SetEquipPose(Player* p) {
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].x = BC_EQUIP_L_SHOULDER_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].y = BC_EQUIP_L_SHOULDER_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].z = BC_EQUIP_L_SHOULDER_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].x = BC_EQUIP_L_FOREARM_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].y = BC_EQUIP_L_FOREARM_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].z = BC_EQUIP_L_FOREARM_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].x = BC_EQUIP_L_HAND_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].y = BC_EQUIP_L_HAND_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].z = BC_EQUIP_L_HAND_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].x = BC_EQUIP_R_SHOULDER_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].y = BC_EQUIP_R_SHOULDER_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].z = BC_EQUIP_R_SHOULDER_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].x = BC_EQUIP_R_FOREARM_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].y = BC_EQUIP_R_FOREARM_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].z = BC_EQUIP_R_FOREARM_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].x = BC_EQUIP_R_HAND_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].y = BC_EQUIP_R_HAND_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].z = BC_EQUIP_R_HAND_Z;
    p->upperLimbRot.x = 0;
    p->upperLimbRot.y = 0;
    p->upperLimbRot.z = 0;
}

static void BallChain_SetSpinPose(Player* p, f32 stickX, f32 stickY) {
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].x = BC_SPIN_L_SHOULDER_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].y = BC_SPIN_L_SHOULDER_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER].z = BC_SPIN_L_SHOULDER_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].x = BC_SPIN_L_FOREARM_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].y = BC_SPIN_L_FOREARM_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM].z = BC_SPIN_L_FOREARM_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].x = BC_SPIN_L_HAND_X;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].y = BC_SPIN_L_HAND_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_L_HAND].z = BC_SPIN_L_HAND_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].x = BC_SPIN_R_SHOULDER_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].y = BC_SPIN_R_SHOULDER_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_SHOULDER].z = BC_SPIN_R_SHOULDER_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].x = BC_SPIN_R_FOREARM_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].y = BC_SPIN_R_FOREARM_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_FOREARM].z = BC_SPIN_R_FOREARM_Z;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].x = BC_SPIN_R_HAND_X;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].y = BC_SPIN_R_HAND_Y;
    p->skelAnime.jointTable[PLAYER_LIMB_R_HAND].z = BC_SPIN_R_HAND_Z;
    p->upperLimbRot.x = (s16)(-stickY * BALLCHAIN_LEAN_MULT);
    p->upperLimbRot.y = 0;
    p->upperLimbRot.z = (s16)(stickX * BALLCHAIN_LEAN_MULT);
}

// =============================================================================
// Collider Functions
// =============================================================================

static void BallChain_InitCollider(PlayState* play, Player* p) {
    if (sBallChainColInitialized)
        return;
    Collider_InitCylinder(play, &bcCollider);
    Collider_SetCylinder(play, &bcCollider, &p->actor, &sBallChainColInit);
    sBallChainColInitialized = 1;
}

static void BallChain_UpdateCollider(PlayState* play, Player* p, Vec3f* pos) {
    // Switch hammer damage type based on where the ball is striking:
    //   ball overhead (in the air)     -> DMG_HAMMER_SWING
    //   ball at/near player feet level -> DMG_HAMMER_JUMP (hammer floor)
    f32 heightAbovePlayer = pos->y - p->actor.world.pos.y;
    bcCollider.info.toucher.dmgFlags = (heightAbovePlayer < 30.0f) ? DMG_HAMMER_JUMP : DMG_HAMMER_SWING;
    bcCollider.info.toucher.damage = BALLCHAIN_DAMAGE;
    bcCollider.info.toucher.effect = 0;
    bcCollider.info.toucherFlags = TOUCH_ON | TOUCH_SFX_NORMAL;

    bcCollider.dim.pos.x = (s16)pos->x;
    bcCollider.dim.pos.y = (s16)(pos->y - (BALLCHAIN_COL_HEIGHT / 2));
    bcCollider.dim.pos.z = (s16)pos->z;
    bcCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER;
    CollisionCheck_SetAT(play, &play->colChkCtx, &bcCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &bcCollider.base);
}

// =============================================================================
// Hit Detection
// =============================================================================

static void BallChain_CheckHit(Vec3f* pos) {
    if (bcCollider.base.atFlags & AT_HIT) {
        Audio_PlaySoundGeneral(BALLCHAIN_SFX_HIT, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultReverb);
        bcCollider.base.atFlags &= ~AT_HIT;
    }
}

// Motion trail (EffectBlure) for the spinning/flying ball — a subtle cool-white streak, fed SPARSER
// than the sword (every 2nd frame + shorter duration) so it reads as heavy metal, not a blade. Same
// EFFECT_BLURE2 / EffectBlure_AddVertex system as the Gerudo scimitar trail (mm_player_form). SoH's
// EffectBlureInit2 has the trailType field. Skijer's NEI
static void BallChain_FeedTrail(PlayState* play, Vec3f* ballPos) {
    Vec3f tip, base;

    if (!bcTrailActive) {
        EffectBlureInit2 init = {
            0,                      // calcMode
            8,                      // flags
            0,                      // addAngleChange
            { 255, 255, 255, 255 }, // p1StartColor (white — clearly visible)
            { 200, 220, 255, 128 }, // p2StartColor (cool tint, softer edge)
            { 255, 255, 255, 0 },   // p1EndColor
            { 200, 220, 255, 0 },   // p2EndColor
            4,                      // elemDuration
            0,                      // unkFlag
            2,                      // drawMode (smooth strip)
            0,                      // mode4Param
            { 235, 235, 245, 200 }, // altPrimColor
            { 180, 190, 210, 96 },  // altEnvColor
            TRAIL_TYPE_SWORDS,      // trailType (SoH-only field)
        };
        Effect_Add(play, &bcTrailIndex, EFFECT_BLURE2, 0, 0, &init);
        bcTrailActive = 1;
        bcTrailTick = 0;
    }

    // Feed a segment EVERY frame (like the sword) so consecutive vertices form a continuous strip.
    // Feeding sparser left fewer than 2 live elements at a time, so the blure drew nothing — the
    // subtler-than-sword look comes from the short elemDuration + softer alpha instead. Skijer's NEI
    tip = *ballPos;
    tip.y += 14.0f;
    base = *ballPos;
    base.y -= 14.0f;
    EffectBlure_AddVertex((EffectBlure*)Effect_GetByIndex(bcTrailIndex), &tip, &base);
}

static void BallChain_KillTrail(PlayState* play) {
    if (bcTrailActive) {
        Effect_Delete(play, bcTrailIndex);
        bcTrailActive = 0;
    }
    bcTrailIndex = -1;
}

// Helper: Drop all Goron Pot (Bg_Spot18_Basket) rewards and destroy
static void BallChain_DestroyGoronPot(PlayState* play, Actor* actor) {
    static s16 sDropAngles[] = { -0x0FA0, 0x0320, 0x0FA0 };
    Vec3f dropPos;
    EnItem00* collectible;
    s32 i;

    dropPos.x = actor->world.pos.x;
    dropPos.y = actor->world.pos.y + 170.0f;
    dropPos.z = actor->world.pos.z;

    // Drop ALL rewards (bombs, rupees, heart piece) at once
    // unk_218=0: Bombs
    for (i = 0; i < 3; i++) {
        collectible = Item_DropCollectible(play, &dropPos, ITEM00_BOMBS_A);
        if (collectible != NULL) {
            collectible->actor.velocity.y = 11.0f;
            collectible->actor.world.rot.y = sDropAngles[i] + 0x2000;
        }
    }
    // unk_218=1: Green rupees
    for (i = 0; i < 3; i++) {
        collectible = Item_DropCollectible(play, &dropPos, ITEM00_RUPEE_GREEN);
        if (collectible != NULL) {
            collectible->actor.velocity.y = 11.0f;
            collectible->actor.world.rot.y = sDropAngles[i] + 0x4000;
        }
    }
    // unk_218=2: Heart piece (if not collected) + rupees
    if (!Flags_GetCollectible(play, (actor->params & 0x3F))) {
        collectible = Item_DropCollectible(play, &dropPos, ((actor->params & 0x3F) << 8) | ITEM00_HEART_PIECE);
        if (collectible != NULL) {
            collectible->actor.velocity.y = 11.0f;
            collectible->actor.world.rot.y = sDropAngles[1];
        }
    } else {
        collectible = Item_DropCollectible(play, &dropPos, ITEM00_RUPEE_PURPLE);
        if (collectible != NULL) {
            collectible->actor.velocity.y = 11.0f;
            collectible->actor.world.rot.y = sDropAngles[1];
        }
    }
    collectible = Item_DropCollectible(play, &dropPos, ITEM00_RUPEE_RED);
    if (collectible != NULL) {
        collectible->actor.velocity.y = 11.0f;
        collectible->actor.world.rot.y = sDropAngles[0] + 0x6000;
    }
    collectible = Item_DropCollectible(play, &dropPos, ITEM00_RUPEE_BLUE);
    if (collectible != NULL) {
        collectible->actor.velocity.y = 11.0f;
        collectible->actor.world.rot.y = sDropAngles[2] + 0x6000;
    }

    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    Audio_PlaySoundGeneral(NA_SE_EV_POT_BROKEN, &actor->world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

    // Kill the child lid actor if present
    if (actor->child != NULL) {
        actor->child->parent = NULL;
        Actor_Kill(actor->child);
    }
    Actor_Kill(actor);
}

// Helper: Destroy Shadow Temple pot (Bg_Haka_Tubo) with rewards
static void BallChain_DestroyShadowPot(PlayState* play, Actor* actor) {
    static Vec3f sZeroVector = { 0.0f, 0.0f, 0.0f };
    BgHakaTubo* pot = (BgHakaTubo*)actor;
    Vec3f pos, spawnPos;
    EnItem00* collectible;
    s32 i;
    s32 collectibleParams;
    f32 rnd;

    pos.x = actor->world.pos.x;
    pos.z = actor->world.pos.z;
    pos.y = actor->world.pos.y + 80.0f;

    // Explosion effect
    EffectSsBomb2_SpawnLayered(play, &pos, &sZeroVector, &sZeroVector, 100, 45);
    SoundSource_PlaySfxAtFixedWorldPos(play, &actor->world.pos, 50, NA_SE_EV_BOX_BREAK);
    EffectSsHahen_SpawnBurst(play, &pos, 20.0f, 0, 350, 100, 50, OBJECT_HAKA_OBJECTS, 40, gEffFragments2DL);

    // Drop collectibles
    spawnPos.x = actor->world.pos.x;
    spawnPos.y = actor->world.pos.y + 200.0f;
    spawnPos.z = actor->world.pos.z;

    if (actor->room == 12) {
        // 3 spinning pots room - drop rupees (simulating all 3 pots destroyed)
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
        for (i = 0; i < 9; i++) {
            collectible = Item_DropCollectible(play, &spawnPos, i % 3);
            if (collectible != NULL) {
                collectible->actor.velocity.y = 15.0f;
                collectible->actor.world.rot.y = actor->shape.rot.y + (i * 0x1C71);
            }
        }
    } else {
        // Small key pot
        if (Flags_GetCollectible(play, actor->params) != 0) {
            // Key already collected - drop heart
            if (!CVarGetInteger(CVAR_ENHANCEMENT("NoHeartDrops"), 0)) {
                collectible = Item_DropCollectible(play, &spawnPos, ITEM00_HEART);
                if (collectible != NULL) {
                    collectible->actor.velocity.y = 15.0f;
                    collectible->actor.world.rot.y = actor->shape.rot.y;
                }
            }
            Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
        } else {
            // Drop small key
            collectible = Item_DropCollectible(play, &spawnPos, ((actor->params & 0x3F) << 8) | ITEM00_SMALL_KEY);
            if (collectible != NULL) {
                collectible->actor.velocity.y = 15.0f;
                collectible->actor.world.rot.y = actor->shape.rot.y;
            }
            Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
        }
    }

    Actor_Kill(actor);
}

static void BallChain_CheckDestructibles(PlayState* play, Vec3f* ballPos) {
    Actor* actor;
    Actor* next;
    f32 dist;
    f32 checkRadius = BALLCHAIN_COL_RADIUS + 40.0f;
    f32 potCheckRadius = BALLCHAIN_COL_RADIUS + 80.0f; // Larger radius for pots

    for (actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = next) {
        next = actor->next;

        if (actor->id == ACTOR_BG_ICE_SHELTER) {
            // Use the red ice's actual cylinder dimensions for detection instead of a
            // fixed small radius. The old sphere check required aiming at the origin
            // point even for large ice types (like King Zora's ice with radius=100, height=200).
            BgIceShelter* ice = (BgIceShelter*)actor;
            f32 iceRadius = (f32)ice->cylinder1.dim.radius + BALLCHAIN_COL_RADIUS;
            f32 iceHeight = (f32)ice->cylinder1.dim.height;
            f32 dx = ballPos->x - actor->world.pos.x;
            f32 dy = ballPos->y - actor->world.pos.y;
            f32 dz = ballPos->z - actor->world.pos.z;
            f32 xzDist = sqrtf(SQ(dx) + SQ(dz));

            if (xzDist < iceRadius && dy > -BALLCHAIN_COL_RADIUS && dy < iceHeight + BALLCHAIN_COL_RADIUS) {
                BgIceShelter_ShatterMelt(actor, play);
            }
        }
        // Shadow Temple spinning pot
        else if (actor->id == ACTOR_BG_HAKA_TUBO) {
            dist = Math_Vec3f_DistXYZ(ballPos, &actor->world.pos);
            if (dist < potCheckRadius) {
                BallChain_DestroyShadowPot(play, actor);
            }
        }
    }

    // Iron objects and Goron pot are in ACTORCAT_PROP
    for (actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = next) {
        next = actor->next;

        if (actor->id == ACTOR_BG_JYA_IRONOBJ) {
            dist = Math_Vec3f_DistXYZ(ballPos, &actor->world.pos);
            if (dist < checkRadius) {
                BgJyaIronobj_DestroyInstantly(actor, play);
            }
        }
        // Goron City spinning pot
        else if (actor->id == ACTOR_BG_SPOT18_BASKET) {
            dist = Math_Vec3f_DistXYZ(ballPos, &actor->world.pos);
            if (dist < potCheckRadius) {
                BallChain_DestroyGoronPot(play, actor);
            }
        }
        // Ice Cavern icicles — proximity so the fast throw doesn't tunnel past. Stalagmites break on
        // an AC hit (native break + item drop); hanging stalactites shatter directly. Skijer's NEI
        else if (actor->id == ACTOR_BG_ICE_TURARA) {
            dist = Math_Vec3f_DistXYZ(ballPos, &actor->world.pos);
            if (dist < BALLCHAIN_ICE_REACH) {
                BgIceTurara* tur = (BgIceTurara*)actor;
                if (tur->dyna.actor.params == TURARA_STALAGMITE) {
                    tur->collider.base.acFlags |= AC_HIT; // native break + drop
                } else {
                    BgIceTurara_Break(tur, play, 40.0f);
                    Actor_Kill(actor);
                }
            }
        }
    }

    // Freezards live in ACTORCAT_ENEMY — drive their native fire-melt (melt + loot drop) by proximity
    // so the fast throw/retract can't tunnel past them. No fire flag needed. Skijer's NEI
    for (actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; actor != NULL; actor = next) {
        next = actor->next;
        if (actor->id == ACTOR_EN_FZ) {
            EnFz* fz = (EnFz*)actor;
            dist = Math_Vec3f_DistXYZ(ballPos, &actor->world.pos);
            if ((dist < BALLCHAIN_ICE_REACH) && (fz->state != 3)) { // state 3 = already melting
                EnFz_SetupMelt(fz);
            }
        }
    }
}

static void BallChain_ApplyDamageBonus(PlayState* play) {
    Actor* hit;

    if (!(bcCollider.base.atFlags & AT_HIT))
        return;

    hit = bcCollider.base.at;
    if (hit == NULL || hit->update == NULL)
        return;

    if (hit->id == ACTOR_EN_ST || hit->id == ACTOR_EN_FZ) {
        if (hit->colChkInfo.health > 0) {
            hit->colChkInfo.health -= BALLCHAIN_DAMAGE;
            if (hit->colChkInfo.health < 0) {
                hit->colChkInfo.health = 0;
            }
        }
    }
}

// =============================================================================
// Core Functions
// =============================================================================

static void BallChain_ApplySpeedPenalty(Player* p) {
    p->actor.speedXZ *= BALLCHAIN_SPEED_MULT;
    p->linearVelocity *= BALLCHAIN_SPEED_MULT;
}

// HARD INTERRUPTS — StateSpinning/StateThrown pin Link's speed to 0 and re-stamp his yaw EVERY
// frame, so anything that takes control away from him MUST drop the item or he stays frozen and
// softlocks. ItemInput_CheckDamage only fires on a POSITIVE invincibilityTimer edge, but a real hit
// drives the timer NEGATIVE for the whole damage/knockback reaction (z64player.h: "negative are
// invulnerability") and it may never go positive — so a big knockback slipped past it entirely.
// Skijer's NEI
static u8 BallChain_ShouldInterrupt(Player* p, PlayState* play) {
    if (p->invincibilityTimer < 0) { // damage / knockback reaction in progress
        return 1;
    }
    if (p->stateFlags1 & PLAYER_STATE1_IN_WATER) { // fell in water / swimming — let go
        return 1;
    }
    if (Player_InBlockingCsMode(play, p)) { // cutscene / forced state
        return 1;
    }
    return 0;
}

static void BallChain_Stop(Player* p, PlayState* play) {
    if (bcFirstPerson) {
        FirstPerson_Exit(p, play);
        bcFirstPerson = 0;
    }
    bcCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    bcActive = 0;
    bcState = BALLCHAIN_STATE_INACTIVE;
    bcCharge = 0;
    bcSpinAngle = 0;
    sBallChainThrownFirstFrame = 0;
    // TP ballistic-throw state. Skijer's NEI
    bcPhase = BALLCHAIN_PHASE_FLY;
    bcBounces = 0;
    bcRestTimer = 0;
    bcBallVel.x = bcBallVel.y = bcBallVel.z = 0.0f;
    BallChain_KillTrail(play); // drop the motion streak — Skijer's NEI
    BallChain_ResetPose(p);
    // The states above pin playSpeed at 0 every frame; if we let go mid-freeze (knockback, water)
    // Link's animation would stay stuck. Hand it back so he can move again. Skijer's NEI
    p->skelAnime.playSpeed = 1.0f;
    // Stop looping sounds
    Audio_StopSfxById(NA_SE_IT_SWORD_SWING);
    Audio_StopSfxById(NA_SE_PL_WALK_GROUND);
    ItemEquip_PlayUnequipSFX(play, p);
}

static void BallChain_Start(Player* p, PlayState* play) {
    if (bcActive)
        return;
    bcActive = 1;
    bcCharge = 0;
    bcSpinAngle = 0;
    bcFirstPerson = 0;
    bcState = BALLCHAIN_STATE_EQUIP;
    ItemEquip_PlayEquipSFX(play, p);
}

// =============================================================================
// State: Equip
// =============================================================================

static void StateEquip(Player* p, PlayState* play, ItemInputState* in) {
    s16 yaw = p->actor.shape.rot.y;
    Vec3f* leftHand = &p->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f* rightHand = &p->bodyPartsPos[PLAYER_BODYPART_R_HAND];

    // Clear the motion streak once the ball is back in the hand (covers throw-return + timeout). Skijer's NEI
    if (bcTrailActive) {
        BallChain_KillTrail(play);
    }

    BallChain_ApplySpeedPenalty(p);
    p->skelAnime.playSpeed = 0.0f;
    BallChain_SetEquipPose(p);

    // TWO-HANDED grip: the ball sits at the MIDPOINT of both hands (same point the chain is drawn
    // from in CustomItems_DrawBallChain), so it stays centered between Link's hands. Skijer's NEI
    bcBallPos.x = (leftHand->x + rightHand->x) * 0.5f;
    bcBallPos.y = (leftHand->y + rightHand->y) * 0.5f + BALLCHAIN_EQUIP_Y_OFFSET;
    bcBallPos.z = (leftHand->z + rightHand->z) * 0.5f;

    // Collider stays live even while just holding the ball (TP style — it's always a weapon), so it
    // keeps hitting whatever it contacts through the whole cycle, including right after it returns to
    // the hand. Contact-only here (no ranged ice sweep while idle). Skijer's NEI
    BallChain_UpdateCollider(play, p, &bcBallPos);
    BallChain_CheckHit(&bcBallPos);

    if (in->isPressed) {
        bcState = BALLCHAIN_STATE_SPINNING;
        bcCharge = 0;
        bcThrowYaw = yaw;
        Audio_PlaySoundGeneral(BALLCHAIN_SFX_SWING, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// =============================================================================
// State: Spinning
// =============================================================================

static void StateSpinning(Player* p, PlayState* play, ItemInputState* in) {
    s8 rawStickX = play->state.input[0].cur.stick_x;
    s8 rawStickY = play->state.input[0].cur.stick_y;
    u8 isZTarget = Player_IsZTargeting(p);
    f32 stickX = 0.0f;
    f32 stickY = 0.0f;
    f32 stickMag, chargeRatio, spinHeight;
    f32 orbitX, orbitZ, orbitY, heightMod, sideMod;
    s16 spinSpeed, yaw;

    // Heavy movement: Link can shuffle SLOWLY while spinning (MM feel), except while aiming in first
    // person. The multiplier caps whatever speed the normal player movement built up this frame. Skijer's NEI
    if (bcFirstPerson) {
        p->actor.speedXZ = 0.0f;
        p->linearVelocity = 0.0f;
        p->skelAnime.playSpeed = 0.0f;
    } else {
        p->actor.speedXZ *= BALLCHAIN_SPIN_WALK_MULT;
        p->linearVelocity *= BALLCHAIN_SPIN_WALK_MULT;
        p->skelAnime.playSpeed = (fabsf(p->linearVelocity) > 0.3f) ? 0.5f : 0.0f;
    }

    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_CUP)) {
        if (bcFirstPerson) {
            FirstPerson_Exit(p, play);
            bcFirstPerson = 0;
        } else {
            FirstPerson_Init(p, play);
            bcFirstPerson = 1;
        }
    }

    if (bcFirstPerson) {
        FirstPerson_Update(p, play);
    }

    if (isZTarget && p->focusActor != NULL) {
        bcThrowYaw = Math_Vec3f_Yaw(&p->actor.world.pos, &p->focusActor->focus.pos);
    }
    p->actor.shape.rot.y = bcThrowYaw;
    p->actor.world.rot.y = bcThrowYaw;
    p->yaw = bcThrowYaw;

    stickMag = sqrtf(SQ(rawStickX) + SQ(rawStickY));
    if (!isZTarget && stickMag > BALLCHAIN_STICK_DEADZONE) {
        stickX = (f32)rawStickX / 127.0f;
        stickY = (f32)rawStickY / 127.0f;
    }

    if (bcCharge < BALLCHAIN_CHARGE_MAX) {
        bcCharge++;
    }

    chargeRatio = (f32)bcCharge / (f32)BALLCHAIN_CHARGE_MAX;
    spinSpeed = (s16)(BALLCHAIN_SPIN_SPEED_MIN + (BALLCHAIN_SPIN_SPEED_MAX - BALLCHAIN_SPIN_SPEED_MIN) * chargeRatio);
    bcSpinAngle += spinSpeed;
    spinHeight = BALLCHAIN_SPIN_HEIGHT_MIN + (BALLCHAIN_SPIN_HEIGHT_MAX - BALLCHAIN_SPIN_HEIGHT_MIN) * chargeRatio;

    orbitX = Math_SinS(bcSpinAngle) * BALLCHAIN_SPIN_RADIUS;
    orbitZ = Math_CosS(bcSpinAngle) * BALLCHAIN_SPIN_RADIUS;
    orbitY = spinHeight;

    heightMod = -Math_CosS(bcSpinAngle) * stickY * BALLCHAIN_LEAN_TILT;
    sideMod = -Math_SinS(bcSpinAngle) * stickX * BALLCHAIN_LEAN_TILT;
    orbitY += heightMod + sideMod;

    yaw = p->actor.shape.rot.y;
    bcBallPos.x = p->actor.world.pos.x + (orbitX * Math_CosS(yaw) + orbitZ * Math_SinS(yaw));
    bcBallPos.y = p->actor.world.pos.y + orbitY;
    bcBallPos.z = p->actor.world.pos.z + (-orbitX * Math_SinS(yaw) + orbitZ * Math_CosS(yaw));

    BallChain_SetSpinPose(p, stickX, stickY);

    BallChain_UpdateCollider(play, p, &bcBallPos);
    BallChain_CheckDestructibles(play, &bcBallPos);
    BallChain_CheckHit(&bcBallPos);
    BallChain_ApplyDamageBonus(play);
    BallChain_FeedTrail(play, &bcBallPos); // spin streak — Skijer's NEI

    Actor_PlaySfx_Flagged(&p->actor, BALLCHAIN_SFX_WHOOSH);

    if (!in->isHeld) {
        // RELEASE: violent ballistic launch in the aimed direction (TP arc). Skijer's NEI
        f32 launchSpeed =
            BALLCHAIN_LAUNCH_SPEED_MIN + (BALLCHAIN_LAUNCH_SPEED_MAX - BALLCHAIN_LAUNCH_SPEED_MIN) * chargeRatio;
        s16 throwYaw = bcThrowYaw;
        s16 throwPitch = 0;

        if (bcFirstPerson) {
            throwYaw = FirstPerson_GetAimYaw(p);
            throwPitch = FirstPerson_GetAimPitch(p);
            FirstPerson_Exit(p, play);
            bcFirstPerson = 0;
        } else if (isZTarget && p->focusActor != NULL) {
            throwYaw = Math_Vec3f_Yaw(&p->actor.world.pos, &p->focusActor->focus.pos);
            throwPitch = 0;
        } else {
            throwYaw = p->actor.shape.rot.y + (s16)(stickX * BALLCHAIN_THROW_YAW_MAX);
            throwPitch = (s16)(-stickY * BALLCHAIN_THROW_PITCH_MAX);
        }

        // Launch origin: over Link's shoulder, slightly forward.
        bcBallPos.x = p->actor.world.pos.x + Math_SinS(throwYaw) * 20.0f;
        bcBallPos.y = p->actor.world.pos.y + 45.0f;
        bcBallPos.z = p->actor.world.pos.z + Math_CosS(throwYaw) * 20.0f;

        if (isZTarget && p->focusActor != NULL && p->focusActor->update != NULL) {
            // Lock-on: ballistic lead — aim the arc so gravity drops the ball ON the target.
            Vec3f* tPos = &p->focusActor->focus.pos;
            f32 dx = tPos->x - bcBallPos.x;
            f32 dy = tPos->y - bcBallPos.y;
            f32 dz = tPos->z - bcBallPos.z;
            f32 flightT = sqrtf(SQ(dx) + SQ(dz)) / launchSpeed;

            if (flightT < 1.0f) {
                flightT = 1.0f;
            }
            bcBallVel.x = dx / flightT;
            bcBallVel.z = dz / flightT;
            bcBallVel.y = (dy / flightT) - (0.5f * BALLCHAIN_GRAVITY * flightT); // gravity compensation
            throwYaw = Math_Vec3f_Yaw(&bcBallPos, tPos);
        } else {
            bcBallVel.x = Math_SinS(throwYaw) * Math_CosS(throwPitch) * launchSpeed;
            bcBallVel.z = Math_CosS(throwYaw) * Math_CosS(throwPitch) * launchSpeed;
            // Positive pitch aims down (same convention as FirstPerson_GetAimPitch).
            bcBallVel.y = BALLCHAIN_LAUNCH_VY - Math_SinS(throwPitch) * launchSpeed;
        }

        bcThrowYaw = throwYaw;
        bcThrowPitch = throwPitch;
        bcState = BALLCHAIN_STATE_THROWN;
        bcPhase = BALLCHAIN_PHASE_FLY;
        bcBounces = 0;
        bcRestTimer = 0;
        bcCharge = 0; // reused as the thrown-safety frame counter
        sBallChainThrownFirstFrame = 0;

        ItemVoice_Play(p, BALLCHAIN_SFX_VOICE_ADULT, BALLCHAIN_SFX_VOICE_CHILD);
        Audio_PlaySoundGeneral(BALLCHAIN_SFX_SWING, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// =============================================================================
// State: Thrown
// =============================================================================

// TP thrown ball: real gravity arc -> hard floor bounces (thud) -> wall RICOCHET (reflect off the
// wall normal) -> rest a beat -> retract along the chain. Link is braced the whole time. Skijer's NEI
static void StateThrown(Player* p, PlayState* play) {
    f32 dist, dx, dy, dz, norm;
    CollisionPoly* poly = NULL;
    Vec3f prevPos, resultPos;

    p->actor.speedXZ = 0.0f;
    p->linearVelocity = 0.0f;
    p->skelAnime.playSpeed = 0.0f;

    p->actor.shape.rot.y = bcThrowYaw;
    p->actor.world.rot.y = bcThrowYaw;
    p->yaw = bcThrowYaw;

    BallChain_SetSpinPose(p, 0.0f, 0.0f);
    p->upperLimbRot.x = BALLCHAIN_THROW_LEAN;

    // Hard safety: never leave Link braced forever (bcCharge = thrown frame counter).
    bcCharge++;
    if (bcCharge > BALLCHAIN_THROWN_TIMEOUT) {
        bcState = BALLCHAIN_STATE_EQUIP;
        return;
    }

    if (bcPhase == BALLCHAIN_PHASE_FLY) {
        CollisionPoly* floorPoly = NULL;
        s32 bgId;
        Vec3f probe;
        f32 floorY, xzDist;

        prevPos = bcBallPos;

        // Heavy ballistic integration (20fps logic frames).
        bcBallVel.y += BALLCHAIN_GRAVITY;
        if (bcBallVel.y < -BALLCHAIN_TERMINAL_VY) {
            bcBallVel.y = -BALLCHAIN_TERMINAL_VY;
        }
        bcBallPos.x += bcBallVel.x;
        bcBallPos.y += bcBallVel.y;
        bcBallPos.z += bcBallVel.z;

        // Chain taut: the ball can never fly past the chain length.
        dx = bcBallPos.x - p->actor.world.pos.x;
        dz = bcBallPos.z - p->actor.world.pos.z;
        xzDist = sqrtf(SQ(dx) + SQ(dz));
        if (xzDist > BALLCHAIN_CHAIN_MAX) {
            f32 clamp = BALLCHAIN_CHAIN_MAX / xzDist;

            bcBallPos.x = p->actor.world.pos.x + dx * clamp;
            bcBallPos.z = p->actor.world.pos.z + dz * clamp;
            bcBallVel.x = 0.0f;
            bcBallVel.z = 0.0f;
        }

        // Wall hit: RICOCHET — reflect the horizontal velocity off the wall normal (TP feel) instead
        // of dropping dead against it. Metal clank on a solid hit. Skijer's NEI
        resultPos = bcBallPos;
        if (BgCheck_EntitySphVsWall1(&play->colCtx, &resultPos, &bcBallPos, &prevPos, BALLCHAIN_WALL_RADIUS, &poly,
                                     BALLCHAIN_WALL_HEIGHT)) {
            bcBallPos = resultPos;
            if (fabsf(bcBallVel.x) + fabsf(bcBallVel.z) > 1.0f) {
                Audio_PlaySoundGeneral(BALLCHAIN_SFX_WALL_BOUNCE, &bcBallPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
            if (poly != NULL) {
                // v' = v - 2(v·n)n, then damp — reflect XZ across the wall's horizontal normal.
                f32 nx = COLPOLY_GET_NORMAL(poly->normal.x);
                f32 nz = COLPOLY_GET_NORMAL(poly->normal.z);
                f32 dot = (bcBallVel.x * nx) + (bcBallVel.z * nz);

                bcBallVel.x = (bcBallVel.x - (2.0f * dot * nx)) * BALLCHAIN_WALL_BOUNCE_FACTOR;
                bcBallVel.z = (bcBallVel.z - (2.0f * dot * nz)) * BALLCHAIN_WALL_BOUNCE_FACTOR;
            } else {
                bcBallVel.x = 0.0f;
                bcBallVel.z = 0.0f;
            }
        }

        // Ground bounce: heavy thud, invert velocity.y, up to MAX_BOUNCES, then rest before retract.
        probe = bcBallPos;
        probe.y += 20.0f;
        floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &floorPoly, &bgId, &p->actor, &probe);
        if ((floorY > BGCHECK_Y_MIN) && (bcBallPos.y - BALLCHAIN_BALL_RADIUS <= floorY) && (bcBallVel.y <= 0.0f)) {
            bcBallPos.y = floorY + BALLCHAIN_BALL_RADIUS;
            bcBounces++;

            Audio_PlaySoundGeneral(BALLCHAIN_SFX_HIT, &bcBallPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

            if ((bcBounces >= BALLCHAIN_MAX_BOUNCES) || (fabsf(bcBallVel.y) < 3.0f)) {
                bcBallVel.x = bcBallVel.y = bcBallVel.z = 0.0f;
                bcPhase = BALLCHAIN_PHASE_REST;
                bcRestTimer = BALLCHAIN_REST_FRAMES;
            } else {
                bcBallVel.y = -bcBallVel.y * BALLCHAIN_BOUNCE_FACTOR;
                bcBallVel.x *= BALLCHAIN_BOUNCE_XZ_KEEP;
                bcBallVel.z *= BALLCHAIN_BOUNCE_XZ_KEEP;
            }
        } else if (bcBallPos.y < p->actor.world.pos.y - 500.0f) {
            // Thrown into the void — just reel it back in.
            bcPhase = BALLCHAIN_PHASE_RETRACT;
            bcRestTimer = 0;
        }
    } else if (bcPhase == BALLCHAIN_PHASE_REST) {
        bcRestTimer--;
        if (bcRestTimer <= 0) {
            bcPhase = BALLCHAIN_PHASE_RETRACT;
            bcRestTimer = 0;
        }
    } else { // BALLCHAIN_PHASE_RETRACT
        dist = Math_Vec3f_DistXYZ(&bcBallPos, &p->actor.world.pos);

        if (dist > BALLCHAIN_RETURN_DIST) {
            dx = p->actor.world.pos.x - bcBallPos.x;
            dy = (p->actor.world.pos.y + 45.0f) - bcBallPos.y;
            dz = p->actor.world.pos.z - bcBallPos.z;
            norm = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));

            if (norm > 0.1f) {
                norm = BALLCHAIN_RETRACT_SPEED / norm;
                bcBallPos.x += dx * norm;
                bcBallPos.y += dy * norm;
                bcBallPos.z += dz * norm;
            }
            // Retract clink — Actor_PlaySfx_Flagged is a flagged (auto-stopping) sfx, so no lingering loop.
            Actor_PlaySfx_Flagged(&p->actor, BALLCHAIN_SFX_RETRACT);
        } else {
            bcState = BALLCHAIN_STATE_EQUIP;
            return;
        }
    }

    BallChain_UpdateCollider(play, p, &bcBallPos);
    BallChain_CheckDestructibles(play, &bcBallPos);
    BallChain_CheckHit(&bcBallPos);
    BallChain_ApplyDamageBonus(play);
    if (bcPhase != BALLCHAIN_PHASE_REST) {
        BallChain_FeedTrail(play,
                            &bcBallPos); // streak while airborne (fly + retract), not while resting — Skijer's NEI
    }
}

// =============================================================================
// Public API
// =============================================================================

void Handle_BallAndChain(Player* p, PlayState* play) {
    ItemInputState in;

    if (!sBallChainColInitialized) {
        BallChain_InitCollider(play, p);
    }

    ItemInput_Update(&in, ITEM_BALL_AND_CHAIN, p, play);

    if (!in.wasEquipped || ItemInput_IsBlocked(p, play) || ItemInput_CheckDamage(p, &sBallChainPrevInvinc) ||
        BallChain_ShouldInterrupt(p, play)) {
        if (bcActive)
            BallChain_Stop(p, play);
        return;
    }
    if (in.otherButtonPressed) {
        BallChain_Stop(p, play);
        return;
    }

    if (!bcActive) {
        if (in.isPressed || in.isHeld) {
            BallChain_Start(p, play);
        }
        return;
    }

    switch (bcState) {
        case BALLCHAIN_STATE_EQUIP:
            StateEquip(p, play, &in);
            break;
        case BALLCHAIN_STATE_SPINNING:
            StateSpinning(p, play, &in);
            break;
        case BALLCHAIN_STATE_THROWN:
            StateThrown(p, play);
            break;
        default:
            bcState = BALLCHAIN_STATE_EQUIP;
            break;
    }
}

void Player_InitBallAndChainIA(PlayState* play, Player* p) {
    BallChain_InitCollider(play, p);
    bcActive = 0;
    bcCharge = 0;
    bcSpinAngle = 0;
    bcFirstPerson = 0;
    bcState = BALLCHAIN_STATE_INACTIVE;
    bcThrowDist = 0;
    sBallChainThrownFirstFrame = 0;
    // TP ballistic-throw state. Skijer's NEI
    bcPhase = BALLCHAIN_PHASE_FLY;
    bcBounces = 0;
    bcRestTimer = 0;
    bcBallVel.x = bcBallVel.y = bcBallVel.z = 0.0f;
    // Motion trail starts inactive. Skijer's NEI
    bcTrailActive = 0;
    bcTrailIndex = -1;
}
