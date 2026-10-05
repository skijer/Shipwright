// Young Epona. Port of marsh6487's poc/child-epona-song-riding and feat/nei-young-epona (8466865e, bcb0450e).
//
// The original rewrote z_en_horse.c, z_horse.c and z_player.c. This mod leaves the engine alone: it registers its
// own actor whose struct *is* an EnHorse, so the engine's riding code (Player reads rideActor->riderPos,
// animationIdx, curFrame, stateFlags...) works on it unchanged, and it runs the vanilla horse controller
// (EnHorse_Update) as its brain. What changes is the body:
//
//   * Skeleton: the child horse (gChildEponaSkel) replaces Epona's after the vanilla init.
//   * Animations: the controller keeps playing Epona's clips, which only drives its timing. At draw time the pose
//     is re-evaluated from the matching child clip at the same *normalised* time. Idle, whinny, walk, trot and
//     gallop are OoT's own child clips; stop, rear and the two jumps are the Majora's Mask clips (mm.o2r).
//   * Rider: the saddle position is read from the skin's seat vertex instead of Epona's saddle limb.
//   * Mounting: the vanilla mount action runs, then OnPlayerUpdate re-seats child Link with the MM child clips.
//
// Across scenes: while the horse exists its last place is remembered, and written to the mod's storage per save file
// when Link leaves the scene or saves, so returning to that scene finds her where she was left. Riding through a
// scene exit brings her along: the new scene spawns her under Link and mounts him, in the outdoor areas below.
//
// Without mm.o2r the horse still works: the four clips with no OoT child version fall back to the idle clip.

#include "soh/ModApi/ModApi.h"

#include <stdio.h>
#include <string.h>

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "regs.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_horse/object_horse.h"
#include "objects/object_horse_link_child/object_horse_link_child.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include <libultraship/bridge/consolevariablebridge.h>

// The vanilla horse controller. Not in any header, but plain global functions of the game.
extern void EnHorse_Init(Actor* thisx, PlayState* play);
extern void EnHorse_Destroy(Actor* thisx, PlayState* play);
extern void EnHorse_Update(Actor* thisx, PlayState* play);
extern void EnHorse_PostDraw(Actor* thisx, PlayState* play, Skin* skin);

#define YOUNG_EPONA_KEY "marsh6487.young_epona"
#define YOUNG_EPONA_CVAR "gMods.YoungEpona.Enabled"
#define YOUNG_EPONA_SCALE 0.00648f
#define YOUNG_EPONA_SEAT_LIMB 5
#define YOUNG_EPONA_SEAT_VERTEX 120
#define YOUNG_EPONA_SEAT_LIFT 13.0f
#define YOUNG_EPONA_SUMMON_DISTANCE 160.0f
#define ANIM_COUNT 9

#define MM_HORSE "__OTR__objects/object_horse_link_child/"
#define MM_PLAYER "__OTR__objects/gameplay_keep/"
static const ALIGN_ASSET(2) char sMmStop[] = MM_HORSE "object_horse_link_child_Anim_005F64";
static const ALIGN_ASSET(2) char sMmRear[] = MM_HORSE "object_horse_link_child_Anim_004DE8";
static const ALIGN_ASSET(2) char sMmLowJump[] = MM_HORSE "object_horse_link_child_Anim_0035B0";
static const ALIGN_ASSET(2) char sMmHighJump[] = MM_HORSE "object_horse_link_child_Anim_003D38";
static const ALIGN_ASSET(2) char sMmMountLeft[] = MM_PLAYER "gPlayerAnim_cl_uma_leftup";
static const ALIGN_ASSET(2) char sMmMountRight[] = MM_PLAYER "gPlayerAnim_cl_uma_rightup";

// Epona's clip for each animationIdx drives the controller; this table is what is shown instead.
static const char* const sChildClips[ANIM_COUNT] = {
    gChildEponaIdleAnim,    // ENHORSE_ANIM_IDLE
    gChildEponaWhinnyAnim,  // ENHORSE_ANIM_WHINNEY
    sMmStop,                // ENHORSE_ANIM_STOPPING
    sMmRear,                // ENHORSE_ANIM_REARING
    gChildEponaWalkingAnim, // ENHORSE_ANIM_WALK
    gChildEponaTrottingAnim, // ENHORSE_ANIM_TROT
    gChildEponaGallopingAnim, // ENHORSE_ANIM_GALLOP
    sMmLowJump,             // ENHORSE_ANIM_LOW_JUMP
    sMmHighJump,            // ENHORSE_ANIM_HIGH_JUMP
};

