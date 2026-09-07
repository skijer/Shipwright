/**
 * kafei_form.cpp — Kafei's behaviour: SW97's standalone shield (z64proto/sw97)
 * plus a BotW-style stamina sprint that takes over the A button.
 */

#include "kafei_form.h"
#include "transformation_masks.h"

extern "C" {

#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "objects/gameplay_keep/gameplay_keep.h"
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);

// z_player.c's melee-weapon state setter: it owns the swing sfx and the grunt, so the moving
// slash arms the blade through it rather than writing meleeWeaponState behind its back.
void func_80833A20(Player* player, s32 newMeleeWeaponState);

extern PlayState* gPlayState;
extern SaveContext gSaveContext;
u8 MmForm_IsKafeiFormActive(void);

// SW97 tests `speedXZ > 0.1f` unsigned, not a dead stop, so a coasting halt still
// guards. Kept exact: backing up is negative here and the prototype guards then too.
#define KAFEI_SHIELD_STILL_SPEED 0.1f

#define KAFEI_SPRINT_MUL 1.5f
#define KAFEI_SPRINT_ANIM_MUL 1.4f
#define KAFEI_WHEEL_FRAMES 220.0f
#define KAFEI_REFILL_PER_FRAME 2.2f
#define KAFEI_REFILL_DELAY 18
#define KAFEI_STICK_MOVING 10.0f
#define KAFEI_SLASH_NONE 0xFF

static struct {
    f32 stamina;
    u8 sprinting;
    u8 winded;
    u8 windedAnimSet;
    s16 refillDelay;
    u8 slashClip;
    u8 slashFrame;
} sKafei;

static f32 KafeiForm_Capacity(void) {
    return gSaveContext.isDoubleDefenseAcquired ? (KAFEI_WHEEL_FRAMES * 2.0f) : KAFEI_WHEEL_FRAMES;
}

void KafeiForm_Reset(void) {
    sKafei.stamina = KafeiForm_Capacity();
    sKafei.sprinting = 0;
    sKafei.winded = 0;
    sKafei.windedAnimSet = 0;
    sKafei.refillDelay = 0;
    sKafei.slashClip = KAFEI_SLASH_NONE;
    sKafei.slashFrame = 0;
}

u8 KafeiForm_ShieldIsPassive(Player* player) {
    if (player == NULL || !MmForm_IsKafeiFormActive()) {
        return 0;
    }
    if (player->currentShield == PLAYER_SHIELD_NONE) {
        return 0;
    }
    return (player->linearVelocity <= KAFEI_SHIELD_STILL_SPEED) ? 1 : 0;
}

// A belongs to the sprint from the moment it goes down. OOT fires the roll on the
// PRESS, so answering only once the sprint has started would let one roll slip out
// on frame one.
u8 KafeiForm_SuppressRoll(Player* player) {
    if (player == NULL || !MmForm_IsKafeiFormActive() || gPlayState == NULL) {
        return 0;
    }
    if (sKafei.sprinting) {
        return 1;
    }
    Input* in = &gPlayState->state.input[0];
    return CHECK_BTN_ALL(in->cur.button, BTN_A) ? 1 : 0;
}

f32 KafeiForm_RunSpeedMul(void) {
    return (MmForm_IsKafeiFormActive() && sKafei.sprinting) ? KAFEI_SPRINT_MUL : 1.0f;
}

// Without this the legs saturate: the clip keeps its walking cadence while the body
// travels 1.5x faster, which reads as skating.
f32 KafeiForm_RunAnimRateMul(void) {
    return (MmForm_IsKafeiFormActive() && sKafei.sprinting) ? KAFEI_SPRINT_ANIM_MUL : 1.0f;
}

u8 KafeiForm_WheelCount(void) {
    return gSaveContext.isDoubleDefenseAcquired ? 2 : 1;
}

f32 KafeiForm_WheelFill(u8 wheel) {
    f32 remaining = sKafei.stamina - (f32)wheel * KAFEI_WHEEL_FRAMES;
    if (remaining <= 0.0f) {
        return 0.0f;
    }
    if (remaining >= KAFEI_WHEEL_FRAMES) {
        return 1.0f;
    }
    return remaining / KAFEI_WHEEL_FRAMES;
}

// Hidden only when full and idle, so the wheel does not sit on the HUD forever.
u8 KafeiForm_IsWinded(void) {
    return sKafei.winded;
}

