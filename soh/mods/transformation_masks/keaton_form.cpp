#include "keaton_form.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ResourceManagerHelpers.h"
#include "mods/anim_translator/mm_anim_loader.h"
#include "mods/mm_sources/mm_anims.h"
#include "objects/gameplay_keep/gameplay_keep.h"

extern "C" void Player_PlayVoiceSfx(Player* player, u16 sfxId);
extern "C" void func_80837918(Player* player, s32 quadIndex, u32 dmgFlags);
extern "C" s32 func_80041DB8(CollisionContext* colCtx, CollisionPoly* poly, s32 bgId);
extern "C" void func_80837C0C(PlayState* play, Player* player, s32 damageResponseType, f32 speed, f32 yVelocity,
                              s16 yRot, s32 invincibilityTimer);
extern "C" s32 ItemMagic_HasEnough(PlayState* play, s16 amount);
extern "C" void ItemMagic_Consume(PlayState* play, s16 amount);
extern "C" u8 GaroForm_VanillaWantsAButton(Player* player);

// Read by func_80041DB8 (z_bgcheck.c) alongside the Mogma Mitts flag.
extern "C" u8 gKeatonClimbActive = 0;

#define ANIM_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_ss_"

namespace {

// Rows of z_player.c's D_80854488, which is indexed by sword. The DMG_SLASH_* names
// in z64collision_check.h sit one sword tier off the weapon they belong to, so both
// the value and the sword it really means are spelled out here.
const u32 SLASH_KOKIRI_SWORD = 0x00000200;
const u32 SLASH_MASTER_SWORD = 0x00000100;

// Rows of En_Light's D_80A9E840, picked by params & 0xF.
const s16 FLAME_BLUE = 2;   // prim 0,170,255 over env 0,0,255
const s16 FLAME_ORANGE = 0; // prim 255,200,0 over env 255,0,0

// En_Light's own Init scales itself by table row * 0.0001f; these multiply that.
// Tuned in-game on the sliders, then baked.
const f32 BALL_SCALE_LEVEL1 = 0.0075f * 0.16f;
const f32 BALL_SCALE_LEVEL2 = 0.0075f * 0.33f;
const s16 BALL_RADIUS_LEVEL1 = 34;
const s16 BALL_RADIUS_LEVEL2 = 60;

const f32 FIST_REACH = 1200.0f;

const f32 LOOP_START = 13.0f;
const f32 LOOP_END = 17.0f;

const s16 CHARGE_LEVEL1 = 20;
const s16 CHARGE_LEVEL2 = 60;

const f32 SHOT_SPEED = 4.5f;
const f32 RANGE_LEVEL1 = 600.0f;
const f32 RANGE_LEVEL2 = 1000.0f;
const f32 PULL_RADIUS_LEVEL1 = 280.0f;
const f32 PULL_RADIUS_LEVEL2 = 400.0f;
const f32 PULL_STEP = 7.0f;

enum State {
    STATE_IDLE,
    STATE_COMBO,
    STATE_CHARGE,
    STATE_THROW,
    STATE_LONGJUMP,
    STATE_AIRKICK,
    STATE_RECOIL,
};

// z_player.c's func_80842D20 uses this for a blocked swing.
const f32 RECOIL_SPEED = -18.0f;
// 16 to a heart.
const u8 KICK_BLOCK_DAMAGE = 8;
const s16 KICK_BLOCK_INVINCIBILITY = 20;
const s16 DAMAGE_FLASH_RED = 0x4000;
const s16 HADOUKEN_MAGIC_LEVEL1 = 8;
const s16 HADOUKEN_MAGIC_LEVEL2 = 16;

// An MM clip is named by id, an MHR one by path; the hit window is the span of
// frames on which the fists may damage.
struct Move {
    const char* path;
    MmAnimId mmAnim;
    f32 hitStart;
    f32 hitEnd;
};

const Move kCombo[3] = {
    { nullptr, MM_ANIM_PG_PUNCHA, 4.0f, 9.0f },
    { nullptr, MM_ANIM_PG_PUNCHB, 6.0f, 13.0f },
    { ANIM_DIR "dash_attack09", MM_ANIM_MAX, 14.0f, 26.0f },
};

const char* kChargePath = ANIM_DIR "attack13";

#define FIELD_DIR "__OTR__misc/link_animetion/gPlayerAnim_mhr_field_"

const char* kLongJumpPath = FIELD_DIR "charge_attack01";
const char* kAirKickPath = FIELD_DIR "wirebug_dash02";

const f32 LONG_JUMP_SPEED = 16.0f;
const f32 LONG_JUMP_LIFT = 8.0f;
// OOT swaps the animation the moment Link leaves the floor, so ours has to land after
// that change or it is overwritten. Same two-frame wait Roc's Feather uses.
const s16 LONG_JUMP_ANIM_DELAY = 2;

// The climb's root translation is what moves Keaton, so the playback rate IS the climb
// speed: twice OOT's own.
const f32 CLIMB_RATE = 2.0f;
// Same drain as the mitts: 1 MP every 10 frames (item_mitts.h).
const s16 CLIMB_DRAIN_INTERVAL = 10;
const s16 CLIMB_MP_COST = 1;

const f32 AIR_KICK_SPEED = 14.0f;
const u8 AIR_KICK_DAMAGE = 10;

// A step only chains while its own animation is still running past the hit, which
// is what makes mashing feel like a combo instead of a queue.
const f32 kChainOpens = 0.55f;

LinkAnimationHeader* sCombo[3];
LinkAnimationHeader* sCharge;
LinkAnimationHeader* sLongJump;
LinkAnimationHeader* sAirKick;
u8 sLoaded = 0;

s16 sClimbDrain = 0;
s16 sJumpDelay = 0;
u8 sBlocked = 0;

u8 sState = STATE_IDLE;
s32 sStep = 0;
s16 sChargeTimer = 0;
u8 sChargeLevel = 0;
u8 sLoopReversing = 0;

struct Tracked {
    Actor* actor;
    u8 category;
};

Tracked sFlame;

struct Shot {
    Vec3f pos;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 travelled;
    f32 range;
    f32 pullRadius;
    u8 level;
    u8 active;
};

Shot sShot;

ColliderCylinder sShotCyl;
u8 sShotCylReady = 0;

ColliderCylinderInit sShotCylInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { SLASH_MASTER_SWORD, 0x00, 0x02 },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 34, 46, -24, { 0, 0, 0 } },
};

