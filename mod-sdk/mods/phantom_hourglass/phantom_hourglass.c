#include <math.h>
#include <stdio.h>
#include <string.h>

#include <libultraship/log/luslog.h>

#include "z64items.h"
#include "z64aiming.h"
#include "hourglass_model.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ActorDB.h"

#define HOURGLASS_KEY "nei.phantom_hourglass"
#define HOURGLASS_BUTTON_COUNT 8
#define HOURGLASS_TIME_OWNER HOURGLASS_KEY
#define HOURGLASS_TIME_PRIORITY 50
#define HOURGLASS_MAGIC_ACTIVATION 1
#define HOURGLASS_DRAIN_INTERVAL 40
#define HOURGLASS_DRAIN_COST 1
#define HOURGLASS_SLOTS 8
#define HOURGLASS_LINK_SLOT 0
#define HOURGLASS_FRAMES 200
#define HOURGLASS_MAX_LIMBS 32
#define HOURGLASS_MAX_INSTANCE 16384
#define HOURGLASS_TRACK_RANGE 700.0f
#define HOURGLASS_MIN_DIST 8.0f
#define HOURGLASS_AIM_CONE 0x1800
#define HOURGLASS_MIN_HISTORY 10
#define HOURGLASS_IDLE_EVICT 200
#define HOURGLASS_IMPACT_SPEED 8.0f
#define HOURGLASS_IMPACT_DAMAGE 1
#define HOURGLASS_SELF_IFRAMES 20
#define HOURGLASS_CAST_POSE_FRAMES 18
#define HOURGLASS_RIBBON_STEP 4
#define HOURGLASS_RIBBON_QUADS (HOURGLASS_FRAMES / HOURGLASS_RIBBON_STEP)
#define HOURGLASS_RIBBON_HALF_WIDTH 6.0f
#define HOURGLASS_HELD_SCALE 0.07f
#define HOURGLASS_GIVE_SCALE 0.18f
#define HOURGLASS_HAND_DROP 2.0f
#define HOURGLASS_MIXER_ADVANCE 0.5f
#define HOURGLASS_MIXER_FADE_STEP 0.002f
#define HOURGLASS_MIXER_ALIVE_CALLS 30
#define HOURGLASS_BLOCKING_STATES                                                                    \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_LOADING | PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_TALKING | \
     PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_HANGING_OFF_LEDGE |          \
     PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE)

typedef enum {
    HOURGLASS_IDLE,
    HOURGLASS_AIMING,
    HOURGLASS_RECALLING,
    HOURGLASS_REWINDING,
} HourglassState;

typedef enum {
    HOURGLASS_CUE_ACTIVATE,
    HOURGLASS_CUE_TARGET,
    HOURGLASS_CUE_EXIT,
    HOURGLASS_CUE_LOOP,
    HOURGLASS_CUE_MAX,
} HourglassCue;

typedef struct {
    Vec3f pos;
    Vec3f velocity;
    Vec3s rot;
    s16 pad;
    f32 speedXZ;
    f32 animFrame;
    Vec3s joints[HOURGLASS_MAX_LIMBS];
} RecordedFrame;

typedef struct {
    Actor* actor;
    SkelAnime* skelAnime;
    RecordedFrame frames[HOURGLASS_FRAMES];
    s16 count;
    s16 writeIndex;
    s16 cursor;
    s16 idleFrames;
    bool isUsed;
    bool isScrubbing;
} RecordedActor;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconPhantomHourglassTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gPhantomHourglassNameTex";
static const ALIGN_ASSET(2) char sCastAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_hook_shot_ready";

static const u8 sTrackedCategories[] = { ACTORCAT_SWITCH, ACTORCAT_BG,   ACTORCAT_EXPLOSIVE,  ACTORCAT_NPC,
                                         ACTORCAT_ENEMY,  ACTORCAT_PROP, ACTORCAT_ITEMACTION, ACTORCAT_MISC,
                                         ACTORCAT_DOOR,   ACTORCAT_CHEST };

