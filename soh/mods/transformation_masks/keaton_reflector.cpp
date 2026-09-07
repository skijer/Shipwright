#include "keaton_reflector.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" void Player_PlayVoiceSfx(Player* player, u16 sfxId);
extern "C" PlayState* gPlayState;

#define REFLECT_ANIM "__OTR__misc/link_animetion/gPlayerAnim_mhr_damage_idle03_loop"

namespace {

const f32 POP_STEP = 0.28f;  // scale gained per frame while the shield snaps open
const f32 PULSE_MIN = 0.95f; // the idle breath the user asked for, 95% to 100%
const s16 PULSE_PERIOD = 24;

// World units now that the hexagon is drawn in world space: Keaton stands about 30 tall.
const f32 REFLECTOR_HEIGHT = 20.0f;

const f32 REFLECT_RADIUS = 90.0f;
const s16 BURST_RADIUS = 70;
const s16 BURST_HEIGHT = 90;
const u8 BURST_DAMAGE = 8;

const f32 AIR_DRAG = 0.35f; // what is left of the momentum on the frame it opens

enum State {
    STATE_OFF,
    STATE_POPPING,
    STATE_HELD,
};

LinkAnimationHeader* sAnim;
u8 sLoaded = 0;
u8 sState = STATE_OFF;
f32 sScale = 0.0f;
s16 sPulse = 0;

ColliderCylinder sBurst;
u8 sBurstReady = 0;

// The reflector has to be HIT to reflect a beam. A Beamos never spawns a projectile:
// its laser is two quads it owns and stretches to Link, so nothing flies in for the
// sweep to turn around. An AC collider catches that, and base.ac names the attacker.
ColliderCylinder sGuard;
u8 sGuardReady = 0;

ColliderCylinderInit sGuardInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
    { BURST_RADIUS, BURST_HEIGHT, -40, { 0, 0, 0 } },
};

ColliderCylinderInit sBurstInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { DMG_MAGIC_LIGHT, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { BURST_RADIUS, BURST_HEIGHT, -40, { 0, 0, 0 } },
};

// A hollow hexagonal ring: no centre fan at all, so you see straight through the middle.
// Four concentric rings of six, pointy left and right, and the band between each pair is
// what carries the look — saturated outline, near-white body, then a blue glow that
// fades to nothing at the hole. Colour rides the vertices: G_LIGHTING off makes shade
// the vertex colour, and G_CC_SHADE takes it straight through with no texture.
Vtx sHexVtx[24] = {
    VTX(22, 0, 0, 0, 0, 74, 148, 240, 255),     VTX(11, 19, 0, 0, 0, 74, 148, 240, 255),
    VTX(-11, 19, 0, 0, 0, 74, 148, 240, 255),   VTX(-22, 0, 0, 0, 0, 74, 148, 240, 255),
    VTX(-11, -19, 0, 0, 0, 74, 148, 240, 255),  VTX(11, -19, 0, 0, 0, 74, 148, 240, 255),

    VTX(20, 0, 0, 0, 0, 168, 218, 251, 255),    VTX(10, 17, 0, 0, 0, 168, 218, 251, 255),
    VTX(-10, 17, 0, 0, 0, 168, 218, 251, 255),  VTX(-20, 0, 0, 0, 0, 168, 218, 251, 255),
    VTX(-10, -17, 0, 0, 0, 168, 218, 251, 255), VTX(10, -17, 0, 0, 0, 168, 218, 251, 255),

    VTX(14, 0, 0, 0, 0, 150, 206, 250, 255),    VTX(7, 12, 0, 0, 0, 150, 206, 250, 255),
    VTX(-7, 12, 0, 0, 0, 150, 206, 250, 255),   VTX(-14, 0, 0, 0, 0, 150, 206, 250, 255),
    VTX(-7, -12, 0, 0, 0, 150, 206, 250, 255),  VTX(7, -12, 0, 0, 0, 150, 206, 250, 255),

    VTX(11, 0, 0, 0, 0, 120, 190, 248, 0),      VTX(6, 10, 0, 0, 0, 120, 190, 248, 0),
    VTX(-6, 10, 0, 0, 0, 120, 190, 248, 0),     VTX(-11, 0, 0, 0, 0, 120, 190, 248, 0),
    VTX(-6, -10, 0, 0, 0, 120, 190, 248, 0),    VTX(6, -10, 0, 0, 0, 120, 190, 248, 0),
};