// Child Link's offset from the saddle while mounting, per side (left, right): forward, sideways.
static const f32 sMountOffsets[2][2] = { { 22.718237f, 2.3294117f }, { -22.0f, 1.9800001f } };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnSceneInit", "OnLoadGame", "OnSaveFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static int16_t sActorId = -1;
static HorseData sAdultHorseData;
static bool sHaveAdultHorseData;
static bool sWasMounted;

typedef struct {
    s16 scene;
    Vec3s pos;
    s16 yaw;
    u8 valid;
} HorsePlace;

static HorsePlace sLast;   // where the live horse was last frame
static HorsePlace sSaved;  // the stored place for this save file
static bool sRemount;      // Link was riding when he left the last scene
static s16 sHandledScene = -1;

static void RestoreHorse(PlayState* play, Player* player);

static bool IsEnabled(void) {
    return CVarGetInteger(YOUNG_EPONA_CVAR, 1) && !LINK_IS_ADULT;
}

static bool IsMine(Actor* actor) {
    return actor != NULL && actor->id == sActorId;
}

static const char* ClipFor(const char* clip) {
    return ResourceMgr_FileExists(clip) ? clip : gChildEponaIdleAnim;
}

// -- Body ----------------------------------------------------------------------------------------------------------

static void YoungEpona_Init(Actor* thisx, PlayState* play) {
    EnHorse* this = (EnHorse*)thisx;
    s16 realScene = play->sceneNum;

    // Vanilla init removes Epona in Lon Lon Ranch and the stable unless she is owned; this horse is not hers.
    play->sceneNum = SCENE_HYRULE_FIELD;
    EnHorse_Init(thisx, play);
    play->sceneNum = realScene;
    if (thisx->update == NULL) {
        return;
    }

    Skin_Free(play, &this->skin);
    Skin_Init(play, &this->skin, (SkeletonHeader*)gChildEponaSkel, (AnimationHeader*)gEponaIdleAnim);
    Animation_PlayOnce(&this->skin.skelAnime, (AnimationHeader*)gEponaIdleAnim);

    Actor_SetScale(thisx, YOUNG_EPONA_SCALE);
    this->cyl1.dim.radius *= 0.8f;
    this->cyl2.dim.radius *= 0.8f;
    this->jntSph.elements[0].dim.modelSphere.radius *= 0.6f;
}

static void YoungEpona_Destroy(Actor* thisx, PlayState* play) {
    EnHorse_Destroy(thisx, play);
}

static void YoungEpona_Update(Actor* thisx, PlayState* play) {
    EnHorse_Update(thisx, play);
}

// Shows the child clip at the same point of its length as the controller is at in Epona's.
static void ApplyChildPose(EnHorse* this, PlayState* play) {
    SkelAnime* skel = &this->skin.skelAnime;
    const char* clip;
    void* shownAnimation = skel->animation;
    f32 shownFrame = skel->curFrame;
    f32 shownSpeed = skel->playSpeed;
    f32 fraction;

    if (this->animationIdx < 0 || this->animationIdx >= ANIM_COUNT || skel->endFrame <= 0.0f) {
        return;
    }
    clip = ClipFor(sChildClips[this->animationIdx]);
    fraction = CLAMP(skel->curFrame / skel->endFrame, 0.0f, 1.0f);

    skel->animation = (void*)clip;
    skel->curFrame = fraction * Animation_GetLastFrame((void*)clip);
    skel->playSpeed = 0.0f;
    SkelAnime_Update(skel);

    skel->animation = shownAnimation;
    skel->curFrame = shownFrame;
    skel->playSpeed = shownSpeed;
}