static const u16 sItemButtons[HOURGLASS_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                          BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static ColliderCylinderInit sImpactColliderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0,
      { 0xFFCFFFFF, 0x00, HOURGLASS_IMPACT_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { 40, 60, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = {
    "OnGameFrameUpdate",      "OnPlayerUpdate",          "OnActorDrawEnd", "OnActorResolveGrayscale",
    "OnRoomResolveGrayscale", "OnPlayerResolveLimbDraw", "OnSceneInit"
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

#define REMOTE_VISUAL_INTERVAL 5u

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);
static void InitRemoteVisuals(void);

static RecordedActor sSlots[HOURGLASS_SLOTS];
static u8 sState;
static Actor* sOffer;
static Actor* sTarget;
static s16 sRecallFrames;
static u32 sRecallStartFrame;
static s16 sCastPoseFrames;
static s8 sPreviousInvincibility;
static bool sIsSelfRewinding;
static bool sIsInHand;
static bool sIsUsePending;
static bool sIsColliderReady;
static ColliderCylinder sImpactCollider;
static Vtx sRibbonVtx[HOURGLASS_RIBBON_QUADS * 4];
static s16 sSceneNum = -1;

void Player_ZeroSpeedXZ(Player* player);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
LinkAnimationHeader* Player_GetIdleAnim(Player* player);
void func_80839FFC(Player* player, PlayState* play);
SoundFontSample* ResourceMgr_LoadAudioSample(const char* path);

static RecordedActor* StalestSlot(void);
static void AimAction(Player* player, PlayState* play);

// The two threads share only these fields, one writer each: the game thread raises wantCue, loopWant and
// volume; the audio thread owns every cursor.
typedef struct {
    s16* samples;
    s32 count;
} CueSound;

static CueSound sCueSounds[HOURGLASS_CUE_MAX];
static volatile u8 sWantCue;
static volatile u8 sPendingCue;
static volatile u8 sWantLoop;
static volatile u8 sMixerAlive;
static volatile f32 sMixerVolume = 1.0f;
static f32 sCuePos;
static f32 sCueEnd;
static u8 sIsCuePlaying;
static f32 sLoopPos;
static f32 sLoopGain;

static void LoadCueSounds(void) {
    static const char* const paths[HOURGLASS_CUE_MAX] = {
        "custom/samples/hourglass_activate",
        "custom/samples/hourglass_target",
        "custom/samples/hourglass_exit",
        "custom/samples/hourglass_loop",
    };

    for (u8 cue = 0; cue < HOURGLASS_CUE_MAX; cue++) {
        SoundFontSample* sample = ResourceMgr_LoadAudioSample(paths[cue]);

        if (sample != NULL && sample->sampleAddr != NULL) {
            sCueSounds[cue].samples = (s16*)sample->sampleAddr;
            sCueSounds[cue].count = (s32)(sample->size / sizeof(s16));
        }
    }
}

static void PlayCue(u8 cue) {
    sPendingCue = cue;
    sWantCue = 1;
}

static void StartCueLoop(void) {
    sWantLoop = 1;
}

static void StopCueLoop(void) {
    sWantLoop = 0;
}

// Read on the audio thread, so it is refreshed from the item's tick; a stalled game lets the bed fade out.
static void KeepMixerAlive(void) {
    sMixerVolume = CVarGetInteger("gSettings.Volume.Master", 100) / 100.0f *
                   (CVarGetInteger("gSettings.Volume.SFX", 100) / 100.0f);
    sMixerAlive = HOURGLASS_MIXER_ALIVE_CALLS;
}

static f32 SampleAt(CueSound* sound, f32 position) {
    s32 index = (s32)position;
    f32 fraction = position - (f32)index;
    s32 next = index + 1 >= sound->count ? index : index + 1;

    if (index < 0 || index >= sound->count) {
        return 0.0f;
    }
    return (f32)sound->samples[index] + ((f32)sound->samples[next] - (f32)sound->samples[index]) * fraction;
}

static void AddSample(s16* samples, u32 frame, s32 value) {
    s32 left = samples[frame * 2 + 0] + value;
    s32 right = samples[frame * 2 + 1] + value;

    samples[frame * 2 + 0] = (s16)CLAMP(left, -32768, 32767);
    samples[frame * 2 + 1] = (s16)CLAMP(right, -32768, 32767);
}

static void MixCues(s16* samples, u32 frameCount) {
    CueSound* loop = &sCueSounds[HOURGLASS_CUE_LOOP];
    CueSound* cue = &sCueSounds[sPendingCue];
    f32 volume = sMixerVolume;

    if (sMixerAlive == 0) {
        sIsCuePlaying = 0;
        sLoopGain = 0.0f;
        sLoopPos = 0.0f;
        return;
    }
    sMixerAlive--;
    if (sWantCue) {
        sWantCue = 0;
        sCuePos = 0.0f;
        sCueEnd = (f32)cue->count - 1.0f;
        sIsCuePlaying = cue->samples != NULL;
    }
    if (samples == NULL) {
        return;
    }
    for (u32 frame = 0; frame < frameCount; frame++) {
        f32 mix = 0.0f;

        if (sIsCuePlaying) {
            if (sCuePos >= sCueEnd) {
                sIsCuePlaying = 0;
            } else {
                mix += SampleAt(cue, sCuePos);
                sCuePos += HOURGLASS_MIXER_ADVANCE;
            }
        }
        if (sWantLoop) {
            sLoopGain = MIN(sLoopGain + HOURGLASS_MIXER_FADE_STEP, 1.0f);
        } else {
            sLoopGain = MAX(sLoopGain - HOURGLASS_MIXER_FADE_STEP, 0.0f);
        }
        if (sLoopGain > 0.0f && loop->samples != NULL) {
            mix += SampleAt(loop, sLoopPos) * sLoopGain;
            sLoopPos += HOURGLASS_MIXER_ADVANCE;
            if (sLoopPos >= (f32)loop->count) {
                sLoopPos -= (f32)loop->count;
            }
        } else {
            sLoopPos = 0.0f;
        }
        if (mix != 0.0f) {
            AddSample(samples, frame, (s32)(mix * volume));
        }
    }
}

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySfxGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static u16 FindEquippedButtons(void) {
    u16 mask = 0;

    for (u8 button = 0; button < HOURGLASS_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, HOURGLASS_KEY) == 0) {
            mask |= sItemButtons[button];
        }
    }
    return mask;
}

static bool HasMagic(s16 amount) {
    return gSaveContext.isMagicAcquired && gSaveContext.magic >= amount;
}

// Magic_RequestChange refuses while the meter animates a request, so the meter is debited directly.
static bool SpendMagic(PlayState* play, s16 amount) {
    if (!HasMagic(amount)) {
        return false;
    }
    gSaveContext.magic -= amount;
    return true;
}

static bool IsPoseInsideInstance(Actor* actor, SkelAnime* skelAnime) {
    ActorDBEntry* entry = actor == NULL ? NULL : ActorDB_Retrieve(actor->id);
    size_t size = entry == NULL ? 0 : entry->instanceSize;
    uintptr_t base = (uintptr_t)actor;
    uintptr_t joints = skelAnime == NULL ? 0 : (uintptr_t)skelAnime->jointTable;

    if (skelAnime == NULL || size == 0 || joints <= base) {
        return false;
    }
    if (skelAnime->limbCount < 4 || skelAnime->limbCount > HOURGLASS_MAX_LIMBS) {
        return false;
    }
    return joints + skelAnime->limbCount * sizeof(Vec3s) <= base + size;
}

// The skeleton is found by scanning the actor instance; only a candidate that fits inside it and reads as a sane
// animation is trusted.
static bool IsUsableSkelAnime(Actor* actor, SkelAnime* skelAnime) {
    if (actor == NULL || skelAnime == NULL) {
        return false;
    }
    ActorDBEntry* entry = ActorDB_Retrieve(actor->id);
    size_t size = entry == NULL ? 0 : entry->instanceSize;
    uintptr_t base = (uintptr_t)actor;
    uintptr_t joints = (uintptr_t)skelAnime->jointTable;

    if (size < sizeof(Actor) + sizeof(SkelAnime) || size > HOURGLASS_MAX_INSTANCE) {
        return false;
    }
    if (skelAnime->limbCount < 4 || skelAnime->limbCount > HOURGLASS_MAX_LIMBS) {
        return false;
    }
    if (skelAnime->skeleton == NULL || skelAnime->animation == NULL || joints <= base) {
        return false;
    }
    if (joints + skelAnime->limbCount * sizeof(Vec3s) > base + size) {
        return false;
    }
    return skelAnime->animLength > 0.0f && skelAnime->animLength < 10000.0f && skelAnime->curFrame >= 0.0f &&
           skelAnime->curFrame <= skelAnime->animLength;
}

static SkelAnime* FindSkelAnime(Actor* actor) {
    ActorDBEntry* entry = ActorDB_Retrieve(actor->id);
    size_t size = entry == NULL ? 0 : entry->instanceSize;

    if (actor->draw == NULL || size < sizeof(Actor) + sizeof(SkelAnime) || size > HOURGLASS_MAX_INSTANCE) {
        return NULL;
    }
    for (size_t offset = sizeof(Actor); offset <= size - sizeof(SkelAnime); offset += 4) {
        SkelAnime* candidate = (SkelAnime*)((uintptr_t)actor + offset);

        if (IsUsableSkelAnime(actor, candidate)) {
            return candidate;
        }
    }
    return NULL;
}

static RecordedActor* FindSlot(Actor* actor) {
    for (u8 index = 0; index < HOURGLASS_SLOTS; index++) {
        if (sSlots[index].isUsed && sSlots[index].actor == actor) {
            return &sSlots[index];
        }
    }
    return NULL;
}

static RecordedFrame* FrameAt(RecordedActor* slot, s16 stepsBack) {
    if (stepsBack < 0 || stepsBack >= slot->count) {
        return NULL;
    }
    return &slot->frames[(slot->writeIndex - 1 - stepsBack + HOURGLASS_FRAMES * 2) % HOURGLASS_FRAMES];
}

static void ClearSlot(RecordedActor* slot) {
    slot->actor = NULL;
    slot->skelAnime = NULL;
    slot->count = 0;
    slot->writeIndex = 0;
    slot->cursor = 0;
    slot->idleFrames = 0;
    slot->isUsed = false;
    slot->isScrubbing = false;
}

static void ClearAllSlots(void) {
    for (u8 index = 0; index < HOURGLASS_SLOTS; index++) {
        ClearSlot(&sSlots[index]);
    }
}

static bool HasMoved(Vec3f* first, Vec3f* second) {
    return first->x != second->x || first->y != second->y || first->z != second->z;
}

static bool HasPose(RecordedActor* slot) {
    return slot->skelAnime != NULL && IsPoseInsideInstance(slot->actor, slot->skelAnime);
}

static bool IsWorthBanking(RecordedActor* slot, RecordedFrame* newest) {
    if (newest == NULL || HasMoved(&newest->pos, &slot->actor->world.pos)) {
        return true;
    }
    return HasPose(slot) && slot->skelAnime->curFrame != newest->animFrame;
}

static void RecordFrame(RecordedActor* slot) {
    Actor* actor = slot->actor;
    RecordedFrame* newest = FrameAt(slot, 0);
    RecordedFrame* frame;

    if (!IsWorthBanking(slot, newest)) {
        slot->idleFrames = MIN(slot->idleFrames + 1, HOURGLASS_FRAMES * 2);
        return;
    }
    slot->idleFrames = 0;
    frame = &slot->frames[slot->writeIndex];
    frame->pos = actor->world.pos;
    frame->velocity = actor->velocity;
    frame->rot = actor->world.rot;
    frame->speedXZ = actor->speedXZ;
    frame->animFrame = 0.0f;
    if (HasPose(slot)) {
        frame->animFrame = slot->skelAnime->curFrame;
        memcpy(frame->joints, slot->skelAnime->jointTable, slot->skelAnime->limbCount * sizeof(Vec3s));
    }
    slot->writeIndex = (slot->writeIndex + 1) % HOURGLASS_FRAMES;
    slot->count = MIN(slot->count + 1, HOURGLASS_FRAMES);
}

static void ApplyFrame(RecordedActor* slot, RecordedFrame* frame) {
    Actor* actor = slot->actor;

    if (HasPose(slot)) {
        slot->skelAnime->curFrame = frame->animFrame;
        memcpy(slot->skelAnime->jointTable, frame->joints, slot->skelAnime->limbCount * sizeof(Vec3s));
    }
    actor->world.pos = frame->pos;
    actor->prevPos = frame->pos;
    actor->world.rot = frame->rot;
    actor->shape.rot = frame->rot;
    actor->bgCheckFlags = 0;
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    actor->speedXZ = 0.0f;
}

static bool IsTrackedCategory(u8 category) {
    for (u8 index = 0; index < ARRAY_COUNT(sTrackedCategories); index++) {
        if (sTrackedCategories[index] == category) {
            return true;
        }
    }
    return false;
}

static void BindSlot(RecordedActor* slot, Actor* actor) {
    ClearSlot(slot);
    slot->actor = actor;
    slot->skelAnime = FindSkelAnime(actor);
    slot->isUsed = true;
}

static bool IsRecallable(Player* player, Actor* actor) {
    return actor->id != ACTOR_EN_ELF && actor != player->naviActor;
}

static bool IsWorthRecording(Player* player, Actor* actor) {
    return actor->update != NULL && FindSlot(actor) == NULL && IsRecallable(player, actor) &&
           HasMoved(&actor->world.pos, &actor->prevPos) &&
           Math_Vec3f_DistXYZ(&player->actor.world.pos, &actor->world.pos) < HOURGLASS_TRACK_RANGE;
}

static void StealSlotForNewcomer(PlayState* play, Player* player) {
    for (u8 index = 0; index < ARRAY_COUNT(sTrackedCategories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[sTrackedCategories[index]].head; actor != NULL;
             actor = actor->next) {
            RecordedActor* stalest = StalestSlot();

            if (stalest == NULL) {
                return;
            }
            if (IsWorthRecording(player, actor)) {
                BindSlot(stalest, actor);
            }
        }
    }
}

static void RefillPool(PlayState* play, Player* player) {
    bool isPoolFull = true;

    for (u8 slotIndex = 1; slotIndex < HOURGLASS_SLOTS; slotIndex++) {
        RecordedActor* slot = &sSlots[slotIndex];
        Actor* nearest = NULL;
        f32 nearestDistance = SQ(HOURGLASS_TRACK_RANGE);

        if (slot->isUsed) {
            continue;
        }
        for (u8 index = 0; index < ARRAY_COUNT(sTrackedCategories); index++) {
            for (Actor* actor = play->actorCtx.actorLists[sTrackedCategories[index]].head; actor != NULL;
                 actor = actor->next) {
                f32 distance = Math_Vec3f_DistXYZ(&player->actor.world.pos, &actor->world.pos);

                if (actor->update == NULL || FindSlot(actor) != NULL || !HasMoved(&actor->world.pos, &actor->prevPos) ||
                    SQ(distance) >= nearestDistance) {
                    continue;
                }
                nearestDistance = SQ(distance);
                nearest = actor;
            }
        }
        if (nearest == NULL) {
            isPoolFull = false;
            break;
        }
        BindSlot(slot, nearest);
    }
    if (isPoolFull) {
        StealSlotForNewcomer(play, player);
    }
}

// Actor memory is pooled, so a slot is only kept while its actor is still in a live list.
static void DropDeadSlots(PlayState* play) {
    for (u8 slotIndex = 1; slotIndex < HOURGLASS_SLOTS; slotIndex++) {
        RecordedActor* slot = &sSlots[slotIndex];
        bool isAlive = false;

        if (!slot->isUsed) {
            continue;
        }
        for (u8 category = 0; category < ACTORCAT_MAX && !isAlive; category++) {
            for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
                if (actor == slot->actor) {
                    isAlive = true;
                    break;
                }
            }
        }
        if (!isAlive) {
            ClearSlot(slot);
        }
    }
}

