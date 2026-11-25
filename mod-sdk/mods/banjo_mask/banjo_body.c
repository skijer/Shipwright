#include "banjo_body.h"

#include <math.h>
#include <stdlib.h>

#include "banjo_animation_ids.h"
#include "banjo_audio.h"
#include "banjo_buttons.h"
#include "banjo_pads.h"
#include "banjo_physics.h"
#include "banjo_poses.h"
#include "banjo_render.h"
#include "banjo_rom.h"
#include "banjo_surfaces.h"
#include "banjo_world.h"
#include "bk_mesh.h"

#define CLIP_CACHE_SIZE 48
#define MODEL_COUNT 2
#define NO_SCENE (-1)
#define NO_MODEL (-1)
#define BANJO_HEIGHT 49.0f
#define FOOT_SPREAD 6.0f
#define RADIANS_PER_BINANG (3.14159265f / 32768.0f)
#define FIGURE_EYES_OPEN 1
// Game units per BK unit: the figure stands about as tall as a get-item model.
#define FIGURE_SCALE 0.25f
// Half of Banjo's height in BK units, so the figure floats on its middle like other get-items.
#define FIGURE_HALF_HEIGHT 60.0f

typedef struct CachedClip {
    uint16_t id;
    BkAnimation clip;
} CachedClip;

typedef struct BanjoFrame {
    uint16_t model;
    uint16_t animation;
    float frame;
    int16_t selectors[BK_SELECTOR_COUNT];
    float pitchYawRoll[3];
    Vec3f offset;
} BanjoFrame;

static const uint16_t kModelIds[MODEL_COUNT] = { BANJO_MODEL_PLAYER, BANJO_MODEL_CONCERT };

// Selector ids of the player model's appendages (modelappendages.c).
static const uint8_t kKazooieUpperHalf[] = { 0x01, 0x09, 0x0C, 0x0F };
static const uint8_t kKazooieFeet[] = { 0x02, 0x0A, 0x0D, 0x10 };
static const uint8_t kTurboTrainers[] = { 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19 };
static const uint8_t kWadingBoots[] = { 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29 };
// The shoe selectors' second branch wears them, the first goes barefoot.
#define SHOES_OFF 1
#define SHOES_ON 2
#define EYES_FIRST_SELECTOR 0x1A
#define EYES_LAST_SELECTOR 0x21
#define CONCERT_SELECTORS_ON 0x40

// Share of Banjo's height at each of Link's body parts, in PlayerBodyPart order (the same in both games).
static const float kBodyPartHeights[] = {
    0.5f, 0.4f, 0.2f, 0.0f, 0.4f, 0.2f, 0.0f, 0.85f, 0.95f, 0.75f, 0.7f, 0.6f, 0.5f, 0.7f, 0.6f, 0.5f, 0.5f, 0.65f,
};
_Static_assert(ARRAY_COUNT(kBodyPartHeights) == PLAYER_BODYPART_MAX, "one height per body part");

static const SOHModApi* sApi;
static BkModel sModels[MODEL_COUNT];
static BkMatrix* sBones;
static BkBoneChannels sChannels[BK_BONE_MAX];
static CachedClip sClips[CLIP_CACHE_SIZE];
static uint32_t sClipCount;
static BkMesh sMesh;
static int32_t sMeshModel = NO_MODEL;
static float sMeshAngles[3];
static Vec3f sMeshOffset;
static int16_t sSceneId = NO_SCENE;
static BkBoneChannels sFigureChannels[BK_BONE_MAX];
static BkMatrix* sFigureBones;
static BkMesh sFigureMesh;

static int32_t FindModel(uint16_t id) {
    for (int32_t index = 0; index < MODEL_COUNT; index++) {
        if (kModelIds[index] == id && sModels[index].data != NULL) {
            return index;
        }
    }
    return NO_MODEL;
}