u8 KafeiForm_MeterVisible(void) {
    if (!MmForm_IsKafeiFormActive()) {
        return 0;
    }
    return (sKafei.sprinting || sKafei.winded || sKafei.stamina < KafeiForm_Capacity()) ? 1 : 0;
}

// OOT has no out-of-breath clip (that one is MM's, and Kafei never loads mm.o2r), so
// the heavy-landing pose stands in: Link doubled over, recovering.
static void KafeiForm_PlayWindedAnim(Player* player, PlayState* play) {
    if (sKafei.windedAnimSet) {
        return;
    }
    static LinkAnimationHeader* sWindedAnim = NULL;
    if (sWindedAnim == NULL) {
        sWindedAnim = (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(dgPlayerAnim_link_normal_landing);
    }
    if (sWindedAnim == NULL) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, sWindedAnim, 1.0f, 0.0f, Animation_GetLastFrame(sWindedAnim),
                         ANIMMODE_ONCE, -6.0f);
    sKafei.windedAnimSet = 1;
}

// SW97 lets Link slash WITHOUT breaking stride. Rather than replacing the body animation, the
// beta clip drives ONLY the sword arm and OOT's own locomotion keeps the legs, the speed and the
// shield exactly as they were — so the vanilla attack, which takes the whole body and brakes to a
// stop, must never start. KafeiForm_StartMovingSlash is what answers Player_ActionHandler_7.
#define KAFEI_MOVING_SLASH_MIN_SPEED 2.0f

typedef struct {
    const char* path;
    u8 arcStart;
    u8 arcEnd;
} KafeiSlashClip;

// The arc bounds are measured, not chosen: they are the frames whose left-arm angular velocity
// clears 30% of each clip's peak, which is the span where the blade is actually sweeping.
static const KafeiSlashClip sSlashClips[] = {
    { "__OTR__misc/link_animetion/gPlayerAnim_mhr_sw97_move_sword_slash", 1, 5 },
    { "__OTR__misc/link_animetion/gPlayerAnim_mhr_sw97_move_stick_slash", 2, 4 },
};

#define KAFEI_SLASH_SWORD 0
#define KAFEI_SLASH_STICK 1

static LinkAnimationHeader* KafeiForm_SlashAnim(u8 clip) {
    static LinkAnimationHeader* sCache[ARRAY_COUNT(sSlashClips)];

    if (sCache[clip] == NULL) {
        sCache[clip] = (LinkAnimationHeader*)ResourceMgr_LoadPlayerAnimAsHeader(sSlashClips[clip].path);
    }
    return sCache[clip];
}

static void KafeiForm_EndMovingSlash(Player* player) {
    if (sKafei.slashClip == KAFEI_SLASH_NONE) {
        return;
    }
    sKafei.slashClip = KAFEI_SLASH_NONE;
    if (player->meleeWeaponState != 0) {
        func_80833A20(player, 0);
    }
}

u8 KafeiForm_StartMovingSlash(Player* player) {
    if (!MmForm_IsKafeiFormActive()) {
        return 0;
    }
    // One of ours is already in flight: still refuse the vanilla swing, or it would take the body
    // out from under the overlay.
    if (sKafei.slashClip != KAFEI_SLASH_NONE) {
        return 1;
    }
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return 0;
    }
    if (TransformMasks_GetStickMagnitude() < KAFEI_STICK_MOVING ||
        player->linearVelocity < KAFEI_MOVING_SLASH_MIN_SPEED) {
        return 0;
    }

    // Swords are 1..3 and the Deku Stick 4; the hammer and the custom rods keep their own swing.
    s32 weapon = Player_ActionToMeleeWeapon(player->heldItemAction);
    if ((weapon < 1) || (weapon > 4)) {
        return 0;
    }

    u8 clip = (weapon == 4) ? KAFEI_SLASH_STICK : KAFEI_SLASH_SWORD;
    if (KafeiForm_SlashAnim(clip) == NULL) {
        return 0;
    }

    sKafei.slashClip = clip;
    sKafei.slashFrame = 0;
    // func_80833A20 picks its swing sfx and grunt off these two; a plain one-handed slash is right.
    player->meleeWeaponAnimation = PLAYER_MWA_FORWARD_SLASH_1H;
    player->unk_845 = 0;
    return 1;
}