static RecordedActor* StalestSlot(void) {
    RecordedActor* stalest = NULL;

    for (u8 index = 1; index < HOURGLASS_SLOTS; index++) {
        RecordedActor* slot = &sSlots[index];

        if (!slot->isUsed || slot->isScrubbing || slot->idleFrames < HOURGLASS_IDLE_EVICT) {
            continue;
        }
        if (stalest == NULL || slot->idleFrames > stalest->idleFrames) {
            stalest = slot;
        }
    }
    return stalest;
}

static void RecordEveryone(PlayState* play, Player* player) {
    RecordedActor* linkSlot = &sSlots[HOURGLASS_LINK_SLOT];

    // Both run even while time is stopped: a slot may point at an actor already freed and reused.
    if (linkSlot->actor != &player->actor) {
        ClearSlot(linkSlot);
        linkSlot->actor = &player->actor;
        linkSlot->isUsed = true;
    }
    DropDeadSlots(play);
    RefillPool(play, player);
    for (u8 index = 0; index < HOURGLASS_SLOTS; index++) {
        RecordedActor* slot = &sSlots[index];

        if (slot->isUsed && !slot->isScrubbing && slot->actor != NULL && slot->actor->update != NULL) {
            RecordFrame(slot);
        }
    }
}

static bool BeginScrub(Actor* actor) {
    RecordedActor* slot = FindSlot(actor);

    if (slot == NULL || slot->count < 2) {
        return false;
    }
    slot->cursor = 0;
    slot->isScrubbing = true;
    return true;
}

