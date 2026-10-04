#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "objects/object_box/object_box.h"
#include "soh_assets.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/item_category_adj.h"

SaveContext gSaveContext;
static int matchSize, matchTexture, requireAgony;
static bool skeletonKey, missingCustomModels;
static int lightSpawns, fanfares;
static Vec3f smokePosition, smokeVelocity;

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (!strcmp(name, CVAR_ENHANCEMENT("ChestSizeMatchesContents")))
        return matchSize;
    if (!strcmp(name, CVAR_ENHANCEMENT("ChestSizeAndTextureMatchContents")))
        return matchTexture;
    if (!strcmp(name, CVAR_ENHANCEMENT("ChestSizeDependsStoneOfAgony")))
        return requireAgony;
    return fallback;
}
s32 Flags_GetRandomizerInf(RandomizerInf flag) {
    return flag == RAND_INF_HAS_SKELETON_KEY && skeletonKey;
}
Gfx* ResourceMgr_LoadGfxByName(const char* name) {
    if (missingCustomModels && strstr(name, "gChest"))
        return NULL;
    return (Gfx*)name;
}
bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...) {
    return false;
}
void EnBox_Open(EnBox* chest, PlayState* play) {
}
void EnBox_ClipToGround(EnBox* chest, PlayState* play) {
}
void Actor_MoveXZGravity(Actor* actor) {
}
void Actor_UpdateBgCheckInfo(PlayState* play, Actor* actor, f32 wallHeight, f32 wallRadius, f32 ceilingHeight,
                             s32 flags) {
}
void Actor_PlaySfx_Flagged(Actor* actor, u16 sound) {
}
f32 Rand_ZeroOne(void) {
    return 0.25f;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (3.14159265358979323846f / 32768.0f));
}
f32 Math_CosS(s16 angle) {
    return cosf(angle * (3.14159265358979323846f / 32768.0f));
}
void EffectSsIceSmoke_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale) {
    smokePosition = *pos;
    smokeVelocity = *velocity;
}
s16 Animation_GetLastFrame(void* animation) {
    return 100;
}
void Animation_Change(SkelAnime* skel, AnimationHeader* anim, f32 speed, f32 start, f32 end, u8 mode, f32 morph) {
}
Actor* Actor_SpawnAsChild(ActorContext* ctx, Actor* parent, PlayState* play, s16 id, f32 x, f32 y, f32 z, s16 rx,
                          s16 ry, s16 rz, s16 params) {
    REQUIRE(id == ACTOR_DEMO_TRE_LGT);
    ++lightSpawns;
    return NULL;
}
void Audio_PlayFanfare(u16 id) {
    ++fanfares;
}
void Flags_SetTreasure(PlayState* play, s32 flag) {
    play->actorCtx.flags.chest |= 1u << flag;
}
s32 Flags_GetTreasure(PlayState* play, s32 flag) {
    return play->actorCtx.flags.chest & (1u << flag);
}
void Actor_WorldToActorCoords(Actor* actor, Vec3f* result, Vec3f* pos) {
    *result = (Vec3f){ 0 };
}
s32 Player_IsFacingActor(Actor* actor, s16 angle, PlayState* play) {
    return false;
}
s32 GiveItemEntryFromActorWithFixedRange(Actor* actor, PlayState* play, GetItemEntry entry) {
    return 0;
}
void Actor_OfferGetItemNearby(Actor* actor, PlayState* play, s32 item) {
}
void osSyncPrintfUnused(const char* fmt, ...) {
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* format, ...) {
}

#include "chest_size_production.inc"

static EnBox Chest(EnBoxType type, GetItemCategory category) {
    EnBox chest = { 0 };
    chest.type = type;
    chest.dyna.actor.params = ENBOX_PARAMS(type, GI_BOW, 3);
    chest.dyna.actor.home.pos = chest.dyna.actor.world.pos = (Vec3f){ 10, 20, 30 };
    chest.getItemEntry.getItemCategory = category;
    chest.getItemEntry.getItemId = GI_BOW;
    chest.actionFunc = EnBox_Open;
    chest.movementFlags = ENBOX_MOVE_IMMOBILE;
    return chest;
}

static void ExpectSize(EnBox* chest, PlayState* play, f32 scale, f32 focus) {
    s16 params = chest->dyna.actor.params;
    u8 type = chest->type;
    EnBox_Update(&chest->dyna.actor, play);
    REQUIRE(fabsf(chest->dyna.actor.scale.x - scale) < 0.000001f);
    REQUIRE(chest->dyna.actor.scale.x == chest->dyna.actor.scale.y);
    REQUIRE(chest->dyna.actor.scale.x == chest->dyna.actor.scale.z);
    REQUIRE(chest->dyna.actor.focus.pos.y == chest->dyna.actor.world.pos.y + focus);
    REQUIRE(chest->dyna.actor.params == params && chest->type == type);
}