f32 BallScale(u8 level) {
    return (level >= 2) ? BALL_SCALE_LEVEL2 : BALL_SCALE_LEVEL1;
}

s16 BallRadius(u8 level) {
    return (level >= 2) ? BALL_RADIUS_LEVEL2 : BALL_RADIUS_LEVEL1;
}

void Load() {
    if (sLoaded) {
        return;
    }
    sLoaded = 1;
    for (s32 i = 0; i < 3; i++) {
        sCombo[i] = (kCombo[i].path != nullptr)
                        ? (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(kCombo[i].path)
                        : MmAnim_Load(kCombo[i].mmAnim);
    }
    sCharge = (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(kChargePath);
    sLongJump = (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(kLongJumpPath);
    sAirKick = (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(kAirKickPath);
}

// Keaton climbs anything while A is held; the wall's own flags decide whether it costs
// magic. Reading func_80041DB8 with our own term switched off is what tells the two
// apart without duplicating the surface tables.
void TickClimb(Player* player, PlayState* play, u8 holdingA) {
    u8 climbing = (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) != 0;

    if (climbing) {
        if (player->skelAnime.playSpeed > 0.0f) {
            player->skelAnime.playSpeed = CLIMB_RATE;
        } else if (player->skelAnime.playSpeed < 0.0f) {
            player->skelAnime.playSpeed = -CLIMB_RATE;
        }
    }

    // Wall contact only decides whether a climb may START. Re-asking it once Keaton is
    // on the wall is what shook him off: BGCHECKFLAG_WALL drops out between rungs, and
    // every gap turned the surface un-climbable for a frame.
    u8 canHold = climbing || ((player->actor.bgCheckFlags & BGCHECKFLAG_WALL) && (player->actor.wallPoly != nullptr));
    if (!holdingA || !canHold) {
        gKeatonClimbActive = 0;
        sClimbDrain = 0;
        return;
    }

    gKeatonClimbActive = 1;
    if (player->actor.wallPoly == nullptr) {
        return;
    }

    gKeatonClimbActive = 0;
    u8 freeSurface = (func_80041DB8(&play->colCtx, player->actor.wallPoly, player->actor.wallBgId) & 8) != 0;
    gKeatonClimbActive = 1;

    if (freeSurface) {
        sClimbDrain = 0;
        return;
    }

    sClimbDrain++;
    if (sClimbDrain < CLIMB_DRAIN_INTERVAL) {
        return;
    }
    sClimbDrain = 0;
    if (!ItemMagic_HasEnough(play, CLIMB_MP_COST)) {
        gKeatonClimbActive = 0;
        return;
    }
    ItemMagic_Consume(play, CLIMB_MP_COST);
}

// Actor_Kill only nulls update(); the actor is freed at end of frame, so a stored
// pointer dangles. Walking the list is what makes following one across frames safe.
Actor* Resolve(PlayState* play, Tracked* t) {
    if (t->actor == nullptr) {
        return nullptr;
    }
    for (Actor* it = play->actorCtx.actorLists[t->category].head; it != nullptr; it = it->next) {
        if (it == t->actor) {
            return (it->update != nullptr) ? it : nullptr;
        }
    }
    t->actor = nullptr;
    return nullptr;
}

void KillFlame(PlayState* play) {
    Actor* flame = Resolve(play, &sFlame);
    if (flame != nullptr) {
        Actor_Kill(flame);
    }
    sFlame.actor = nullptr;
}

// The colour is baked into En_Light's params at spawn, so stepping up to orange
// means replacing the actor, not recolouring it.
void SpawnFlame(PlayState* play, Vec3f* at, u8 level) {
    KillFlame(play);
    Actor* flame = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, at->x, at->y, at->z, 0, 0, 0x4000,
                               (level >= 2) ? FLAME_ORANGE : FLAME_BLUE);
    sFlame.actor = flame;
    if (flame != nullptr) {
        sFlame.category = flame->category;
    }
}

void PlaceFlame(PlayState* play, Vec3f* at, u8 level) {
    Actor* flame = Resolve(play, &sFlame);
    if (flame == nullptr) {
        return;
    }
    flame->world.pos = *at;
    Actor_SetScale(flame, BallScale(level));
}

// The ball is its own vacuum: enemies are dragged onto it rather than knocked back,
// so a crowd collapses into the one spot the cylinder covers.
void PullEnemies(PlayState* play) {
    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; it != nullptr; it = it->next) {
        if (it->update == nullptr) {
            continue;
        }
        Vec3f toBall;
        f32 dist = Math_Vec3f_DistXYZAndStoreDiff(&it->world.pos, &sShot.pos, &toBall);
        if ((dist > sShot.pullRadius) || (dist < 1.0f)) {
            continue;
        }
        f32 step = PULL_STEP / dist;
        it->world.pos.x += toBall.x * step;
        it->world.pos.y += toBall.y * step;
        it->world.pos.z += toBall.z * step;
    }
}

void SubmitShotCollider(PlayState* play, Player* player) {
    if (!sShotCylReady) {
        Collider_InitCylinder(play, &sShotCyl);
        Collider_SetCylinder(play, &sShotCyl, &player->actor, &sShotCylInit);
        sShotCylReady = 1;
    }
    sShotCyl.dim.radius = BallRadius(sShot.level);
    sShotCyl.base.atFlags |= AT_ON;
    sShotCyl.dim.pos.x = (s16)sShot.pos.x;
    sShotCyl.dim.pos.y = (s16)sShot.pos.y;
    sShotCyl.dim.pos.z = (s16)sShot.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sShotCyl.base);
}

// Straight at whatever Keaton has locked on, otherwise wherever he is facing. The
// camera does not get a vote: it drifts off Link's body while the charge is held.
void Aim(Player* player) {
    Actor* target = player->focusActor;
    u8 lockedOnFoe = (target != nullptr) && (target->update != nullptr) &&
                     ((target->category == ACTORCAT_ENEMY) || (target->category == ACTORCAT_BOSS));

    if (lockedOnFoe) {
        Vec3f toFoe;
        f32 dist = Math_Vec3f_DistXYZAndStoreDiff(&sShot.pos, &target->focus.pos, &toFoe);
        if (dist > 1.0f) {
            f32 scale = SHOT_SPEED / dist;
            sShot.dx = toFoe.x * scale;
            sShot.dy = toFoe.y * scale;
            sShot.dz = toFoe.z * scale;
            return;
        }
    }

    s16 yaw = player->actor.shape.rot.y;
    sShot.dx = Math_SinS(yaw) * SHOT_SPEED;
    sShot.dy = 0.0f;
    sShot.dz = Math_CosS(yaw) * SHOT_SPEED;
}

void Launch(PlayState* play, Player* player, u8 level) {
    sShot.pos = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    Aim(player);
    sShot.travelled = 0.0f;
    sShot.range = (level >= 2) ? RANGE_LEVEL2 : RANGE_LEVEL1;
    sShot.pullRadius = (level >= 2) ? PULL_RADIUS_LEVEL2 : PULL_RADIUS_LEVEL1;
    sShot.level = level;
    sShot.active = 1;

    Player_PlayVoiceSfx(player, NA_SE_VO_LI_MAGIC_ATTACK);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_MAGIC_ARROW_SHOT);
}