bool BanjoBody_Load(void) {
    uint16_t boneCount = 0;

    if (sModels[0].data != NULL) {
        return true;
    }
    if (BanjoRom_Get() == NULL) {
        return false;
    }
    for (int32_t index = 0; index < MODEL_COUNT; index++) {
        if (!BkModel_Load(&sModels[index], kModelIds[index])) {
            return false;
        }
        boneCount = (sModels[index].boneCount > boneCount) ? sModels[index].boneCount : boneCount;
    }
    sBones = calloc(boneCount + 1, sizeof(BkMatrix));
    return sBones != NULL;
}

static const BkAnimation* GetClip(uint16_t id) {
    for (uint32_t index = 0; index < sClipCount; index++) {
        if (sClips[index].id == id) {
            return &sClips[index].clip;
        }
    }
    if (sClipCount == CLIP_CACHE_SIZE) {
        return NULL;
    }

    CachedClip* cached = &sClips[sClipCount];

    if (!BkAnimation_Load(&cached->clip, id)) {
        return NULL;
    }
    cached->id = id;
    sClipCount++;
    return &cached->clip;
}

static bool GetClipRange(uint16_t animation, float* outFirst, float* outLast) {
    const BkAnimation* clip = GetClip(animation);

    if (clip == NULL) {
        return false;
    }
    *outFirst = clip->firstFrame;
    *outLast = clip->lastFrame;
    return true;
}

uint8_t BanjoBody_GetAppendagesOf(uint16_t animation) {
    switch (animation) {
        case BANJO_ANIM_TROT_WALK:
        case BANJO_ANIM_SHOCK_CHARGE:
        case BANJO_ANIM_SHOCK_JUMP:
            return BANJO_APPENDAGE_KAZOOIE | BANJO_APPENDAGE_KAZOOIE_FEET;
        case BANJO_ANIM_TROT_TURBO_WALK:
            return BANJO_APPENDAGE_KAZOOIE | BANJO_APPENDAGE_KAZOOIE_FEET | BANJO_APPENDAGE_TURBO_TRAINERS;
        case BANJO_ANIM_LONGLEG_WALK:
            return BANJO_APPENDAGE_KAZOOIE | BANJO_APPENDAGE_KAZOOIE_FEET | BANJO_APPENDAGE_WADING_BOOTS;
        default:
            return 0;
    }
}

static void SetSelectors(int16_t* selectors, const uint8_t* ids, size_t count, int16_t value) {
    for (size_t index = 0; index < count; index++) {
        selectors[ids[index]] = value;
    }
}

void BanjoBody_FillSelectors(int16_t* selectors, uint16_t model, uint8_t appendages, int16_t eyes) {
    for (int32_t index = 0; index < BK_SELECTOR_COUNT; index++) {
        selectors[index] = (model == BANJO_MODEL_CONCERT && index < CONCERT_SELECTORS_ON) ? 1 : 0;
    }
    if (model != BANJO_MODEL_PLAYER) {
        return;
    }
    SetSelectors(selectors, kKazooieUpperHalf, sizeof(kKazooieUpperHalf), (appendages & BANJO_APPENDAGE_KAZOOIE) != 0);
    SetSelectors(selectors, kKazooieFeet, sizeof(kKazooieFeet), (appendages & BANJO_APPENDAGE_KAZOOIE_FEET) != 0);
    SetSelectors(selectors, kTurboTrainers, sizeof(kTurboTrainers),
                 (appendages & BANJO_APPENDAGE_TURBO_TRAINERS) ? SHOES_ON : SHOES_OFF);
    SetSelectors(selectors, kWadingBoots, sizeof(kWadingBoots),
                 (appendages & BANJO_APPENDAGE_WADING_BOOTS) ? SHOES_ON : SHOES_OFF);
    for (int32_t index = EYES_FIRST_SELECTOR; index <= EYES_LAST_SELECTOR; index++) {
        selectors[index] = eyes;
    }
}

static void SetPoseSelectors(int16_t* selectors, const BanjoPoseFrame* pose) {
    BanjoBody_FillSelectors(selectors, pose->model, pose->appendages, pose->eyes);
}