static s32 YoungEpona_OverrideLimbDraw(Actor* thisx, PlayState* play, s32 limbIndex, Skin* skin) {
    static const char* const eyes[] = { gChildEponaEyeOpenTex, gChildEponaEyeHalfTex, gChildEponaEyeCloseTex };
    static const u8 blink[] = { 0, 1, 2, 1 };
    EnHorse* this = (EnHorse*)thisx;

    if (limbIndex == 13) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eyes[blink[this->blinkTimer & 3]]));
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return true;
}

static void YoungEpona_PostDraw(Actor* thisx, PlayState* play, Skin* skin) {
    EnHorse* this = (EnHorse*)thisx;

    // Hoof dust and the head position come from the vanilla routine, which indexes Epona's limbs.
    EnHorse_PostDraw(thisx, play, skin);

    if (!(this->stateFlags & ENHORSE_CALC_RIDER_POS) && skin->vtxTable != NULL &&
        skin->vtxTable[YOUNG_EPONA_SEAT_LIMB].buf[0] != NULL && skin->vtxTable[YOUNG_EPONA_SEAT_LIMB].buf[1] != NULL) {
        SkinLimbVtx* body = &skin->vtxTable[YOUNG_EPONA_SEAT_LIMB];
        // Skin_ApplyLimbModifications advances the index after it submits the buffer it just rendered.
        Vtx* seat = &body->buf[body->index ^ 1][YOUNG_EPONA_SEAT_VERTEX];
        Vec3f seatPos = { seat->n.ob[0], seat->n.ob[1], seat->n.ob[2] };

        SkinMatrix_Vec3fMtxFMultXYZ(&skin->mtx, &seatPos, &this->riderPos);
        this->riderPos.y += YOUNG_EPONA_SEAT_LIFT;
        this->riderPos.x -= thisx->world.pos.x;
        this->riderPos.y -= thisx->world.pos.y;
        this->riderPos.z -= thisx->world.pos.z;
    }
}

static void YoungEpona_Draw(Actor* thisx, PlayState* play) {
    EnHorse* this = (EnHorse*)thisx;

    if (this->stateFlags & ENHORSE_INACTIVE) {
        return;
    }
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    this->stateFlags |= ENHORSE_DRAW;
    ApplyChildPose(this, play);
    func_800A6360(thisx, play, &this->skin, YoungEpona_PostDraw, YoungEpona_OverrideLimbDraw,
                  !(this->stateFlags & ENHORSE_JUMPING));
    if (this->postDrawFunc != NULL) {
        this->postDrawFunc(this, play);
    }
}

// -- Summoning ------------------------------------------------------------------------------------------------------

static EnHorse* FindMine(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        if (IsMine(actor) && actor->update != NULL) {
            return (EnHorse*)actor;
        }
    }
    return NULL;
}

// The five areas where vanilla Epona has spawn points to gallop in from. Her own summon code runs there.
static bool HasNativeSpawns(s16 scene) {
    return scene == SCENE_HYRULE_FIELD || scene == SCENE_LAKE_HYLIA || scene == SCENE_GERUDO_VALLEY ||
           scene == SCENE_GERUDOS_FORTRESS;
}