void UpdateShot(PlayState* play, Player* player) {
    if (!sShot.active) {
        return;
    }

    sShot.pos.x += sShot.dx;
    sShot.pos.y += sShot.dy;
    sShot.pos.z += sShot.dz;
    sShot.travelled += SHOT_SPEED;

    PlaceFlame(play, &sShot.pos, sShot.level);
    PullEnemies(play);
    SubmitShotCollider(play, player);
    Audio_PlaySoundGeneral(NA_SE_EV_FIRE_PILLAR, &sShot.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

    if ((sShot.travelled >= sShot.range) || (sShotCyl.base.atFlags & AT_HIT)) {
        sShotCyl.base.atFlags &= ~(AT_ON | AT_HIT);
        sShot.active = 0;
        KillFlame(play);
    }
}

void ClearState() {
    sState = STATE_IDLE;
    sStep = 0;
    sChargeTimer = 0;
    sChargeLevel = 0;
    sLoopReversing = 0;
    sJumpDelay = 0;
    sBlocked = 0;
}

void Play(PlayState* play, Player* player, LinkAnimationHeader* anim, f32 start, f32 end) {
    sBlocked = 0;
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    f32 speed = (end >= start) ? 1.0f : -1.0f;
    LinkAnimation_Change(play, &player->skelAnime, anim, speed, start, end, ANIMMODE_ONCE, -6.0f);
}

u8 WasBlocked() {
    return sBlocked;
}

// A shield stops Keaton dead. In the air it also hurts: the kick has no defence of its
// own, so throwing it at a guard costs health rather than only distance.
void StartRecoil(PlayState* play, Player* player) {
    sBlocked = 0;
    sJumpDelay = 0;
    player->meleeWeaponState = 0;
    player->meleeWeaponQuads[0].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    player->meleeWeaponQuads[1].base.atFlags &= ~(AT_ON | AT_BOUNCED);
    Rumble_Request(0.0f, 180, 20, 100);

    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        // The whole vanilla reaction, not just the health loss: func_80837C0C carries the
        // hurt sfx, the animation and the knockback, and reads colChkInfo.damage for the
        // hearts. It is thrown backwards from where Keaton faces, away from the guard.
        player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
        ClearState();
        player->actor.colChkInfo.damage = KICK_BLOCK_DAMAGE;
        func_80837C0C(play, player, PLAYER_HIT_RESPONSE_KNOCKBACK_SMALL, 4.0f, 5.0f, player->actor.shape.rot.y + 0x8000,
                      KICK_BLOCK_INVINCIBILITY);
        Actor_SetColorFilter(&player->actor, DAMAGE_FLASH_RED, 0xFF, 0, KICK_BLOCK_INVINCIBILITY);
        return;
    }

    sState = STATE_RECOIL;
    LinkAnimationHeader* rebound = (LinkAnimationHeader*)&gPlayerAnim_link_fighter_rebound;
    Play(play, player, rebound, 0.0f, Animation_GetLastFrame(rebound));
    player->linearVelocity = RECOIL_SPEED;
}