const BkModel* BanjoBody_GetModel(uint16_t id) {
    const int32_t model = BanjoBody_Load() ? FindModel(id) : NO_MODEL;

    return model != NO_MODEL ? &sModels[model] : NULL;
}

// While the game drives Link, Banjo copies Link's pose through the pose table.
static void ReadGamePose(Player* player, BanjoFrame* frame) {
    BanjoPoseFrame pose;

    BanjoPoses_Follow(player, GetClipRange, &pose);
    frame->model = pose.model;
    frame->animation = pose.animation;
    frame->frame = pose.frame;
    SetPoseSelectors(frame->selectors, &pose);
    frame->pitchYawRoll[0] = 0.0f;
    frame->pitchYawRoll[1] = player->actor.shape.rot.y / BANJO_BINANG_PER_DEGREE;
    frame->pitchYawRoll[2] = 0.0f;
    frame->offset = (Vec3f){ 0.0f, 0.0f, 0.0f };
}

// BK's anim timer runs 0..1 across the clip's frames (animationfile.c).
static bool ReadPhysicsPose(BanjoFrame* frame) {
    const LibBkState* state = BanjoPhysics_GetState();
    float first;
    float last;

    if (!state->isModelVisible || !GetClipRange((uint16_t)state->animationId, &first, &last)) {
        return false;
    }
    frame->model = (uint16_t)state->modelId;
    frame->animation = (uint16_t)state->animationId;
    frame->frame = first + state->animationTimer * (last - first);
    for (int32_t index = 0; index < BK_SELECTOR_COUNT; index++) {
        frame->selectors[index] = state->selectors[index];
    }
    frame->pitchYawRoll[0] = state->modelPitch;
    frame->pitchYawRoll[1] = state->modelYaw;
    frame->pitchYawRoll[2] = state->modelRoll;
    frame->offset.x = state->modelOffset[0] * BANJO_GAME_UNITS_PER_BK_UNIT;
    frame->offset.y = state->modelOffset[1] * BANJO_GAME_UNITS_PER_BK_UNIT;
    frame->offset.z = state->modelOffset[2] * BANJO_GAME_UNITS_PER_BK_UNIT;
    return true;
}

static void BuildMesh(const BanjoFrame* frame) {
    const int32_t model = FindModel(frame->model);
    const BkAnimation* clip = GetClip(frame->animation);

    if (model == NO_MODEL || clip == NULL) {
        return;
    }
    BkAnimation_Sample(clip, frame->frame, sChannels);
    BkModel_ComputeBoneMatrices(&sModels[model], sChannels, sBones);
    BkMesh_Build(&sMesh, &sModels[model], sBones, frame->selectors);
    sMeshModel = model;
    for (int32_t axis = 0; axis < 3; axis++) {
        sMeshAngles[axis] = frame->pitchYawRoll[axis];
    }
    sMeshOffset = frame->offset;
}

// A new scene restarts BK's player from where the game put Link.
static bool EnsureSpawnedInScene(PlayState* play, Player* player) {
    if (sSceneId == BANJO_SCENE_ID(play)) {
        return true;
    }
    if (!BanjoPhysics_Spawn(player)) {
        return false;
    }
    BanjoPoses_Reset();
    BanjoSurfaces_ClearActorColliders();
    sSceneId = BANJO_SCENE_ID(play);
    return true;
}

void BanjoBody_Register(const SOHModApi* api) {
    sApi = api;
}

void BanjoBody_Reset(PlayState* play, Player* player) {
    BanjoPoses_Reset();
    BanjoSurfaces_ClearActorColliders();
    sMesh.triangleCount = 0;
    sMeshModel = NO_MODEL;
    sSceneId = NO_SCENE;
}

void BanjoBody_Update(PlayState* play, Player* player) {
    BanjoFrame frame;

    if (!BanjoBody_Load() || !EnsureSpawnedInScene(play, player)) {
        return;
    }
    if (sApi->IsFormSuspended()) {
        BanjoPhysics_Park(player);
        ReadGamePose(player, &frame);
        BuildMesh(&frame);
        return;
    }
    if (!BanjoButtons_Update(play, player)) {
        return;
    }
    BanjoPads_MarkStanding(play, player);
    BanjoPhysics_Drive(play, player);
    BanjoAudio_PlayQueued();
    if (ReadPhysicsPose(&frame)) {
        BuildMesh(&frame);
    } else {
        sMeshModel = NO_MODEL;
    }
}