static void KafeiForm_UpdateMovingSlash(Player* player, PlayState* play) {
    if (sKafei.slashClip == KAFEI_SLASH_NONE) {
        return;
    }
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (Player_ActionToMeleeWeapon(player->heldItemAction) == 0)) {
        KafeiForm_EndMovingSlash(player);
        return;
    }

    const KafeiSlashClip* clip = &sSlashClips[sKafei.slashClip];
    LinkAnimationHeader* anim = KafeiForm_SlashAnim(sKafei.slashClip);
    if (anim == NULL) {
        KafeiForm_EndMovingSlash(player);
        return;
    }

    // This runs after Player_UpdateCommon, so jointTable already holds the walk or run pose and
    // these three joints are the only thing that changes. SetLoadFrame copies synchronously.
    Vec3s pose[PLAYER_LIMB_MAX];
    AnimationContext_SetLoadFrame(play, anim, sKafei.slashFrame, PLAYER_LIMB_MAX, pose);
    player->skelAnime.jointTable[PLAYER_LIMB_L_SHOULDER] = pose[PLAYER_LIMB_L_SHOULDER];
    player->skelAnime.jointTable[PLAYER_LIMB_L_FOREARM] = pose[PLAYER_LIMB_L_FOREARM];
    player->skelAnime.jointTable[PLAYER_LIMB_L_HAND] = pose[PLAYER_LIMB_L_HAND];

    // The blade damages straight off the hand limb matrix, so arming it over the arc is all the
    // hit detection this needs.
    u8 blade = (sKafei.slashFrame >= clip->arcStart) && (sKafei.slashFrame <= clip->arcEnd);
    if (blade != (player->meleeWeaponState != 0)) {
        func_80833A20(player, blade);
    }

    sKafei.slashFrame++;
    if (sKafei.slashFrame > (u8)Animation_GetLastFrame(anim)) {
        KafeiForm_EndMovingSlash(player);
    }
}

// A under Z-target is a jump, not a jump slash. Answered where OOT decides it
// (z_player.c), because the slash has to be prevented rather than undone afterwards.
u8 KafeiForm_ReplacesJumpslash(void) {
    return MmForm_IsKafeiFormActive();
}

u8 KafeiForm_ReplacesBombchu(void) {
    return MmForm_IsKafeiFormActive();
}

void KafeiStaminaHud_DrawImGui(void);

void KafeiForm_Tick(Player* player, PlayState* play) {
    if (player == NULL || play == NULL) {
        return;
    }
    if (!MmForm_IsKafeiFormActive()) {
        KafeiForm_Reset();
        return;
    }

    KafeiStaminaHud_DrawImGui();

    KafeiForm_UpdateMovingSlash(player, play);

    Input* in = &play->state.input[0];
    u8 onGround = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ? 1 : 0;

    // Winded lasts until the wheels are FULL again, not for a fixed count: running
    // yourself out should cost you the whole recovery, the way BotW does it.
    if (sKafei.winded) {
        sKafei.sprinting = 0;
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        KafeiForm_PlayWindedAnim(player, play);
        if (sKafei.stamina >= KafeiForm_Capacity()) {
            sKafei.winded = 0;
        }
    } else {
        u8 wantsSprint = CHECK_BTN_ALL(in->cur.button, BTN_A) && onGround &&
                         (TransformMasks_GetStickMagnitude() >= KAFEI_STICK_MOVING) &&
                         !(player->stateFlags1 & PLAYER_STATE1_SHIELDING);
        sKafei.sprinting = (wantsSprint && sKafei.stamina > 0.0f) ? 1 : 0;
    }

    if (sKafei.sprinting) {
        sKafei.stamina -= 1.0f;
        sKafei.refillDelay = KAFEI_REFILL_DELAY;
        if (sKafei.stamina <= 0.0f) {
            sKafei.stamina = 0.0f;
            sKafei.sprinting = 0;
            sKafei.winded = 1;
            sKafei.windedAnimSet = 0;
        }
        return;
    }

    if (sKafei.refillDelay > 0) {
        sKafei.refillDelay--;
        return;
    }
    if (sKafei.stamina < KafeiForm_Capacity()) {
        sKafei.stamina += KAFEI_REFILL_PER_FRAME;
        if (sKafei.stamina > KafeiForm_Capacity()) {
            sKafei.stamina = KafeiForm_Capacity();
        }
    }
}

} // extern "C"