void StartAirKick(PlayState* play, Player* player) {
    sState = STATE_AIRKICK;
    sJumpDelay = 0;
    func_80837918(player, 0, SLASH_KOKIRI_SWORD);
    func_80837918(player, 1, SLASH_KOKIRI_SWORD);
    player->actor.shape.rot.y = player->yaw;
    player->linearVelocity = AIR_KICK_SPEED;
    Play(play, player, sAirKick, 0.0f, Animation_GetLastFrame(sAirKick));
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_SWING_HARD);
}

u8 LevelForTimer(s16 timer) {
    if (timer >= CHARGE_LEVEL2) {
        return 2;
    }
    if (timer >= CHARGE_LEVEL1) {
        return 1;
    }
    return 0;
}

void EnterCharge(PlayState* play, Player* player) {
    sState = STATE_CHARGE;
    sChargeTimer = 0;
    sChargeLevel = 0;
    sLoopReversing = 0;
    player->meleeWeaponState = 0;
    Play(play, player, sCharge, 0.0f, LOOP_END);
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_CHARGE);
}

// Ping-pong across the hold window: LinkAnimation has ONCE and LOOP but no
// bounce, so each leg is relaunched with the sign of the speed flipped.
void TickChargeLoop(PlayState* play, Player* player) {
    sLoopReversing = !sLoopReversing;
    if (sLoopReversing) {
        Play(play, player, sCharge, LOOP_END, LOOP_START);
    } else {
        Play(play, player, sCharge, LOOP_START, LOOP_END);
    }
}

} // namespace