static void EndScrub(Actor* actor) {
    RecordedActor* slot = FindSlot(actor);

    if (slot == NULL) {
        return;
    }
    slot->isScrubbing = false;
    if (actor != NULL && actor->update != NULL) {
        RecordedFrame* frame = FrameAt(slot, slot->cursor);

        if (frame != NULL) {
            ApplyFrame(slot, frame);
        }
        actor->freezeTimer = 0;
    }
    if (slot->cursor > 0) {
        slot->writeIndex = (slot->writeIndex - slot->cursor + HOURGLASS_FRAMES * 2) % HOURGLASS_FRAMES;
        slot->count = MAX(slot->count - slot->cursor, 0);
    }
    slot->cursor = 0;
}

static bool ScrubBack(Actor* actor) {
    RecordedActor* slot = FindSlot(actor);
    RecordedFrame* frame;

    if (slot == NULL || !slot->isScrubbing) {
        return false;
    }
    if (slot->cursor + 1 >= slot->count) {
        frame = FrameAt(slot, slot->cursor);
        if (frame != NULL) {
            ApplyFrame(slot, frame);
            if (actor->category != ACTORCAT_PLAYER) {
                actor->freezeTimer = 3;
            }
        }
        return false;
    }
    slot->cursor++;
    frame = FrameAt(slot, slot->cursor);
    if (frame != NULL) {
        ApplyFrame(slot, frame);
    }
    if (actor->category != ACTORCAT_PLAYER) {
        actor->freezeTimer = 3;
    }
    return true;
}