int main(void) {
    setbuf(stdout, NULL);
    PlayState* play = calloc(1, sizeof(*play));
    REQUIRE(play);
    memset(gSaveContext.inventory.items, ITEM_NONE, sizeof(gSaveContext.inventory.items));
    play->sceneNum = SCENE_HYRULE_FIELD;

    // Disabling CSMC preserves each authored trigger/type and size.
    const struct {
        EnBoxType type;
        f32 scale;
        f32 focus;
    } defaults[] = {
        { ENBOX_TYPE_BIG_DEFAULT, 0.01f, 40 },
        { ENBOX_TYPE_ROOM_CLEAR_BIG, 0.01f, 40 },
        { ENBOX_TYPE_DECORATED_BIG, 0.01f, 40 },
        { ENBOX_TYPE_SWITCH_FLAG_FALL_BIG, 0.01f, 40 },
        { ENBOX_TYPE_4, 0.01f, 40 },
        { ENBOX_TYPE_SMALL, 0.005f, 20 },
        { ENBOX_TYPE_6, 0.005f, 20 },
        { ENBOX_TYPE_ROOM_CLEAR_SMALL, 0.005f, 20 },
        { ENBOX_TYPE_SWITCH_FLAG_FALL_SMALL, 0.005f, 20 },
        { ENBOX_TYPE_9, 0.01f, 40 },
        { ENBOX_TYPE_10, 0.01f, 40 },
        { ENBOX_TYPE_SWITCH_FLAG_BIG, 0.01f, 40 },
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i) {
        EnBox chest = Chest(defaults[i].type, ITEM_CATEGORY_MAJOR);
        ExpectSize(&chest, play, defaults[i].scale, defaults[i].focus);
    }
    puts("PASS: all 12 native chest types retain size when disabled");

    // Legacy categories, including current health items and the NEI Time Gate.
    const struct {
        GetItemCategory category;
        f32 scale;
        f32 focus;
    } categories[] = {
        { ITEM_CATEGORY_MAJOR, 0.01f, 40 },
        { ITEM_CATEGORY_LESSER, 0.01f, 40 },
        { ITEM_CATEGORY_HEALTH, 0.01f, 40 },
        { ITEM_CATEGORY_BOSS_KEY, 0.01f, 40 },
        { ITEM_CATEGORY_JUNK, 0.005f, 20 },
        { ITEM_CATEGORY_SMALL_KEY, 0.005f, 20 },
        { ITEM_CATEGORY_SKULLTULA_TOKEN, 0.005f, 20 },
    };
    matchSize = 1;
    for (int randomizer = 0; randomizer < 2; ++randomizer) {
        gSaveContext.ship.quest.id = randomizer ? QUEST_RANDOMIZER : QUEST_NORMAL;
        for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i) {
            for (size_t j = 0; j < sizeof(categories) / sizeof(categories[0]); ++j) {
                EnBox chest = Chest(defaults[i].type, categories[j].category);
                ExpectSize(&chest, play, categories[j].scale, categories[j].focus);
            }
        }
    }
    EnBox chest = Chest(ENBOX_TYPE_SMALL, ITEM_CATEGORY_MAJOR);
    chest.getItemEntry.modIndex = MOD_RANDOMIZER;
    chest.getItemEntry.getItemId = RG_TIME_GATE;
    ExpectSize(&chest, play, 0.01f, 40);
    REQUIRE(chest.getItemEntry.getItemId == RG_TIME_GATE);
    REQUIRE(!strcmp((char*)chest.boxBodyDL, gTreasureChestChestFrontDL));
    matchSize = 0;
    ExpectSize(&chest, play, 0.005f, 20);
    puts("PASS: normal/randomizer contents, Time Gate, focus and live toggle");

    matchTexture = 1;
    ExpectSize(&chest, play, 0.005f, 20);
    REQUIRE(!strcmp((char*)chest.boxBodyDL, gChestBodyMajorDL));
    requireAgony = matchSize = 1;
    ExpectSize(&chest, play, 0.01f, 40);
    REQUIRE(!strcmp((char*)chest.boxBodyDL, gTreasureChestChestFrontDL));
    requireAgony = 0;
    missingCustomModels = true;
    ExpectSize(&chest, play, 0.01f, 40);
    REQUIRE(!strcmp((char*)chest.boxBodyDL, gTreasureChestChestFrontDL));
    missingCustomModels = false;
    matchTexture = 0;
    puts("PASS: independent texture/Agony options and custom-model fallback");

    chest = Chest(ENBOX_TYPE_SMALL, ITEM_CATEGORY_MAJOR);
    ExpectSize(&chest, play, 0.01f, 40);
    chest.unk_1F4 = 1; // Native player code requested the long opening animation.
    EnBox_WaitOpen(&chest, play);
    REQUIRE(lightSpawns == 1 && fanfares == 1);
    REQUIRE(play->actorCtx.flags.chest == (1u << 3));
    EnBox_SpawnIceSmoke(&chest, play);
    REQUIRE(fabsf(smokePosition.x - 0.0f) < 0.0001f);
    REQUIRE(fabsf(smokeVelocity.z - -0.8f) < 0.0001f);
    chest = Chest(ENBOX_TYPE_BIG_DEFAULT, ITEM_CATEGORY_JUNK);
    ExpectSize(&chest, play, 0.005f, 20);
    chest.unk_1F4 = 1;
    EnBox_WaitOpen(&chest, play);
    REQUIRE(lightSpawns == 1 && fanfares == 1);
    EnBox_SpawnIceSmoke(&chest, play);
    REQUIRE(fabsf(smokePosition.x - 5.0f) < 0.0001f);
    REQUIRE(fabsf(smokeVelocity.z - -0.4f) < 0.0001f);
    puts("PASS: light/fanfare and ice effects follow effective size; collection flag preserved");

    play->sceneNum = SCENE_TREASURE_BOX_SHOP;
    chest = Chest(ENBOX_TYPE_SMALL, ITEM_CATEGORY_MAJOR);
    for (int room = 0; room < 6; ++room) {
        chest.dyna.actor.room = room;
        ExpectSize(&chest, play, 0.005f, 20);
    }
    chest.dyna.actor.room = 6;
    ExpectSize(&chest, play, 0.01f, 40);
    puts("PASS: Treasure Chest Game guessing rooms stay concealed; final reward resizes");

    // Native placement corrections must toggle reversibly, never accumulate,
    // and must not pull a placement moved off the affected native coordinates back.
    const struct {
        s16 scene, room, params;
        Vec3f original, adjusted;
        GetItemCategory category;
    } positions[] = {
        { SCENE_INSIDE_GANONS_CASTLE, 9, (s16)0x8000, { 0, 20, -952 }, { 0, 20, -962 }, ITEM_CATEGORY_MAJOR },
        { SCENE_DEKU_TREE, 5, 0x5AA0, { -1376, 20, 0 }, { -1380, 20, 0 }, ITEM_CATEGORY_MAJOR },
        { SCENE_INSIDE_GANONS_CASTLE, 12, 0x36C5, { 1757, 20, -3595 }, { 1777, 20, -3626 }, ITEM_CATEGORY_JUNK },
        { SCENE_SPIRIT_TEMPLE, 14, 0x3804, { 358, 20, 0 }, { 400, 20, 0 }, ITEM_CATEGORY_JUNK },
    };
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
        chest = Chest(positions[i].params >> 12 & 0xF, positions[i].category);
        chest.dyna.actor.params = positions[i].params;
        chest.dyna.actor.room = positions[i].room;
        play->sceneNum = positions[i].scene;
        chest.dyna.actor.home.pos = chest.dyna.actor.world.pos = positions[i].original;
        matchSize = 1;
        for (int frame = 0; frame < 10; ++frame)
            EnBox_Update(&chest.dyna.actor, play);
        REQUIRE(!memcmp(&chest.dyna.actor.world.pos, &positions[i].adjusted, sizeof(Vec3f)));
        matchSize = 0;
        EnBox_Update(&chest.dyna.actor, play);
        REQUIRE(!memcmp(&chest.dyna.actor.world.pos, &positions[i].original, sizeof(Vec3f)));
        matchSize = 1;
        chest.dyna.actor.home.pos.x += 100;
        chest.dyna.actor.home.pos.z += 100;
        chest.dyna.actor.world.pos = chest.dyna.actor.home.pos;
        EnBox_Update(&chest.dyna.actor, play);
        REQUIRE(!memcmp(&chest.dyna.actor.world.pos, &chest.dyna.actor.home.pos, sizeof(Vec3f)));
    }
    puts("PASS: four reachability corrections, reversal, no drift and affected-axis placement guards");
    free(play);
}