Gfx sHexDL[] = {
    gsDPPipeSync(),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPClearGeometryMode(G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    // G_SHADE is the bit that makes vertex colour exist at all: without it the combiner
    // gets whatever shade was left over, which is why the ring came out olive.
    gsSPSetGeometryMode(G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH),
    gsSPVertex(sHexVtx, 24, 0),

    gsSP2Triangles(0, 6, 7, 0, 0, 7, 1, 0),
    gsSP2Triangles(1, 7, 8, 0, 1, 8, 2, 0),
    gsSP2Triangles(2, 8, 9, 0, 2, 9, 3, 0),
    gsSP2Triangles(3, 9, 10, 0, 3, 10, 4, 0),
    gsSP2Triangles(4, 10, 11, 0, 4, 11, 5, 0),
    gsSP2Triangles(5, 11, 6, 0, 5, 6, 0, 0),

    gsSP2Triangles(6, 12, 13, 0, 6, 13, 7, 0),
    gsSP2Triangles(7, 13, 14, 0, 7, 14, 8, 0),
    gsSP2Triangles(8, 14, 15, 0, 8, 15, 9, 0),
    gsSP2Triangles(9, 15, 16, 0, 9, 16, 10, 0),
    gsSP2Triangles(10, 16, 17, 0, 10, 17, 11, 0),
    gsSP2Triangles(11, 17, 12, 0, 11, 12, 6, 0),

    gsSP2Triangles(12, 18, 19, 0, 12, 19, 13, 0),
    gsSP2Triangles(13, 19, 20, 0, 13, 20, 14, 0),
    gsSP2Triangles(14, 20, 21, 0, 14, 21, 15, 0),
    gsSP2Triangles(15, 21, 22, 0, 15, 22, 16, 0),
    gsSP2Triangles(16, 22, 23, 0, 16, 23, 17, 0),
    gsSP2Triangles(17, 23, 18, 0, 17, 18, 12, 0),

    gsSPEndDisplayList(),
};

void Load() {
    if (sLoaded) {
        return;
    }
    sLoaded = 1;
    sAnim = (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(REFLECT_ANIM);
}

// Returns the attack to its sender, landed on focus.pos rather than the feet: that is
// the eye on a Beamos and the head on everything else, which is where a returned beam
// has to arrive. Carries the Deku Nut bit so it stuns rather than only hurting.
void PunishAttacker(PlayState* play, Player* player, Actor* attacker) {
    if ((attacker == nullptr) || (attacker->update == nullptr) || !sBurstReady) {
        return;
    }
    sBurst.info.toucher.dmgFlags = DMG_DEKU_NUT | DMG_MAGIC_LIGHT;
    sBurst.info.toucher.damage = BURST_DAMAGE;
    sBurst.base.atFlags |= AT_ON;
    sBurst.dim.pos.x = (s16)attacker->focus.pos.x;
    sBurst.dim.pos.y = (s16)attacker->focus.pos.y;
    sBurst.dim.pos.z = (s16)attacker->focus.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG2);
}

void SubmitGuard(PlayState* play, Player* player) {
    if (!sGuardReady) {
        Collider_InitCylinder(play, &sGuard);
        Collider_SetCylinder(play, &sGuard, &player->actor, &sGuardInit);
        sGuardReady = 1;
    }

    if (sGuard.base.acFlags & AC_HIT) {
        sGuard.base.acFlags &= ~AC_HIT;
        PunishAttacker(play, player, sGuard.base.ac);
        sGuard.base.ac = nullptr;
    }

    // Link's own cylinder sits inside the guard, so a beam that reaches him hits BOTH:
    // detecting the attack is worth nothing while he still takes it. Standing his AC
    // down for the frame is what makes the reflector actually block.
    player->cylinder.base.acFlags &= ~(AC_ON | AC_HIT);

    sGuard.base.acFlags |= AC_ON;
    sGuard.dim.pos.x = (s16)player->actor.world.pos.x;
    sGuard.dim.pos.y = (s16)player->actor.world.pos.y;
    sGuard.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAC(play, &play->colChkCtx, &sGuard.base);
}

void SubmitBurst(PlayState* play, Player* player) {
    if (!sBurstReady) {
        Collider_InitCylinder(play, &sBurst);
        Collider_SetCylinder(play, &sBurst, &player->actor, &sBurstInit);
        sBurstReady = 1;
    }
    sBurst.info.toucher.dmgFlags = DMG_MAGIC_LIGHT;
    sBurst.info.toucher.damage = BURST_DAMAGE;
    sBurst.base.atFlags |= AT_ON;
    sBurst.dim.pos.x = (s16)player->actor.world.pos.x;
    sBurst.dim.pos.y = (s16)player->actor.world.pos.y;
    sBurst.dim.pos.z = (s16)player->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
}

// A projectile's collider lives in its private struct, but every actor hands a pointer
// to the collision system each frame, so colChkCtx.colAT IS the generic way to reach it.
// Re-aligning it from enemy-damage to player-damage is what makes a returned shot kill
// the thing that fired it. This has to run in the window between the projectile's own
// update (where it registers) and the next CollisionCheck_AT, which is exactly when the
// OnActorUpdate hook fires.
const s32 REFLECTED_MAX = 8;
const s16 REFLECTED_LIFE = 40;

struct Reflected {
    Actor* actor;
    s16 timer;
};

Reflected sReflected[REFLECTED_MAX];
u8 sHookInstalled = 0;

void Remember(Actor* actor) {
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].actor == actor) {
            sReflected[i].timer = REFLECTED_LIFE;
            return;
        }
    }
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer <= 0) {
            sReflected[i].actor = actor;
            sReflected[i].timer = REFLECTED_LIFE;
            return;
        }
    }
}