// Player_UpdateCommon wipes AT_BOUNCED before TransformMasks_Update runs, so this is the
// only place the block is still readable — the same reason GerudoMhr_ScanBladeHits sits
// beside it.
extern "C" void KeatonForm_ScanBlock(Player* player) {
    if ((sState != STATE_COMBO) && (sState != STATE_AIRKICK)) {
        return;
    }
    if ((player->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) ||
        (player->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED)) {
        sBlocked = 1;
    }
}

// Runs before the OOT-yield block, not from the update below: climbing IS a yield, and
// that block returns, so anything called from the controller proper never ticks while
// Keaton is on a wall.
extern "C" void KeatonForm_TickClimb(Player* player, PlayState* play) {
    Load();
    TickClimb(player, play, CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A));
}

// The quads are Link's own; only their reach and flags are Keaton's. Returns the
// hand mask so mm_player_form.cpp can place them from the hand matrices.
extern "C" u8 KeatonForm_GetQuadGate(u32* dmgFlags, f32* reach) {
    if (sState == STATE_AIRKICK) {
        *dmgFlags = SLASH_KOKIRI_SWORD;
        *reach = FIST_REACH;
        return 1;
    }
    if ((sState != STATE_COMBO) || (sStep < 0) || (sStep >= 3)) {
        return 0;
    }
    *dmgFlags = SLASH_KOKIRI_SWORD;
    *reach = FIST_REACH;
    return 1;
}

// Clip end must not reach this: the shot outlives the animation that threw it.
extern "C" void KeatonForm_Reset(PlayState* play) {
    ClearState();
    KillFlame(play);
    sShot.active = 0;
    if (sShotCylReady) {
        sShotCyl.base.atFlags &= ~(AT_ON | AT_HIT);
    }
}