static bool IsScrubbing(Actor* actor) {
    RecordedActor* slot = FindSlot(actor);

    return slot != NULL && slot->isScrubbing;
}

static bool HasEnoughHistory(Actor* actor) {
    RecordedActor* slot = FindSlot(actor);

    return slot != NULL && slot->count >= HOURGLASS_MIN_HISTORY;
}

static Actor* ScanForTarget(PlayState* play, Player* player, s16 aimYaw) {
    Actor* best = NULL;
    s16 bestError = HOURGLASS_AIM_CONE;

    for (u8 index = 0; index < ARRAY_COUNT(sTrackedCategories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[sTrackedCategories[index]].head; actor != NULL;
             actor = actor->next) {
            f32 deltaX = actor->world.pos.x - player->actor.world.pos.x;
            f32 deltaZ = actor->world.pos.z - player->actor.world.pos.z;
            f32 distance = sqrtf(SQ(deltaX) + SQ(deltaZ));
            s16 error;

            if (actor->update == NULL || distance <= HOURGLASS_MIN_DIST || distance > HOURGLASS_TRACK_RANGE ||
                !IsRecallable(player, actor) || !HasEnoughHistory(actor)) {
                continue;
            }
            error = ABS((s16)(Math_Atan2S(deltaZ, deltaX) - aimYaw));
            if (error < bestError) {
                bestError = error;
                best = actor;
            }
        }
    }
    return best;
}

static void SpawnSand(PlayState* play, Actor* actor) {
    Vec3f still = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primary = { 255, 200, 80, 255 };
    Color_RGBA8 secondary = { 180, 110, 20, 255 };

    for (u8 particle = 0; particle < 2; particle++) {
        Vec3f pos = { actor->world.pos.x + Rand_CenteredFloat(30.0f),
                      actor->world.pos.y + 10.0f + Rand_ZeroFloat(30.0f),
                      actor->world.pos.z + Rand_CenteredFloat(30.0f) };

        EffectSsKiraKira_SpawnFocused(play, &pos, &still, &still, &primary, &secondary, 500, 12);
    }
}

static void StrikeWhatItPassesThrough(PlayState* play, Actor* target, Vec3f* from) {
    if (Math_Vec3f_DistXYZ(from, &target->world.pos) < HOURGLASS_IMPACT_SPEED) {
        return;
    }
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sImpactCollider);
        sIsColliderReady = true;
    }
    Collider_SetCylinder(play, &sImpactCollider, target, &sImpactColliderInit);
    Collider_UpdateCylinder(target, &sImpactCollider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sImpactCollider.base);
}

static bool IsLinkBusy(Player* player) {
    return (player->stateFlags1 & HOURGLASS_BLOCKING_STATES) != 0;
}

static bool WasHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return isHurt;
}

static void Report(const char* what) {
    LUSLOG_INFO("%s", what);
}

static void ReportPool(void) {
    char line[128];
    u8 used = 0;
    s16 best = 0;

    for (u8 index = 0; index < HOURGLASS_SLOTS; index++) {
        if (sSlots[index].isUsed) {
            used++;
            best = MAX(best, sSlots[index].count);
        }
    }
    snprintf(line, sizeof(line), "hourglass: nothing to recall (slots %d, longest history %d frames)", used, best);
    Report(line);
}

static void PlayError(Player* player) {
    PlaySfxAt(NA_SE_SY_ERROR, &player->actor.world.pos);
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    sApi->ReleaseTimeControl(HOURGLASS_TIME_OWNER);
    sOffer = NULL;
}

static void EndSelfRewind(Player* player, PlayState* play, bool isBackToAim) {
    if (!sIsSelfRewinding) {
        return;
    }
    EndScrub(&player->actor);
    StopCueLoop();
    sIsSelfRewinding = false;
    player->invincibilityTimer = 0;
    sPreviousInvincibility = 0;
    if (isBackToAim) {
        Z64Aiming_Request(player, play);
    }
}

static void Stow(Player* player, PlayState* play) {
    if (sState == HOURGLASS_RECALLING && sTarget != NULL) {
        EndScrub(sTarget);
    }
    if (sIsSelfRewinding) {
        EndSelfRewind(player, play, false);
    }
    if (sState == HOURGLASS_AIMING) {
        StopAiming(player, play);
    }
    StopCueLoop();
    PlayCue(HOURGLASS_CUE_EXIT);
    sState = HOURGLASS_IDLE;
    sTarget = NULL;
    sRecallFrames = 0;
}

static void RaiseHourglass(PlayState* play, Player* player) {
    if (sState != HOURGLASS_IDLE || IsLinkBusy(player)) {
        return;
    }
    if (!HasMagic(HOURGLASS_MAGIC_ACTIVATION)) {
        PlayError(player);
        return;
    }
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sPreviousInvincibility = player->invincibilityTimer;
    sOffer = NULL;
    sState = HOURGLASS_AIMING;
    Player_SetupAction(play, player, AimAction, 1);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)sCastAnim);
    Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
    sApi->RequestTimeControl(HOURGLASS_TIME_OWNER, HOURGLASS_TIME_PRIORITY, 0.0f, true);
    PlayCue(HOURGLASS_CUE_ACTIVATE);
}

// A one-shot, never a loop: the engine takes the upper skeleton back when the clip ends.
static void StartCastPose(Player* player, PlayState* play) {
    LinkAnimation_PlayOnce(play, &player->upperSkelAnime, (LinkAnimationHeader*)sCastAnim);
    sCastPoseFrames = HOURGLASS_CAST_POSE_FRAMES;
}

static void TickCastPose(Player* player, PlayState* play) {
    if (sCastPoseFrames <= 0) {
        return;
    }
    sCastPoseFrames--;
    LinkAnimation_Update(play, &player->upperSkelAnime);
}