// Player_Draw sets these from Link's limbs and the takeover skips it, so Navi, the feet shadow and targeting
// would stay where Link transformed.
static void PlaceLinkBodyParts(Player* player, Vec3f base) {
    const float yaw = player->actor.shape.rot.y * RADIANS_PER_BINANG;
    const float sideX = cosf(yaw) * FOOT_SPREAD;
    const float sideZ = -sinf(yaw) * FOOT_SPREAD;

    for (int32_t part = 0; part < PLAYER_BODYPART_MAX; part++) {
        player->bodyPartsPos[part] = (Vec3f){ base.x, base.y + BANJO_HEIGHT * kBodyPartHeights[part], base.z };
    }
    player->actor.shape.feetPos[FOOT_LEFT] = (Vec3f){ base.x + sideX, base.y, base.z + sideZ };
    player->actor.shape.feetPos[FOOT_RIGHT] = (Vec3f){ base.x - sideX, base.y, base.z - sideZ };
    player->actor.focus.pos = player->bodyPartsPos[PLAYER_BODYPART_HEAD];
}

void BanjoBody_Draw(PlayState* play, Player* player) {
    const Vec3f position = { player->actor.world.pos.x + sMeshOffset.x, player->actor.world.pos.y + sMeshOffset.y,
                             player->actor.world.pos.z + sMeshOffset.z };

    BanjoPads_Draw(play);
    BanjoSurfaces_CaptureActorColliders(play);
    PlaceLinkBodyParts(player, position);
    if (sMeshModel == NO_MODEL) {
        return;
    }

    const BanjoPlacement placement = { position,
                                       { sMeshAngles[0], sMeshAngles[1], sMeshAngles[2] },
                                       BANJO_GAME_UNITS_PER_BK_UNIT };

    BanjoRender_DrawMesh(play, &sMesh, &sModels[sMeshModel], &placement);
}

// The figure keeps its own bones and mesh: the player's body rebuilds its own every frame.
void BanjoBody_DrawFigure(PlayState* play, uint16_t animation, bool isKazooieOut) {
    const int32_t model = BanjoBody_Load() ? FindModel(BANJO_MODEL_PLAYER) : NO_MODEL;
    const BkAnimation* clip = model != NO_MODEL ? GetClip(animation) : NULL;
    int16_t selectors[BK_SELECTOR_COUNT];

    if (clip == NULL) {
        return;
    }
    if (sFigureBones == NULL) {
        sFigureBones = calloc(sModels[model].boneCount + 1, sizeof(BkMatrix));
    }
    if (sFigureBones == NULL) {
        return;
    }

    const float span = (float)(clip->lastFrame - clip->firstFrame);

    BkAnimation_Sample(clip, clip->firstFrame + (span > 0.0f ? fmodf((float)play->state.frames, span) : 0.0f),
                       sFigureChannels);
    BkModel_ComputeBoneMatrices(&sModels[model], sFigureChannels, sFigureBones);
    BanjoBody_FillSelectors(selectors, BANJO_MODEL_PLAYER,
                            (isKazooieOut ? BANJO_APPENDAGE_KAZOOIE : 0) | BanjoBody_GetAppendagesOf(animation),
                            FIGURE_EYES_OPEN);
    BkMesh_Build(&sFigureMesh, &sModels[model], sFigureBones, selectors);
    Matrix_Push();
    Matrix_Translate(0.0f, -FIGURE_HALF_HEIGHT * FIGURE_SCALE, 0.0f, MTXMODE_APPLY);
    BanjoRender_DrawMeshHere(play, &sFigureMesh, &sModels[model], FIGURE_SCALE);
    Matrix_Pop();
}