void TickRemembered() {
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if (sReflected[i].timer > 0) {
            sReflected[i].timer--;
            if (sReflected[i].timer == 0) {
                sReflected[i].actor = nullptr;
            }
        }
    }
}

void RealignColliders(void* actorRef) {
    Actor* actor = (Actor*)actorRef;
    if ((actor == nullptr) || (gPlayState == nullptr)) {
        return;
    }

    u8 known = 0;
    for (s32 i = 0; i < REFLECTED_MAX; i++) {
        if ((sReflected[i].timer > 0) && (sReflected[i].actor == actor)) {
            known = 1;
            break;
        }
    }
    if (!known) {
        return;
    }

    CollisionCheckContext* ctx = &gPlayState->colChkCtx;
    for (s32 i = 0; i < ctx->colATCount; i++) {
        Collider* col = ctx->colAT[i];
        if ((col == nullptr) || (col->actor != actor)) {
            continue;
        }
        col->atFlags &= ~AT_TYPE_ENEMY;
        col->atFlags |= AT_TYPE_PLAYER;
    }
}

void InstallHook() {
    if (sHookInstalled) {
        return;
    }
    sHookInstalled = 1;
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(RealignColliders);
}

// Turns a projectile around and points it back down its own flight path.
void ReflectOne(Actor* actor) {
    actor->world.rot.y += 0x8000;
    actor->shape.rot.y = actor->world.rot.y;
    actor->world.rot.x = -actor->world.rot.x;
    actor->velocity.x = -actor->velocity.x;
    actor->velocity.z = -actor->velocity.z;

    if (actor->parent != nullptr) {
        actor->world.rot.y = Actor_WorldYawTowardActor(actor, actor->parent);
        actor->shape.rot.y = actor->world.rot.y;
    }
}

// Projectiles are scattered across four categories, not one: arrows and magic are
// ITEMACTION, bombs EXPLOSIVE, Deku nuts PROP, and an Octorok's rock is an ENEMY like
// the Octorok itself. The last two lists are full of things that must NOT be turned
// around, so they are held to being airborne and fast.
struct SweepRule {
    s32 category;
    u8 flyingOnly;
};

const SweepRule kSweep[] = {
    { ACTORCAT_ITEMACTION, 0 },
    { ACTORCAT_EXPLOSIVE, 0 },
    { ACTORCAT_PROP, 1 },
    { ACTORCAT_ENEMY, 1 },
};

const f32 PROJECTILE_SPEED = 6.0f;

void ReflectSweep(PlayState* play, Player* player) {
    for (s32 c = 0; c < ARRAY_COUNT(kSweep); c++) {
        for (Actor* it = play->actorCtx.actorLists[kSweep[c].category].head; it != nullptr; it = it->next) {
            if ((it->update == nullptr) || (it == &player->actor)) {
                continue;
            }
            if (kSweep[c].flyingOnly) {
                if ((it->bgCheckFlags & BGCHECKFLAG_GROUND) || (it->speedXZ < PROJECTILE_SPEED)) {
                    continue;
                }
            } else if (it->speedXZ <= 0.0f) {
                continue;
            }

            Vec3f toPlayer;
            f32 dist = Math_Vec3f_DistXYZAndStoreDiff(&it->world.pos, &player->actor.world.pos, &toPlayer);
            if ((dist > REFLECT_RADIUS) || (dist < 0.1f)) {
                continue;
            }

            // Only what is closing in gets turned. This is also what stops a reflected
            // shot being flipped again every frame it stays inside the radius: once it
            // is heading away the test fails on its own, with nothing to remember.
            f32 approach =
                (it->velocity.x * toPlayer.x) + (it->velocity.y * toPlayer.y) + (it->velocity.z * toPlayer.z);
            if (approach <= 0.0f) {
                continue;
            }

            ReflectOne(it);
            Remember(it);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG);
        }
    }
}

