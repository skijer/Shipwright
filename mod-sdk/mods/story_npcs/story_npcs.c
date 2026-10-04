// Static story NPCs. Port of the first part of marsh6487's static story actor catalogue (codex/static-story-actors-poc4
// .. poc14, soh/src/overlays/actors/ovl_En_Viewer/static_story_actor.c). These are scenery: they stand, sing or
// dance in the pose their param asks for, blink, and never run the story code of the NPC they borrow the model of,
// so they cannot grant anything, start cutscenes or set flags.
//
// The original lives inside En_Viewer ("Cutscene Actor" in Prelude, params 0x7Fxx / 0x7Exx). A mod cannot change
// En_Viewer, so there are two ways in:
//   * a scene that already places En_Viewer with one of those params gets the NPC instead: OnActorInit swaps it;
//   * `spawn custom_actor "marsh6487.story_npc" <params>` places one by hand.
//
// Params (same numbers as the catalogue):
//   0x7F02 child Malon idle    0x7F12 / 0x7F22 child Malon singing
//   0x7F03 Saria arms at sides 0x7F13 Saria hands behind back   0x7F23 Saria playing ocarina
//   0x7F0A adult Malon idle    0x7F2A / 0x7F3A adult Malon singing (0x7F1A, with the basket, shows idle)
//   0x7E01 Darunia idle        0x7E11 Darunia dancing
//   0x7E02 Nabooru hands on hips
// The models are OoT's own, loaded by path. Not ported yet: head and torso tracking, talking, soft collision, the
// other characters (Impa, Zelda, Sheik, Ruto, Kokiri, Fado, Great Fairy, Ganondorf and the Majora's Mask cast).

#include "soh/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_ma1/object_ma1.h"
#include "objects/object_ma2/object_ma2.h"
#include "objects/object_sa/object_sa.h"
#include "objects/object_du/object_du.h"
#include "objects/object_nb/object_nb.h"
#include "overlays/actors/ovl_En_Ma2/z_en_ma2.h"

#define STORY_NPC_KEY "marsh6487.story_npc"
#define PARAM_PREFIX_LEGACY 0x7F00
#define PARAM_PREFIX_EXPANDED 0x7E00
#define BLINK_MIN 30
#define BLINK_RANGE 30
#define MAX_EYE 3

typedef enum {
    NPC_CHILD_MALON,
    NPC_SARIA,
    NPC_ADULT_MALON,
    NPC_DARUNIA,
    NPC_NABOORU,
    NPC_COUNT,
} NpcKind;

typedef struct {
    const char* skeleton;
    const char* poses[4];
    u8 poseCount;
    const char* eyes[MAX_EYE];
    const char* mouth; // an open or neutral mouth texture, or NULL
    f32 scale;
    s32 hiddenLimbs[2];
} NpcDefinition;

static const NpcDefinition sNpcs[NPC_COUNT] = {
    [NPC_CHILD_MALON] = { gMalonChildSkel,
                          { gMalonChildIdleAnim, gMalonChildSingAnim, gMalonChildSingAnim },
                          3,
                          { gMalonChildEyeOpenTex, gMalonChildEyeHalfTex, gMalonChildEyeClosedTex },
                          gMalonChildNeutralMouthTex,
                          0.01f,
                          { 2, 5 } },
    [NPC_SARIA] = { gSariaSkel,
                    { gSariaWaitArmsToSideAnim, gSariaHandsBehindBackWaitAnim, gSariaPlayingOcarinaAnim },
                    3,
                    { gSariaEyeOpenTex, gSariaEyeHalfTex, gSariaEyeClosedTex },
                    gSariaMouthClosed2Tex,
                    0.01f,
                    { -1, -1 } },
    [NPC_ADULT_MALON] = { gMalonAdultSkel,
                          { gMalonAdultIdleAnim, gMalonAdultIdleAnim, gMalonAdultSingAnim, gMalonAdultSingAnim },
                          4,
                          { gMalonAdultEyeOpenTex, gMalonAdultEyeHalfTex, gMalonAdultEyeClosedTex },
                          gMalonAdultMouthNeutralTex,
                          0.01f,
                          { MALON_ADULT_LEFT_THIGH_LIMB, MALON_ADULT_RIGHT_THIGH_LIMB } },
    [NPC_DARUNIA] = { gDaruniaSkel,
                      { gDaruniaIdleAnim, gDaruniaDancingLoop1Anim },
                      2,
                      { gDaruniaEyeOpenTex, gDaruniaEyeOpeningTex, gDaruniaEyeShutTex },
                      gDaruniaMouthSeriousTex,
                      0.01f,
                      { -1, -1 } },
    [NPC_NABOORU] = { gNabooruSkel,
                      { gNabooruStandingHandsOnHipsAnim },
                      1,
                      { gNabooruEyeOpenTex, gNabooruEyeHalfTex, gNabooruEyeClosedTex },
                      NULL,
                      0.01f,
                      { -1, -1 } },
};