static void BeginRecall(Player* player, PlayState* play, Actor* target) {
    RecordedActor* slot = FindSlot(target);
    char line[128];

    snprintf(line, sizeof(line), "hourglass: recall actor 0x%03X, %d frames banked, magic %d", target->id,
             slot == NULL ? -1 : slot->count, gSaveContext.magic);
    Report(line);
    if (!HasMagic(HOURGLASS_MAGIC_ACTIVATION) || !BeginScrub(target)) {
        PlayError(player);
        return;
    }
    SpendMagic(play, HOURGLASS_MAGIC_ACTIVATION);
    StopAiming(player, play);
    sTarget = target;
    sRecallFrames = 0;
    sRecallStartFrame = play->state.frames;
    sState = HOURGLASS_RECALLING;
    StartCastPose(player, play);
    PlayCue(HOURGLASS_CUE_TARGET);
    StartCueLoop();
    Rumble_Request(300.0f, 150, 20, 60);
}

static bool TickSelfRewind(Player* player, PlayState* play, bool isHeld, bool isBackToAim) {
    if (!isHeld) {
        if (sIsSelfRewinding) {
            EndSelfRewind(player, play, isBackToAim);
        }
        return false;
    }
    if (!sIsSelfRewinding) {
        if (!HasMagic(HOURGLASS_MAGIC_ACTIVATION) || !BeginScrub(&player->actor)) {
            return false;
        }
        SpendMagic(play, HOURGLASS_MAGIC_ACTIVATION);
        sIsSelfRewinding = true;
        sRecallFrames = 0;
        sOffer = NULL;
        Z64Aiming_Release(player, play);
        StartCueLoop();
    }
    sRecallFrames++;
    if ((sRecallFrames % HOURGLASS_DRAIN_INTERVAL) == 0 && !SpendMagic(play, HOURGLASS_DRAIN_COST)) {
        EndSelfRewind(player, play, isBackToAim);
        return false;
    }
    player->invincibilityTimer = -HOURGLASS_SELF_IFRAMES;
    Player_ZeroSpeedXZ(player);
    ScrubBack(&player->actor);
    SpawnSand(play, &player->actor);
    return true;
}

static Actor* OfferedActor(Player* player, PlayState* play, s16 aimYaw) {
    Actor* locked = player->focusActor;

    if (locked != NULL && locked->update != NULL && IsRecallable(player, locked) && HasEnoughHistory(locked)) {
        return locked;
    }
    return ScanForTarget(play, player, aimYaw);
}

static void ResolveAimingHand(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (gPlayState == NULL || player != GET_PLAYER(gPlayState)) {
        return;
    }

    if (sState == HOURGLASS_AIMING && !sIsSelfRewinding && limbIndex == PLAYER_LIMB_R_HAND &&
        (player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON)) {
        *dList = gPhantomHourglassDL;
    }
}

static void AimAction(Player* player, PlayState* play) {
    u16 buttonMask = FindEquippedButtons();

    if (buttonMask == 0 || !HasMagic(HOURGLASS_MAGIC_ACTIVATION) || IsLinkBusy(player) || WasHurt(player) ||
        (play->state.input[0].press.button & (BTN_B | BTN_CUP))) {
        Stow(player, play);
        func_80839FFC(player, play);
        return;
    }
    if (TickSelfRewind(player, play, (play->state.input[0].cur.button & BTN_R) != 0, true)) {
        return;
    }
    Z64Aiming_Update(player, play);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_Update(play, &player->skelAnime);
    LinkAnimation_Update(play, &player->upperSkelAnime);
    sOffer = OfferedActor(player, play, player->actor.focus.rot.y);
    if (!(play->state.input[0].press.button & buttonMask)) {
        return;
    }
    if (sOffer == NULL) {
        ReportPool();
        PlayError(player);
        return;
    }
    BeginRecall(player, play, sOffer);
    func_80839FFC(player, play);
}

static void TickRecalling(Player* player, PlayState* play, u16 buttonMask) {
    Vec3f before;

    DropDeadSlots(play);
    if (sTarget == NULL || !IsScrubbing(sTarget) || IsLinkBusy(player) || WasHurt(player) ||
        sRecallFrames >= HOURGLASS_FRAMES) {
        Stow(player, play);
        return;
    }
    sRecallFrames++;
    if ((sRecallFrames % HOURGLASS_DRAIN_INTERVAL) == 0 && !SpendMagic(play, HOURGLASS_DRAIN_COST)) {
        Stow(player, play);
        return;
    }
    before = sTarget->world.pos;
    if (!ScrubBack(sTarget)) {
        Stow(player, play);
        return;
    }
    TickCastPose(player, play);
    StrikeWhatItPassesThrough(play, sTarget, &before);
    SpawnSand(play, sTarget);
    // The aim action confirms earlier this frame, so its press is still on the pad and would read here as the cancel.
    if (sRecallStartFrame == play->state.frames) {
        return;
    }
    if (play->state.input[0].press.button & buttonMask) {
        Stow(player, play);
    }
}

static void TickRewinding(Player* player, PlayState* play, u16 buttonMask) {
    bool isHeld = (play->state.input[0].cur.button & buttonMask) != 0;

    if (IsLinkBusy(player) || !TickSelfRewind(player, play, isHeld, false)) {
        Stow(player, play);
    }
}

static void StartRewinding(Player* player, PlayState* play) {
    sState = HOURGLASS_REWINDING;
    if (!TickSelfRewind(player, play, true, false)) {
        sState = HOURGLASS_IDLE;
        PlayError(player);
    }
}

static bool IsReadyInHand(Player* player) {
    return sIsInHand && sState == HOURGLASS_IDLE && player->heldItemAction == PLAYER_IA_CUSTOM &&
           !(player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) && !IsLinkBusy(player);
}