static void Summon(PlayState* play, Player* player) {
    Actor* horse;

    if (HasNativeSpawns(play->sceneNum)) {
        // Inactive (params 2): it waits for the song flag, then gallops in from the nearest spawn point.
        horse = Actor_Spawn(&play->actorCtx, play, sActorId, player->actor.world.pos.x, player->actor.world.pos.y,
                            player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 2);
        if (horse != NULL) {
            DREG(53) = 1; // Horse init clears it; signal only after spawning.
        }
    } else {
        // No spawn points here: the horse comes in front of Link and waits.
        f32 yaw = player->actor.shape.rot.y;
        horse = Actor_Spawn(&play->actorCtx, play, sActorId,
                            player->actor.world.pos.x + Math_SinS(yaw) * YOUNG_EPONA_SUMMON_DISTANCE,
                            player->actor.world.pos.y,
                            player->actor.world.pos.z + Math_CosS(yaw) * YOUNG_EPONA_SUMMON_DISTANCE, 0, yaw + 0x8000,
                            0, 0);
        DREG(53) = 0;
    }
    if (horse != NULL) {
        Audio_PlaySoundGeneral(NA_SE_EV_HORSE_NEIGH, &horse->projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

static bool CanSummon(PlayState* play) {
    return IsEnabled() && sActorId >= 0 && CHECK_QUEST_ITEM(QUEST_SONG_EPONA) &&
           INV_CONTENT(ITEM_OCARINA_FAIRY) != ITEM_NONE && play->csCtx.state == CS_STATE_IDLE;
}

// -- Link ----------------------------------------------------------------------------------------------------------

// The vanilla mount action has just started with the adult clip and offsets. Redo them for child Link.
static void SeatChildLink(PlayState* play, Player* player, EnHorse* horse) {
    s32 side = player->mountSide < 0 ? 0 : 1;
    const char* clip = side == 0 ? sMmMountLeft : sMmMountRight;
    f32 forward = sMountOffsets[side][0];
    f32 sideways = sMountOffsets[side][1];
    f32 cosYaw = Math_CosS(horse->actor.shape.rot.y);
    f32 sinYaw = Math_SinS(horse->actor.shape.rot.y);

    // Null when the clip is missing or is not a player animation (mm.o2r absent or laid out differently).
    LinkAnimationHeader* mountClip = ResourceMgr_LoadPlayerAnimAsHeader(clip);

    if (mountClip == NULL) {
        return;
    }
    player->actor.world.pos.x = horse->actor.world.pos.x + horse->riderPos.x + (forward * cosYaw + sideways * sinYaw);
    player->actor.world.pos.z = horse->actor.world.pos.z + horse->riderPos.z + (sideways * cosYaw - forward * sinYaw);
    LinkAnimation_PlayOnce(play, &player->skelAnime, mountClip);
}

static void WatchLink(void) {
    PlayState* play = gPlayState;
    Player* player;
    EnHorse* horse;
    bool mounted;

    if (play == NULL || sActorId < 0) {
        return;
    }
    player = GET_PLAYER(play);
    horse = FindMine(play);

    if (horse == NULL) {
        sHaveAdultHorseData = false;
        sWasMounted = false;
        RestoreHorse(play, player);
        if (DREG(53) != 0 && CanSummon(play) && player->rideActor == NULL) {
            sAdultHorseData = gSaveContext.horseData;
            sHaveAdultHorseData = true;
            Summon(play, player);
        }
        return;
    }

    // Child riding must not move adult Epona's saved position, which the vanilla dismount and summon code write.
    if (sHaveAdultHorseData) {
        gSaveContext.horseData = sAdultHorseData;
    }
    if (!IsEnabled() && !(player->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        Actor_Kill(&horse->actor); // Link grew up: the child horse goes away.
        return;
    }

    sLast.valid = 1;
    sLast.scene = play->sceneNum;
    sLast.pos.x = (s16)horse->actor.world.pos.x;
    sLast.pos.y = (s16)horse->actor.world.pos.y;
    sLast.pos.z = (s16)horse->actor.world.pos.z;
    sLast.yaw = horse->actor.shape.rot.y;
    // In Lon Lon Ranch the native young Epona stays until ours is out and active, then gives the field to her.
    if (play->sceneNum == SCENE_LON_LON_RANCH && horse->action != ENHORSE_ACT_INACTIVE) {
        Actor* native = Actor_Find(&play->actorCtx, ACTOR_EN_HORSE_LINK_CHILD, ACTORCAT_BG);

        if (native != NULL) {
            Actor_Kill(native);
        }
    }
    mounted = (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) && player->rideActor == &horse->actor;
    if (mounted && !sWasMounted) {
        SeatChildLink(play, player, horse);
    }
    sWasMounted = mounted;
}

static bool IsRideableScene(s16 scene) {
    switch (scene) {
        case SCENE_HYRULE_FIELD:
        case SCENE_LAKE_HYLIA:
        case SCENE_GERUDO_VALLEY:
        case SCENE_GERUDOS_FORTRESS:
        case SCENE_LON_LON_RANCH:
        case SCENE_KAKARIKO_VILLAGE:
        case SCENE_GRAVEYARD:
        case SCENE_ZORAS_RIVER:
        case SCENE_KOKIRI_FOREST:
        case SCENE_SACRED_FOREST_MEADOW:
        case SCENE_ZORAS_FOUNTAIN:
        case SCENE_LOST_WOODS:
        case SCENE_DESERT_COLOSSUS:
        case SCENE_HAUNTED_WASTELAND:
        case SCENE_HYRULE_CASTLE:
        case SCENE_DEATH_MOUNTAIN_TRAIL:
        case SCENE_DEATH_MOUNTAIN_CRATER:
        case SCENE_OUTSIDE_GANONS_CASTLE:
        case SCENE_MARKET_ENTRANCE_DAY:
        case SCENE_MARKET_ENTRANCE_NIGHT:
        case SCENE_MARKET_ENTRANCE_RUINS:
        case SCENE_MARKET_DAY:
        case SCENE_MARKET_NIGHT:
        case SCENE_MARKET_RUINS:
        case SCENE_BACK_ALLEY_DAY:
        case SCENE_BACK_ALLEY_NIGHT:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT:
        case SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS:
            return true;
        default:
            return false;
    }
}

static void StorePlace(void) {
    if (sApi != NULL && sLast.valid) {
        char key[24];

        snprintf(key, sizeof(key), "file%d.place", (int)gSaveContext.fileNum);
        sApi->StorageSet("young_epona", key, &sLast, sizeof(sLast));
        sSaved = sLast;
    }
}

static void ForgetScene(int16_t sceneNum) {
    // The scene that is ending: remember the horse's place, and whether Link was on her.
    sRemount = sWasMounted && IsEnabled();
    StorePlace();
    sLast.valid = 0;
    sWasMounted = false;
    sHandledScene = -1;
}

static void ForgetFile(int32_t fileNum) {
    char key[24];

    sWasMounted = false;
    sHaveAdultHorseData = false;
    sRemount = false;
    sHandledScene = -1;
    sLast.valid = 0;
    memset(&sSaved, 0, sizeof(sSaved));
    snprintf(key, sizeof(key), "file%d.place", (int)fileNum);
    if (sApi->StorageGetSize("young_epona", key) == sizeof(sSaved)) {
        sApi->StorageGet("young_epona", key, &sSaved, sizeof(sSaved));
    }
}

static void SaveGame(int32_t fileNum, int32_t sectionID) {
    StorePlace();
}

// The first frame of a scene with no horse: bring back the one that was left here, or the one Link rode in on.
static void RestoreHorse(PlayState* play, Player* player) {
    Actor* horse;

    if (sHandledScene == play->sceneNum || !IsEnabled() || !IsRideableScene(play->sceneNum) ||
        gSaveContext.sceneSetupIndex > 3) {
        return;
    }
    sHandledScene = play->sceneNum;
    if (sRemount) {
        sRemount = false;
        horse = Actor_Spawn(&play->actorCtx, play, sActorId, player->actor.world.pos.x, player->actor.world.pos.y,
                            player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 1);
        if (horse != NULL && horse->update != NULL) {
            player->rideActor = horse;
            Actor_MountHorse(play, player, horse);
            func_8002DE74(play, player);
        }
    } else if (sSaved.valid && sSaved.scene == play->sceneNum) {
        Actor_Spawn(&play->actorCtx, play, sActorId, sSaved.pos.x, sSaved.pos.y, sSaved.pos.z, 0, sSaved.yaw, 0, 1);
    }
}

static void RegisterToggle(void) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = "Ride Young Epona as Child";
    widget.cvar = YOUNG_EPONA_CVAR;
    widget.tooltip = "Child Link, knowing Epona's Song and holding an ocarina, can call a young Epona and ride her.";
    widget.defaultInt = 1;
    sApi->RegisterMenuWidget(&widget);
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
    definition.key = YOUNG_EPONA_KEY;
    definition.description = "Young Epona";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnHorse);
    definition.init = YoungEpona_Init;
    definition.destroy = YoungEpona_Destroy;
    definition.update = YoungEpona_Update;
    definition.draw = YoungEpona_Draw;
    sActorId = sApi->RegisterActor(&definition);
    if (sActorId < 0) {
        return;
    }

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, WatchLink);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, ForgetFile);
    SOH_REGISTER_HOOK(sApi, OnSaveFile, SaveGame);
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterToggle();
    }
}