extern "C" u8 KeatonForm_Update(Player* player, PlayState* play) {
    Load();
    UpdateShot(play, player);
    if (sCombo[0] == nullptr) {
        return 0;
    }

    Input* input = &play->state.input[0];
    u8 grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    u8 holdingB = CHECK_BTN_ALL(input->cur.button, BTN_B);

    if (sState == STATE_IDLE) {
        if (player->stateFlags1 &
            (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED |
             PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE)) {
            return 0;
        }
        if (!grounded) {
            if ((sAirKick != nullptr) && CHECK_BTN_ALL(input->press.button, BTN_B)) {
                StartAirKick(play, player);
                return 1;
            }
            return 0;
        }
        // A stays OOT's whenever it has something contextual to do, and a wall in front
        // means the press belongs to the climb, not to the jump.
        if ((sLongJump != nullptr) && CHECK_BTN_ALL(input->press.button, BTN_A) &&
            !GaroForm_VanillaWantsAButton(player) && !(player->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
            sState = STATE_LONGJUMP;
            sJumpDelay = LONG_JUMP_ANIM_DELAY;
            player->linearVelocity = LONG_JUMP_SPEED;
            player->actor.velocity.y = LONG_JUMP_LIFT;
            player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            player->stateFlags1 |= PLAYER_STATE1_JUMPING;
            Player_PlayVoiceSfx(player, NA_SE_VO_LI_AUTO_JUMP);
            return 0;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
            sState = STATE_COMBO;
            sStep = 0;
            func_80837918(player, 0, SLASH_KOKIRI_SWORD);
            func_80837918(player, 1, SLASH_KOKIRI_SWORD);
            Play(play, player, sCombo[0], 0.0f, Animation_GetLastFrame(sCombo[0]));
            return 1;
        }
        return 0;
    }

    if (sState == STATE_COMBO) {
        const Move* move = &kCombo[sStep];
        const f32 last = Animation_GetLastFrame(sCombo[sStep]);
        const f32 frame = player->skelAnime.curFrame;

        if (WasBlocked()) {
            StartRecoil(play, player);
            return 1;
        }

        if ((sStep + 1 < 3) && (frame >= last * kChainOpens) && CHECK_BTN_ALL(input->press.button, BTN_B)) {
            sStep++;
            Play(play, player, sCombo[sStep], 0.0f, Animation_GetLastFrame(sCombo[sStep]));
            return 1;
        }
        player->meleeWeaponState = ((frame >= move->hitStart) && (frame <= move->hitEnd)) ? 1 : 0;

        // The charge opens only once the punch has played out, the way OOT's spin relates
        // to its slash: a tap always buys the whole hit, and chaining releases B anyway.
        if (LinkAnimation_Update(play, &player->skelAnime)) {
            player->meleeWeaponState = 0;
            if (holdingB && (sCharge != nullptr)) {
                EnterCharge(play, player);
                return 1;
            }
            player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
            ClearState();
            return 0;
        }
        player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->linearVelocity = 0.0f;
        return 1;
    } else if (sState == STATE_CHARGE) {
        sChargeTimer++;
        u8 level = LevelForTimer(sChargeTimer);

        if (level != sChargeLevel) {
            sChargeLevel = level;
            SpawnFlame(play, &player->bodyPartsPos[PLAYER_BODYPART_R_HAND], level);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SWORD_CHARGE);
        }
        PlaceFlame(play, &player->bodyPartsPos[PLAYER_BODYPART_R_HAND], level);

        if (!holdingB) {
            s16 cost = (level >= 2) ? HADOUKEN_MAGIC_LEVEL2 : HADOUKEN_MAGIC_LEVEL1;
            if ((level == 0) || !ItemMagic_HasEnough(play, cost)) {
                KillFlame(play);
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                ClearState();
                return 0;
            }
            ItemMagic_Consume(play, cost);
            Launch(play, player, level);
            sState = STATE_THROW;
            Play(play, player, sCharge, LOOP_END, Animation_GetLastFrame(sCharge));
            return 1;
        }

        if (LinkAnimation_Update(play, &player->skelAnime)) {
            TickChargeLoop(play, player);
        }
        player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->linearVelocity = 0.0f;
        return 1;
    } else if ((sState == STATE_LONGJUMP) || (sState == STATE_AIRKICK)) {
        if ((sState == STATE_AIRKICK) && WasBlocked()) {
            StartRecoil(play, player);
            return 1;
        }

        // OOT owns these frames on purpose: it is mid-swap to its own airborne clip, and
        // ours only sticks once that has happened.
        if (sJumpDelay > 0) {
            sJumpDelay--;
            if (sJumpDelay > 0) {
                return 0;
            }
            Play(play, player, sLongJump, 0.0f, Animation_GetLastFrame(sLongJump));
        }

        // The jump is interruptible: B turns it into the kick, and a wall with A held
        // hands the frame straight back, since OOT does the grabbing and the pause flag
        // below is what would keep it from ever running.
        if (sState == STATE_LONGJUMP) {
            if ((sAirKick != nullptr) && CHECK_BTN_ALL(input->press.button, BTN_B)) {
                StartAirKick(play, player);
                return 1;
            }
            if (CHECK_BTN_ALL(input->cur.button, BTN_A) && (player->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
                player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
                ClearState();
                return 0;
            }
        }

        // Neither move zeroes the velocity: carrying it is the whole point of both.
        player->meleeWeaponState = (sState == STATE_AIRKICK) ? 1 : 0;

        u8 landed = grounded && (player->actor.velocity.y <= 0.0f);
        if (landed || LinkAnimation_Update(play, &player->skelAnime)) {
            player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
            player->meleeWeaponState = 0;
            ClearState();
            return 0;
        }
        player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        return 1;
    }

    if (LinkAnimation_Update(play, &player->skelAnime)) {
        player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->meleeWeaponState = 0;
        ClearState();
        return 0;
    }

    // Re-asserted every frame: OOT's own transitions clear stateFlags3 out from under us.
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    // The recoil is the one clip here whose backward push must survive to be felt.
    if (sState != STATE_RECOIL) {
        player->linearVelocity = 0.0f;
    }
    return 1;
}