// What keeps its colour is what a press recalls; holding R skips it so the press rewinds Link instead.
static void MarkOffer(Player* player, PlayState* play) {
    if (sState != HOURGLASS_IDLE) {
        return;
    }
    if (!IsReadyInHand(player) || (play->state.input[0].cur.button & BTN_R)) {
        sOffer = NULL;
        return;
    }
    sOffer = OfferedActor(player, play, player->actor.shape.rot.y);
}

// With nothing offered the press rewinds Link himself, which works in midair too.
static void UseHourglass(PlayState* play, Player* player) {
    if (sOffer != NULL && sOffer->update != NULL) {
        BeginRecall(player, play, sOffer);
        return;
    }
    StartRewinding(player, play);
}

static void RunPendingUse(Player* player, PlayState* play) {
    if (!sIsUsePending) {
        return;
    }
    sIsUsePending = false;
    if (IsReadyInHand(player)) {
        UseHourglass(play, player);
    }
}

static void AimOnCUp(Player* player, PlayState* play) {
    if (IsReadyInHand(player) && (play->state.input[0].press.button & BTN_CUP)) {
        RaiseHourglass(play, player);
    }
}

static void TakeOutHourglass(PlayState* play, Player* player) {
    sIsInHand = true;
    sIsUsePending = false;
}

static int32_t HoldHourglass(Player* player, PlayState* play) {
    return 0;
}

// Acts from the end of the player's update: an action set inside the item-button pass is overwritten the same frame.
static void RequestUse(Player* player, PlayState* play) {
    sIsUsePending = sState == HOURGLASS_IDLE;
}

static void PutHourglassAway(Player* player, PlayState* play) {
    if (sState != HOURGLASS_IDLE) {
        Stow(player, play);
    }
    sIsInHand = false;
    sIsUsePending = false;
    sOffer = NULL;
}

static void TickHourglass(void) {
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);
    u16 buttonMask;

    if (play == NULL || player == NULL) {
        return;
    }
    if (play->sceneNum != sSceneNum) {
        sSceneNum = play->sceneNum;
        ClearAllSlots();
        sState = HOURGLASS_IDLE;
        sTarget = NULL;
        sOffer = NULL;
        sIsSelfRewinding = false;
        sIsInHand = false;
        sIsUsePending = false;
        Z64Aiming_Drop();
    }
    buttonMask = FindEquippedButtons();
    if (buttonMask == 0) {
        if (sState != HOURGLASS_IDLE) {
            Stow(player, play);
        }
        ClearAllSlots();
        return;
    }
    KeepMixerAlive();
    RecordEveryone(play, player);
    // Anything that takes the player replaces the aim action without telling it, which would leave the world frozen.
    if (sState == HOURGLASS_AIMING && player->actionFunc != AimAction) {
        Stow(player, play);
        return;
    }
    if (sState == HOURGLASS_RECALLING) {
        TickRecalling(player, play, buttonMask);
    } else if (sState == HOURGLASS_REWINDING) {
        TickRewinding(player, play, buttonMask);
    }
    RunPendingUse(player, play);
    AimOnCUp(player, play);
    MarkOffer(player, play);
}

static bool IsDrawnGray(Actor* actor) {
    if (sIsSelfRewinding) {
        return actor == NULL || actor->category != ACTORCAT_PLAYER;
    }
    if (sState == HOURGLASS_AIMING) {
        return actor != NULL && actor == sOffer;
    }
    if (sState == HOURGLASS_RECALLING) {
        return actor == NULL || (actor->category != ACTORCAT_PLAYER && actor != sTarget);
    }
    if (sIsInHand) {
        return actor == NULL || (actor->category != ACTORCAT_PLAYER && actor != sOffer);
    }
    return false;
}

static void ResolveActorGrayscale(Actor* actor, PlayState* play, Color_RGBA8* grayscale) {
    if (IsDrawnGray(actor)) {
        grayscale->r = grayscale->g = grayscale->b = grayscale->a = 255;
    }
}

static void ResolveRoomGrayscale(PlayState* play, Room* room, Color_RGBA8* grayscale) {
    if (IsDrawnGray(NULL)) {
        grayscale->r = grayscale->g = grayscale->b = grayscale->a = 255;
    }
}

static Actor* PathActor(void) {
    if (sState == HOURGLASS_AIMING) {
        return sOffer;
    }
    return sState == HOURGLASS_RECALLING ? sTarget : NULL;
}

static s32 BuildRibbon(PlayState* play, Actor* target) {
    RecordedActor* slot = FindSlot(target);
    Vec3f* eye = &GET_ACTIVE_CAM(play)->eye;
    s32 quads = 0;
    s16 step = slot == NULL ? 0 : slot->cursor;
    RecordedFrame* start = slot == NULL ? NULL : FrameAt(slot, step);

    if (start == NULL) {
        return 0;
    }
    Vec3f from = start->pos;

    for (s32 quad = 0; quad < HOURGLASS_RIBBON_QUADS; quad++) {
        RecordedFrame* next = FrameAt(slot, step + (quad + 1) * HOURGLASS_RIBBON_STEP);
        Vec3f direction;
        Vec3f toCamera;
        Vec3f side;
        f32 length;

        if (next == NULL) {
            break;
        }
        direction.x = next->pos.x - from.x;
        direction.y = next->pos.y - from.y;
        direction.z = next->pos.z - from.z;
        toCamera.x = eye->x - from.x;
        toCamera.y = eye->y - from.y;
        toCamera.z = eye->z - from.z;
        side.x = direction.y * toCamera.z - direction.z * toCamera.y;
        side.y = direction.z * toCamera.x - direction.x * toCamera.z;
        side.z = direction.x * toCamera.y - direction.y * toCamera.x;
        length = sqrtf(SQ(side.x) + SQ(side.y) + SQ(side.z));
        if (length < 0.001f) {
            from = next->pos;
            continue;
        }
        side.x *= HOURGLASS_RIBBON_HALF_WIDTH / length;
        side.y *= HOURGLASS_RIBBON_HALF_WIDTH / length;
        side.z *= HOURGLASS_RIBBON_HALF_WIDTH / length;
        for (u8 corner = 0; corner < 4; corner++) {
            Vec3f* anchor = corner < 2 ? &from : &next->pos;
            f32 sign = (corner % 2) == 0 ? 1.0f : -1.0f;
            Vtx* vertex = &sRibbonVtx[quads * 4 + corner];

            vertex->v.ob[0] = (s16)(anchor->x + side.x * sign);
            vertex->v.ob[1] = (s16)(anchor->y + side.y * sign);
            vertex->v.ob[2] = (s16)(anchor->z + side.z * sign);
            vertex->v.flag = 0;
            vertex->v.tc[0] = vertex->v.tc[1] = 0;
            vertex->v.cn[0] = vertex->v.cn[1] = vertex->v.cn[2] = vertex->v.cn[3] = 255;
        }
        quads++;
        from = next->pos;
    }
    return quads;
}