void Open(PlayState* play, Player* player) {
    sState = STATE_POPPING;
    sScale = 0.0f;
    sPulse = 0;

    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    if (sAnim != nullptr) {
        LinkAnimation_Change(play, &player->skelAnime, sAnim, 1.0f, 0.0f, Animation_GetLastFrame(sAnim), ANIMMODE_LOOP,
                             -6.0f);
    }

    // Airborne: the shield eats the momentum instead of letting it carry through.
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        player->linearVelocity *= AIR_DRAG;
        player->actor.velocity.y *= AIR_DRAG;
    }

    SubmitBurst(play, player);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG);
}

// Handing the body back needs the clip in a TERMINAL state. OOT only re-issues an
// animation when its action changes, so a loop left running keeps the reflect pose
// forever: parking it on its last frame as ONCE is what lets OOT take the pose back.
void Close(PlayState* play, Player* player) {
    sState = STATE_OFF;
    sScale = 0.0f;
    if (sBurstReady) {
        sBurst.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    if (sGuardReady) {
        sGuard.base.acFlags &= ~(AC_ON | AC_HIT);
        sGuard.base.ac = nullptr;
    }
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;

    if (sAnim != nullptr) {
        f32 last = Animation_GetLastFrame(sAnim);
        LinkAnimation_Change(play, &player->skelAnime, sAnim, 1.0f, last, last, ANIMMODE_ONCE, -4.0f);
    }
}

} // namespace

extern "C" void KeatonReflector_Reset(void) {
    sState = STATE_OFF;
    sScale = 0.0f;
    sPulse = 0;
    if (sBurstReady) {
        sBurst.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    if (sGuardReady) {
        sGuard.base.acFlags &= ~(AC_ON | AC_HIT);
        sGuard.base.ac = nullptr;
    }
}

extern "C" u8 KeatonReflector_IsActive(void) {
    return sState != STATE_OFF;
}

extern "C" u8 KeatonReflector_Update(Player* player, PlayState* play) {
    Load();
    InstallHook();
    TickRemembered();

    Input* input = &play->state.input[0];
    u8 holdingR = CHECK_BTN_ALL(input->cur.button, BTN_R);

    if (sState == STATE_OFF) {
        if (!holdingR ||
            (player->stateFlags1 & (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED))) {
            return 0;
        }
        Open(play, player);
        return 1;
    }

    if (!holdingR || (player->stateFlags1 & PLAYER_STATE1_DAMAGED)) {
        Close(play, player);
        return 0;
    }

    if (sState == STATE_POPPING) {
        sScale += POP_STEP;
        if (sScale >= 1.0f) {
            sScale = 1.0f;
            sState = STATE_HELD;
        }
    } else {
        sPulse = (sPulse + 1) % PULSE_PERIOD;
        sScale = PULSE_MIN + ((1.0f - PULSE_MIN) * (0.5f + (0.5f * Math_CosS(sPulse * (0x10000 / PULSE_PERIOD)))));
    }

    SubmitGuard(play, player);
    ReflectSweep(play, player);
    LinkAnimation_Update(play, &player->skelAnime);

    // damage_idle03 walks its root, and jointTable[0] IS the model-space translation the
    // draw uses — zeroing only the delta left the model itself drifting back. Both have
    // to be pinned to the skeleton's own base.
    if (player->skelAnime.jointTable != nullptr) {
        player->skelAnime.jointTable[0] = player->skelAnime.baseTransl;
        player->skelAnime.prevTransl = player->skelAnime.baseTransl;
    }

    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    player->linearVelocity = 0.0f;
    return 1;
}

extern "C" void KeatonReflector_Draw(PlayState* play, Player* player) {
    if (sState == STATE_OFF) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    // Built from the world position rather than off the limb matrix that is current
    // here: limb space runs in hundreds of units (jointPos ~697), so a 40-unit hexagon
    // drawn in it comes out a fraction of a bone long. Pushed so the limbs drawn after
    // this one still get their own matrix back.
    Matrix_Push();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(player->actor.world.pos.x, player->actor.world.pos.y + REFLECTOR_HEIGHT, player->actor.world.pos.z,
                     MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(sScale, sScale, sScale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPDisplayList(POLY_XLU_DISP++, sHexDL);

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}