typedef struct StoryNpc {
    Actor actor;
    SkelAnime skelAnime;
    const NpcDefinition* definition;
    s16 blinkTimer;
    u8 eyeIndex;
} StoryNpc;

static const char* const sRequiredHooks[] = { "OnActorInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static int16_t sActorId = -1;

// params -> character and pose; false when the params are not one of ours.
static bool Decode(s16 params, NpcKind* kind, u8* pose) {
    u16 prefix = (u16)params & 0xFF00;
    u16 id = (u16)params & 0x000F;

    *pose = ((u16)params >> 4) & 0xF;
    if (prefix == PARAM_PREFIX_EXPANDED) {
        if (id == 1) {
            *kind = NPC_DARUNIA;
        } else if (id == 2) {
            *kind = NPC_NABOORU;
        } else {
            return false;
        }
        return true;
    }
    if (prefix == PARAM_PREFIX_LEGACY) {
        switch (id) {
            case 2:
                *kind = NPC_CHILD_MALON;
                return true;
            case 3:
                *kind = NPC_SARIA;
                return true;
            case 0xA:
                *kind = NPC_ADULT_MALON;
                return true;
            default:
                return false;
        }
    }
    return false;
}

static void StoryNpc_Init(Actor* thisx, PlayState* play) {
    StoryNpc* this = (StoryNpc*)thisx;
    NpcKind kind;
    u8 pose;

    if (!Decode(thisx->params, &kind, &pose)) {
        Actor_Kill(thisx);
        return;
    }
    this->definition = &sNpcs[kind];
    pose = MIN(pose, this->definition->poseCount - 1);

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    Actor_SetScale(thisx, this->definition->scale);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)this->definition->skeleton, NULL, NULL, NULL, 0);
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)this->definition->poses[pose]);
    this->blinkTimer = BLINK_MIN + (s16)Rand_ZeroFloat(BLINK_RANGE);
    thisx->params = 0;
}

static void StoryNpc_Destroy(Actor* thisx, PlayState* play) {
    StoryNpc* this = (StoryNpc*)thisx;

    SkelAnime_Free(&this->skelAnime, play);
}

static void StoryNpc_Update(Actor* thisx, PlayState* play) {
    StoryNpc* this = (StoryNpc*)thisx;

    SkelAnime_Update(&this->skelAnime);
    if (--this->blinkTimer <= 0) {
        // Open, half, closed, half, open.
        this->eyeIndex++;
        if (this->eyeIndex >= 4) {
            this->eyeIndex = 0;
            this->blinkTimer = BLINK_MIN + (s16)Rand_ZeroFloat(BLINK_RANGE);
        } else {
            this->blinkTimer = 2;
        }
    }
}

static s32 StoryNpc_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    StoryNpc* this = (StoryNpc*)thisx;

    if (limbIndex == this->definition->hiddenLimbs[0] || limbIndex == this->definition->hiddenLimbs[1]) {
        *dList = NULL;
    }
    return false;
}

static void StoryNpc_Draw(Actor* thisx, PlayState* play) {
    static const u8 blinkFrames[] = { 0, 1, 2, 1 };
    StoryNpc* this = (StoryNpc*)thisx;
    const NpcDefinition* definition = this->definition;
    const char* eye = definition->eyes[blinkFrames[this->eyeIndex & 3]];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // Eyes sit on segments 8 and 9 (Saria's second eye too) and the mouth on 9 or 10, as in each NPC's own draw.
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eye));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(definition->mouth != NULL ? definition->mouth : eye));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(definition->mouth != NULL ? definition->mouth : eye));
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, StoryNpc_OverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

// A scene that places En_Viewer with one of the catalogue params gets the NPC in its place.
static void ReplaceViewer(void* actorPtr) {
    Actor* viewer = (Actor*)actorPtr;
    NpcKind kind;
    u8 pose;
    PlayState* play = gPlayState;

    if (play == NULL || sActorId < 0 || !Decode(viewer->params, &kind, &pose)) {
        return;
    }
    Actor_Spawn(&play->actorCtx, play, sActorId, viewer->world.pos.x, viewer->world.pos.y, viewer->world.pos.z,
                viewer->world.rot.x, viewer->world.rot.y, 0, viewer->params);
    Actor_Kill(viewer);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = STORY_NPC_KEY;
    definition.description = "Static Story NPC";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(StoryNpc);
    definition.init = StoryNpc_Init;
    definition.destroy = StoryNpc_Destroy;
    definition.update = StoryNpc_Update;
    definition.draw = StoryNpc_Draw;
    sActorId = sApi->RegisterActor(&definition);
    if (sActorId >= 0) {
        SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_VIEWER, ReplaceViewer);
    }
}