static void DrawRibbon(PlayState* play, Actor* target) {
    s32 quads = BuildRibbon(play, target);

    if (quads == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH | G_LIGHTING);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 220, 40, 110);
    for (s32 quad = 0; quad < quads; quad++) {
        gSPVertex(POLY_XLU_DISP++, (uintptr_t)&sRibbonVtx[quad * 4], 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawHourglassInHand(Player* player, PlayState* play) {
    Vec3f* hand = &player->bodyPartsPos[PLAYER_BODYPART_L_HAND];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Translate(hand->x, hand->y - HOURGLASS_HAND_DROP, hand->z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(HOURGLASS_HELD_SCALE, HOURGLASS_HELD_SCALE, HOURGLASS_HELD_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gPhantomHourglassDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawHourglass(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    if (player != GET_PLAYER(play)) {
        RemoteVisualState visual;
        if (!ReadRemoteVisual(player, &visual)) {
            return;
        }
        DrawHourglassInHand(player, play);
        return;
    }

    Actor* path = PathActor();

    if (sState == HOURGLASS_IDLE && !sIsInHand) {
        return;
    }
    if (!(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) && !(player->stateFlags1 & PLAYER_STATE1_FIRST_PERSON)) {
        DrawHourglassInHand(player, play);
    }
    if (path != NULL) {
        DrawRibbon(play, path);
    }
    if (sState == HOURGLASS_AIMING && !sIsSelfRewinding) {
        Z64Aiming_DrawReticle(play, player, HOURGLASS_TRACK_RANGE,
                              (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_HOOKSHOT, Z64AIMING_TINT_CONTROL });
    }
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(HOURGLASS_GIVE_SCALE, HOURGLASS_GIVE_SCALE, HOURGLASS_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gPhantomHourglassDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ForgetSceneState(int16_t sceneNum) {
    ClearAllSlots();
    sState = HOURGLASS_IDLE;
    sTarget = NULL;
    sOffer = NULL;
    sIsSelfRewinding = false;
    sSceneNum = sceneNum;
    Z64Aiming_Drop();
}

static const SOHCustomItemRandomizer sRandomizerItem = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, HOURGLASS_KEY, HOURGLASS_KEY, "",
};

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const SOHNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, "nei.phantom_hourglass.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneNum || visual->flags[0] > 255 ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        Player* player = GET_PLAYER(play);
        visual.scene = play->sceneNum;
        visual.adult = LINK_IS_ADULT;
        visual.active = (sState != HOURGLASS_IDLE);
        if (visual.active) {}
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.phantom_hourglass.visual", &visual, sizeof(visual))) {
            sLastRemoteVisual = visual;
            sRemoteVisualSent = true;
            sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
        }
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    DrawHourglass(actor, play);
}

static void InitRemoteVisuals(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterNetChannel) || !SOH_MOD_API_HAS(sApi, SetNetPeerState) ||
        !SOH_MOD_API_HAS(sApi, GetNetPeerState) || !SOH_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.phantom_hourglass.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        SOH_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemotePlayer);
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    if (!SOH_MOD_API_HAS(sApi, RequestTimeControl)) {
        return;
    }

    SOHCustomItemDefinition hourglass = Z64Items_Define(HOURGLASS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&hourglass, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&hourglass, 0, 17, 0);
    Z64Items_SetTextbox(&hourglass, "You got the %rPhantom&Hourglass%w! Sand that&remembers where everything&"
                                    "has been.^Ready it with %y\xA1%w: the world&greys but what you face. Press %y\xA1%w&"
                                    "and it walks its path back.^With nothing there, or %y\xA3%w held,&"
                                    "%gyou%w go back while %y\xA1%w is held.&"
                                    "%y\xA5%w stops time to aim. It spends&%gmagic%w.");
    Z64Items_SetPauseText(&hourglass, "%rPhantom Hourglass&%wPress %y\xA1%w to recall what keeps colour;&"
                                      "hold it to rewind yourself. %y\xA5%w aims.");
    Z64Items_SetAction(&hourglass, TakeOutHourglass, HoldHourglass);
    Z64Items_SetHeldCallbacks(&hourglass, RequestUse, PutHourglassAway, NULL);
    hourglass.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    hourglass.getItemEntry.drawFunc = DrawGetItem;

    Z64Items_SetLogic(&hourglass, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &hourglass)) {
        return;
    }
    LoadCueSounds();
    if (SOH_MOD_API_HAS(sApi, RegisterAudioMix)) {
        sApi->RegisterAudioMix(MixCues);
    }
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickHourglass);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnActorResolveGrayscale, ResolveActorGrayscale);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, ResolveAimingHand);
    SOH_REGISTER_HOOK(sApi, OnRoomResolveGrayscale, ResolveRoomGrayscale);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawHourglass);
}
