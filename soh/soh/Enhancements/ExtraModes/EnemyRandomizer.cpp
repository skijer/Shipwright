#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "functions.h"
#include "macros.h"
#include "soh/ShipUtils.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/SohGui/MenuTypes.h"
#include "soh/SohGui/SohMenu.h"
#include "soh/ShipInit.hpp"
#include "z64scene.h"
#include "soh/SohGui/SohGui.hpp"
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/ModApi/ActorRegistry/ActorRegistry.h"
#include "soh/Enhancements/randomizer/EnemyPlacement.h"

extern "C" {
#include <z64.h>
#include "src/overlays/actors/ovl_Bg_Haka/z_bg_haka.h"
#include "src/overlays/actors/ovl_Bg_Haka_Huta/z_bg_haka_huta.h"
#include "src/overlays/actors/ovl_Bg_Haka_Tubo/z_bg_haka_tubo.h"
#include "src/overlays/actors/ovl_Bg_Mori_Bigst/z_bg_mori_bigst.h"
#include "src/overlays/actors/ovl_En_Blkobj/z_en_blkobj.h"
#include "src/overlays/actors/ovl_En_Encount1/z_en_encount1.h"
#include "src/overlays/actors/ovl_En_GeldB/z_en_geldb.h"
#include "src/overlays/actors/ovl_En_Peehat/z_en_peehat.h"
#include "src/overlays/actors/ovl_En_Vali/z_en_vali.h"
#include "src/overlays/actors/ovl_En_Skb/z_en_skb.h"
#include "src/overlays/actors/ovl_En_Tp/z_en_tp.h"

extern PlayState* gPlayState;
extern int gMapLoading;
}

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

#define CVAR_ENEMY_RANDOMIZER_NAME CVAR_ENHANCEMENT("RandomizedEnemies")
#define CVAR_ENEMY_RANDOMIZER_DEFAULT ENEMY_RANDOMIZER_OFF
#define CVAR_ENEMY_RANDOMIZER_VALUE CVarGetInteger(CVAR_ENEMY_RANDOMIZER_NAME, CVAR_ENEMY_RANDOMIZER_DEFAULT)

static bool IsSeedEnemyRandomizer() {
    return IS_RANDO && RAND_GET_OPTION(RSK_ENEMY_RANDOMIZER);
}

static bool IsEnemyRandomizerActive() {
    return CVAR_ENEMY_RANDOMIZER_VALUE != CVAR_ENEMY_RANDOMIZER_DEFAULT || IsSeedEnemyRandomizer();
}

static std::vector<EnemyKind> selectedEnemyList;

static std::string GetCustomEnemyCvar(const char* key) {
    return std::string(CVAR_ENHANCEMENT("RandomizedEnemyList.Mod.")) + key;
}

static void UpdateSelectedEnemies() {
    selectedEnemyList.clear();
    bool selectAll = CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemyList.All"), 0);

    for (const EnemyKind& enemy : EnemyCatalog_GetNatives()) {
        if (enemy.spawnable && (selectAll || CVarGetInteger(enemy.enhancementCvar, 1))) {
            selectedEnemyList.push_back(enemy);
        }
    }
    for (const EnemyKind& enemy : EnemyCatalog_GetPlaceableCustoms()) {
        if (selectAll || CVarGetInteger(GetCustomEnemyCvar(enemy.key).c_str(), 1)) {
            selectedEnemyList.push_back(enemy);
        }
    }
    if (selectedEnemyList.size() == 0) {
        selectedEnemyList.push_back(EnemyCatalog_GetNatives().front());
    }
}

static EnemyKind GetRandomizedEnemyEntry(u32 seed, PlayState* play, s16 posY, bool fromBari) {
    std::vector<EnemyKind> filteredEnemyList = {};
    bool isMQ = ResourceMgr_IsSceneMasterQuest(play->sceneNum);

    if (selectedEnemyList.size() == 0) {
        UpdateSelectedEnemies();
    }

    for (const EnemyKind& enemy : selectedEnemyList) {
        if (EnemyRando_IsAllowedAt(play->sceneNum, play->roomCtx.curRoom.num, isMQ, enemy, posY, fromBari)) {
            filteredEnemyList.push_back(enemy);
        }
    }

    if (filteredEnemyList.size() == 0) {
        filteredEnemyList = selectedEnemyList;
    }

    if (CVAR_ENEMY_RANDOMIZER_VALUE == ENEMY_RANDOMIZER_RANDOM_SEEDED) {
        uint64_t randomState = 0;

        ShipUtils::RandInit(
            seed + (IS_RANDO ? Rando::Context::GetInstance()->GetSeed() : gSaveContext.ship.stats.fileCreatedAt),
            &randomState);

        return ShipUtils::RandomElement(filteredEnemyList, false, &randomState);
    }

    return ShipUtils::RandomElement(filteredEnemyList, false);
}

static std::optional<EnemyKind> FindSeedReplacement(PlayState* play, s16 actorId, s16 params) {
    std::optional<EnemyKind> original = EnemyCatalog_Classify(play->sceneNum, actorId, params);
    if (!original.has_value()) {
        return std::nullopt;
    }
    return EnemyPlacement_FindSaveReplacement(play->sceneNum, original->key);
}

static u8 GetRandomizedEnemy(PlayState* play, s16* actorId, s16* posX, s16* posY, s16* posZ, s16* rotX, s16* rotY,
                             s16* rotZ, s16* params, s16 offset = 0, bool fromBari = false) {
    u32 isMQ = ResourceMgr_IsSceneMasterQuest(play->sceneNum);
    bool isSeeded = IsSeedEnemyRandomizer();

    // Hack to remove enemies that wrongfully spawn because of bypassing object dependency with enemy randomizer on.
    // This should probably be handled on OTR generation in the future when object dependency is fully removed.
    // Remove bats and Skulltulas from graveyard.
    // Remove Octorok in Lost Woods.
    // Remove signs in Gerudo Fortress as child
    if (((*actorId == ACTOR_EN_FIREFLY || (*actorId == ACTOR_EN_SW && *params == 0)) &&
         play->sceneNum == SCENE_GRAVEYARD) ||
        (*actorId == ACTOR_EN_OKUTA && play->sceneNum == SCENE_LOST_WOODS) ||
        (*actorId == ACTOR_EN_KANBAN && play->sceneNum == SCENE_GERUDOS_FORTRESS && LINK_IS_CHILD)) {
        return 0;
    }

    // Hack to change a pot in Spirit Temple that holds a Deku Shield to not hold anything.
    // This should probably be handled on OTR generation in the future when object dependency is fully removed.
    // This Deku Shield doesn't normally spawn in authentic gameplay because of object dependency.
    if (*actorId == ACTOR_OBJ_TSUBO && *params == 24597) {
        *params = 24067;
    }

    // Lengthen timer in non-MQ Jabu Jabu bubble room.
    if (!isMQ && *actorId == ACTOR_OBJ_ROOMTIMER && *params == 30760 && play->sceneNum == SCENE_JABU_JABU &&
        play->roomCtx.curRoom.num == 12) {
        *params = (*params & ~0x3FF) | 120;
    }

    // Seeded placements never replace the Lost Woods Skull Kid, so generation needn't know Link's age.
    bool isChild = isSeeded || LINK_IS_CHILD;
    if (!EnemyRando_IsRandomizableSpawn(play->sceneNum, play->roomCtx.curRoom.num, *actorId, *params, *posX, isMQ,
                                        isChild)) {
        return 1;
    }
    std::optional<EnemyKind> seedReplacement;
    if (isSeeded) {
        seedReplacement = FindSeedReplacement(play, *actorId, *params);
        if (!seedReplacement.has_value()) {
            return 1;
        }
    }

    // When replacing Iron Knuckles in Spirit Temple, move them away from the throne because
    // some enemies can get stuck on the throne.
    if (*actorId == ACTOR_EN_IK && play->sceneNum == SCENE_SPIRIT_TEMPLE) {
        if (*params == 6657) {
            *posX = *posX + 150;
        } else if (*params == 6401) {
            *posX = *posX - 150;
        }
    }

    // Move like-likes in MQ Jabu Jabu down into the room as they otherwise get stuck on Song of Time blocks.
    if (*actorId == ACTOR_EN_RR && play->sceneNum == SCENE_JABU_JABU && play->roomCtx.curRoom.num == 11) {
        if (*posX == 1003) {
            *posX = *posX - 75;
        } else {
            *posX = *posX + 75;
        }
        *posY = *posY - 200;
    }

    // One enemy in GTG hammer room doesn't raycast properly, move it slightly
    if (*posY == 117 && play->sceneNum == SCENE_GERUDO_TRAINING_GROUND && play->roomCtx.curRoom.num == 5) {
        *posY = 118;
    }

    // Do a raycast from the original position of the actor to find the ground below it, then try to place
    // the new actor on the ground. This way enemies don't spawn very high in the sky, and gives us control
    // over height offsets per enemy from a proven grounded position.
    CollisionPoly poly;
    Vec3f pos;
    f32 raycastResult;

    pos.x = *posX;
    pos.y = static_cast<f32>(*posY + 50);
    pos.z = *posZ;

    // the forest temple second twisted hallway spawns after the enemies so we need to "find the floor" manually
    if (play->sceneNum == SCENE_FOREST_TEMPLE && play->roomCtx.curRoom.num == 20 && *posZ > -3000) {
        // when hallway is twisted (play->actorCtx.flags.tempSwch & 1), one spawn has the floor at 1235.165 &
        // the other at 1239.094 but that changes based on the player position
        // when not twisted, the whole floor is at 1228

        *posY = 1228;
    } else if (isMQ && play->sceneNum == SCENE_SPIRIT_TEMPLE && play->roomCtx.curRoom.num == 27) {
        // Similar to the twisted hallway, the turntable here is an actor used as flooring, so the enemy needs
        // spawning on it

        *posY = 50;
    } else {
        raycastResult = BgCheck_AnyRaycastFloor1(&play->colCtx, &poly, &pos);

        // If ground is found below actor, move actor to that height.
        if (raycastResult > BGCHECK_Y_MIN) {
            *posY = static_cast<s16>(raycastResult);
        }
    }

    u32 seed =
        play->sceneNum + *actorId + (int)*posX + (int)*posY + (int)*posZ + *rotX + *rotY + *rotZ + *params + offset;
    EnemyKind randomEnemy =
        seedReplacement.has_value() ? *seedReplacement : GetRandomizedEnemyEntry(seed, play, *posY, fromBari);

    *actorId = randomEnemy.actorId;
    *params = randomEnemy.params;

    // Straighten out enemies so they aren't flipped on their sides when the original spawn is.
    *rotX = 0;
    *posY = *posY + randomEnemy.spawnHeight;

    switch (*actorId) {
        // When spawning big jellyfish, spawn it up high.
        case ACTOR_EN_VALI:
            *posY = *posY + 300;
            break;
        // Spawn Peahat off the ground, otherwise it kills itself by colliding with the ground.
        case ACTOR_EN_PEEHAT:
            if (*params == 1) {
                *posY = *posY + 100;
            }
            break;
        // Spawn Skulltulas off the ground.
        case ACTOR_EN_ST:
            *posY = *posY + 200;
            break;
        // Spawn flying enemies off the ground.
        case ACTOR_EN_FIREFLY:
        case ACTOR_EN_BILI:
        case ACTOR_EN_BB:
        case ACTOR_EN_CLEAR_TAG:
        case ACTOR_EN_CROW:
            *posY = *posY + 75;
            break;
        // Remove switch flag setting from Wolfos
        case ACTOR_EN_WF:
            *params |= 0xFF00;
            break;
        default:
            break;
    }
    return 1;
}

void FixClubMoblinScale(void* ptr) {
    Actor* actor = (Actor*)ptr;
    if (actor->params == -1) {
        Actor_SetScale(actor, 0.014f);
    }
}

// EnTp, EnSkb and EnPeehat update collider position only in their draw functions,
// causes 0,0,0 issues, so set their collider positions once on actor init
void EnemyRando_SetColliderPosTp(void* ptr) {
    EnTp* enTp = (EnTp*)ptr;

    enTp->collider.elements[0].dim.worldSphere.center.x = (s16)enTp->actor.world.pos.x;
    enTp->collider.elements[0].dim.worldSphere.center.y = (s16)enTp->actor.world.pos.y;
    enTp->collider.elements[0].dim.worldSphere.center.z = (s16)enTp->actor.world.pos.z;
    enTp->collider.elements[0].dim.worldSphere.radius =
        (s16)(enTp->collider.elements[0].dim.modelSphere.radius * enTp->collider.elements[0].dim.scale);
}

void EnemyRando_SetColliderPosSkb(void* ptr) {
    EnSkb* enSkb = (EnSkb*)ptr;
    u8 i;

    for (i = 0; i < 2; i++) {
        enSkb->collider.elements[i].dim.worldSphere.center.x = (s16)enSkb->actor.world.pos.x;
        enSkb->collider.elements[i].dim.worldSphere.center.y = (s16)enSkb->actor.world.pos.y;
        enSkb->collider.elements[i].dim.worldSphere.center.z = (s16)enSkb->actor.world.pos.z;
        enSkb->collider.elements[i].dim.worldSphere.radius =
            (s16)(enSkb->collider.elements[i].dim.modelSphere.radius * enSkb->collider.elements[i].dim.scale);
    }
}

void EnemyRando_SetColliderPosPeehat(void* ptr) {
    EnPeehat* enPeehat = (EnPeehat*)ptr;
    u8 i;

    for (i = 0; i < 4; i++) {
        enPeehat->colliderQuad.dim.quad[i].x = enPeehat->actor.world.pos.x + (f32)i;
        enPeehat->colliderQuad.dim.quad[i].y = enPeehat->actor.world.pos.y + (f32)i;
        enPeehat->colliderQuad.dim.quad[i].z = enPeehat->actor.world.pos.z + (f32)i;
    }
    Collider_SetQuadVertices(&enPeehat->colliderQuad, &enPeehat->colliderQuad.dim.quad[0],
                             &enPeehat->colliderQuad.dim.quad[1], &enPeehat->colliderQuad.dim.quad[2],
                             &enPeehat->colliderQuad.dim.quad[3]);
}

static void OnGerudoFighterDefeat(void* refActor) {
    EnGeldB* enGeldB = reinterpret_cast<EnGeldB*>(refActor);

    if (enGeldB->keyFlag == 0) {
        Item_DropCollectibleRandom(gPlayState, &enGeldB->actor, &enGeldB->actor.world.pos, 0xC0);
    }
}

// A stored pointer outlives its actor whenever the two are freed in the other order (room or scene unload), and its
// block may already hold a new actor, so the pointer is trusted only while it is listed with the expected id.
static bool IsActorAlive(PlayState* play, Actor* target, int16_t expectedId) {
    for (int32_t category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != nullptr; actor = actor->next) {
            if (actor == target) {
                return actor->id == expectedId && actor->update != nullptr;
            }
        }
    }
    return false;
}

struct CustomStalfosPairFightData {
    BgMoriBigst* moriBigst = nullptr;
    ActorFunc originalDestroy = nullptr;
};

static ObjectExtension::Register<CustomStalfosPairFightData> CustomStalfosPairFightDataRegister;

void CustomStalfosPairFightDestroy(Actor* thisx, PlayState* play) {
    assert(ObjectExtension::GetInstance().Has<CustomStalfosPairFightData>(thisx));

    CustomStalfosPairFightData* customStalfosPairFightData =
        ObjectExtension::GetInstance().Get<CustomStalfosPairFightData>(thisx);

    BgMoriBigst* moriBigst = customStalfosPairFightData->moriBigst;
    if (IsActorAlive(play, &moriBigst->dyna.actor, ACTOR_BG_MORI_BIGST)) {
        moriBigst->dyna.actor.home.rot.z -= 1;
    }

    customStalfosPairFightData->originalDestroy(thisx, play);

    ObjectExtension::GetInstance().Remove<CustomStalfosPairFightData>(thisx);
}

struct CustomPeehatLarvaData {
    EnPeehat* peehat = nullptr;
    ActorFunc originalDestroy = nullptr;
};

static ObjectExtension::Register<CustomPeehatLarvaData> CustomPeehatLarvaDataRegister;

void CustomPeehatLarvaDestroy(Actor* thisx, PlayState* play) {
    assert(ObjectExtension::GetInstance().Has<CustomPeehatLarvaData>(thisx));

    CustomPeehatLarvaData* customPeehatLarvaData = ObjectExtension::GetInstance().Get<CustomPeehatLarvaData>(thisx);

    EnPeehat* peehat = customPeehatLarvaData->peehat;
    if (IsActorAlive(play, &peehat->actor, ACTOR_EN_PEEHAT)) {
        peehat->unk_2FA -= 1;
    }

    customPeehatLarvaData->originalDestroy(thisx, play);

    ObjectExtension::GetInstance().Remove<CustomPeehatLarvaData>(thisx);
}

void RegisterEnemyRandomizer() {
    bool isActive = IsEnemyRandomizerActive();

    COND_ID_HOOK(OnActorInit, ACTOR_EN_MB, isActive, FixClubMoblinScale);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_TP, isActive, EnemyRando_SetColliderPosTp);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_SKB, isActive, EnemyRando_SetColliderPosSkb);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_PEEHAT, isActive, EnemyRando_SetColliderPosPeehat);

    // Allow Random Gerudo Fighters (contain no keys) to spawn without any switch flags
    COND_VB_SHOULD(VB_GERUDO_FIGHTER_CONTINUE_WAITING, isActive, {
        EnGeldB* enGeldB = va_arg(args, EnGeldB*);

        if (enGeldB->keyFlag == 0) {
            if (!enGeldB->invisible || enGeldB->actor.xzDistToPlayer <= 300.0f) {
                *should = false;
            }
        }
    });

    // Don't play Miniboss music for Random Gerudo Fighters
    COND_VB_SHOULD(VB_GERUDO_FIGHTER_PLAY_MINIBOSS_MUSIC, isActive, {
        EnGeldB* enGeldB = va_arg(args, EnGeldB*);

        if (enGeldB->keyFlag == 0) {
            *should = false;
        }
    });

    // If Random Gerudo Fighters knock Link down, void him out like Wallmasters
    COND_VB_SHOULD(VB_GERUDO_FIGHTER_THROW_LINK_TO_JAIL, isActive, {
        EnGeldB* enGeldB = va_arg(args, EnGeldB*);

        if (enGeldB->keyFlag == 0) {
            *should = false;
            Sfx_PlaySfxCentered(NA_SE_OC_ABYSS);
            Play_TriggerRespawn(gPlayState);
        }
    });

    // If Random Gerudo Fighters are defeated, drop some items
    COND_ID_HOOK(OnEnemyDefeat, ACTOR_EN_GELDB, isActive, OnGerudoFighterDefeat);

    // Spawn random enemies even if their object isn't loaded
    COND_VB_SHOULD(VB_SPAWN_ACTOR_WITHOUT_OBJECT, isActive, {
        if (gMapLoading) {
            *should = true;
        }
    });

    // The following enemies break when the parent actor isn't the same as what would happen in authentic gameplay.
    // As such, don't assign a parent to them at all when spawned with Enemy Randomizer.
    // Gohma (z_boss_goma.c) and the falling platform spawning Stalfos in
    // Forest Temple (z_bg_mori_bigst.c) that normally rely on this behaviour are changed when
    // Enemy Rando is on so they still work properly even without assigning a parent.
    COND_VB_SHOULD(VB_SET_CHILD_ACTOR_PARENT, isActive, {
        Actor* spawnedActor = va_arg(args, Actor*);

        if (spawnedActor->id == ACTOR_EN_FLOORMAS || spawnedActor->id == ACTOR_EN_PEEHAT) {
            *should = false;
        }
    });

    COND_VB_SHOULD(VB_SPAWN_ACTOR_ENTRY, isActive, {
        ActorContext* actorCtx = va_arg(args, ActorContext*);
        ActorEntry* actorEntry = va_arg(args, ActorEntry*);
        PlayState* play = va_arg(args, PlayState*);
        Actor** actor = va_arg(args, Actor**);

        s16 actorId = actorEntry->id;
        s16 posX = static_cast<s16>(actorEntry->pos.x);
        s16 posY = static_cast<s16>(actorEntry->pos.y);
        s16 posZ = static_cast<s16>(actorEntry->pos.z);
        s16 rotX = actorEntry->rot.x;
        s16 rotY = actorEntry->rot.y;
        s16 rotZ = actorEntry->rot.z;
        s16 params = actorEntry->params;

        *should = false;
        if (GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            *actor = Actor_Spawn(actorCtx, play, actorId, posX, posY, posZ, rotX, rotY, rotZ, params);
        }
    });

    COND_VB_SHOULD(VB_ADULT_ZELDA_SPAWN_STALFOS_IN_COLLAPSE, isActive, {
        PlayState* play = va_arg(args, PlayState*);
        Vec3f* playerPos = va_arg(args, Vec3f*);
        double posX = va_arg(args, double);
        double posY = va_arg(args, double);
        double posZ = va_arg(args, double);

        s16 actorId = ACTOR_EN_TEST;
        s16 posX2 = static_cast<s16>(posX);
        s16 posY2 = static_cast<s16>(posY);
        s16 posZ2 = static_cast<s16>(posZ);
        s16 rotX = 0;
        s16 rotY = static_cast<s16>(
            Math_FAtan2F(playerPos->x - static_cast<f32>(posX), playerPos->z - static_cast<f32>(posZ)) *
            static_cast<f32>(0x8000 / M_PI));
        s16 rotZ = 0;
        s16 params = 5;

        if (!GetRandomizedEnemy(play, &actorId, &posX2, &posY2, &posZ2, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, posX2, posY2, posZ2, rotX, rotY, rotZ, params);

        *should = false;
    });

    COND_VB_SHOULD(VB_BLKOBJ_SPAWN_DARK_LINK, isActive, {
        if (!*should) {
            return;
        }

        EnBlkobj* blkobj = va_arg(args, EnBlkobj*);
        PlayState* play = va_arg(args, PlayState*);

        s16 actorId = ACTOR_EN_TORCH2;
        s16 posX = static_cast<s16>(blkobj->dyna.actor.world.pos.x);
        s16 posY = static_cast<s16>(blkobj->dyna.actor.world.pos.y);
        s16 posZ = static_cast<s16>(blkobj->dyna.actor.world.pos.z);
        s16 rotX = 0;
        s16 rotY = blkobj->dyna.actor.yawTowardsPlayer;
        s16 seededRotY = 6969;
        s16 rotZ = 0;
        s16 params = 0;

        // rotation is hardcoded here as it is inconsistent, breaking seeded randomiser
        // value provided is the best meme number avalible, theoretically any number works
        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &seededRotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        EnBlkobj_SetupAction(blkobj, EnBlkobj_DarkLinkFight);

        *should = false;
    });

    COND_VB_SHOULD(VB_HAKA_TUBO_SPAWN_KEESE, isActive, {
        BgHakaTubo* hakaTubo = va_arg(args, BgHakaTubo*);
        PlayState* play = va_arg(args, PlayState*);

        s16 actorId = ACTOR_EN_FIREFLY;
        s16 posX = static_cast<s16>(hakaTubo->dyna.actor.world.pos.x);
        s16 posY = static_cast<s16>(hakaTubo->dyna.actor.world.pos.y) + 80;
        s16 posZ = static_cast<s16>(hakaTubo->dyna.actor.world.pos.z);
        s16 rotX = 0;
        s16 rotY = hakaTubo->dyna.actor.shape.rot.y;
        s16 rotZ = 0;
        s16 params = 2;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, posX, posY, posZ, rotX, rotY, rotZ, params);

        *should = false;
    });

    COND_VB_SHOULD(VB_HAKA_SPAWN_POE, isActive, {
        if (!*should) {
            return;
        }

        BgHaka* haka = va_arg(args, BgHaka*);
        PlayState* play = va_arg(args, PlayState*);

        s16 actorId = ACTOR_EN_POH;
        s16 posX = static_cast<s16>(haka->dyna.actor.world.pos.x);
        s16 posY = static_cast<s16>(haka->dyna.actor.world.pos.y);
        s16 posZ = static_cast<s16>(haka->dyna.actor.world.pos.z);
        s16 rotX = 0;
        s16 rotY = haka->dyna.actor.shape.rot.y;
        s16 rotZ = 0;
        s16 params = 1;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        *should = false;
    });

    COND_VB_SHOULD(VB_BIRI_SPAWN_JELLYFISH_UPON_DEATH, isActive, {
        EnVali* vali = va_arg(args, EnVali*);
        PlayState* play = va_arg(args, PlayState*);

        s16 posX = static_cast<s16>(vali->actor.world.pos.x);
        s16 posY = static_cast<s16>(vali->actor.world.pos.y);
        s16 posZ = static_cast<s16>(vali->actor.world.pos.z);
        s16 rotX = 0;
        s16 rotY = vali->actor.world.rot.y;
        s16 rotZ = 0;

        for (s32 i = 0; i < 3; i++) {
            s16 actorId = ACTOR_EN_BILI;
            s16 params = 0;
            s16 homePosX = static_cast<s16>(vali->actor.home.pos.x);
            s16 homePosY = static_cast<s16>(vali->actor.home.pos.y);
            s16 homePosZ = static_cast<s16>(vali->actor.home.pos.z);
            s16 homeRotX = vali->actor.home.rot.x;
            s16 homeRotY = vali->actor.home.rot.y;
            s16 homeRotZ = vali->actor.home.rot.z;

            // use the home pos & rot to make it consistent
            if (!GetRandomizedEnemy(play, &actorId, &homePosX, &homePosY, &homePosZ, &homeRotX, &homeRotY, &homeRotZ,
                                    &params, i * 1000, true)) {
                assert(false);
            }

            Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                        static_cast<f32>(posZ), rotX, rotY, rotZ, params);

            rotY += 0x10000 / 3;
        }

        *should = false;
    });

    COND_VB_SHOULD(VB_ENCOUNT1_SPAWN_STALCHILD_OR_WOLFOS, isActive, {
        EnEncount1* encount1 = va_arg(args, EnEncount1*);
        PlayState* play = va_arg(args, PlayState*);

        // have to use int instead of s16 in the va_arg call due to integer promotion
        s16 actorId = va_arg(args, int);
        Vec3f spawnPos = va_arg(args, Vec3f);
        s16 posX = static_cast<s16>(spawnPos.x);
        s16 posY = static_cast<s16>(spawnPos.y);
        s16 posZ = static_cast<s16>(spawnPos.z);
        s16 rotX = 0;
        s16 rotY = 0;
        s16 rotZ = 0;
        // have to use int instead of s16 in the va_arg call due to integer promotion
        s16 params = va_arg(args, int);

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        if (Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                        static_cast<f32>(posZ), rotX, rotY, rotZ, params)) {
            encount1->curNumSpawn++;
            if (encount1->curNumSpawn >= encount1->maxCurSpawns) {
                encount1->fieldSpawnTimer = 100;
            }
            if (play->sceneNum != SCENE_HYRULE_FIELD) {
                encount1->totalNumSpawn++;
            }
        }

        *should = false;
    });

    COND_VB_SHOULD(VB_MORI_BIGST_SUMMON_STALFOS_PAIR, isActive, {
        BgMoriBigst* moriBigst = va_arg(args, BgMoriBigst*);
        PlayState* play = va_arg(args, PlayState*);

        s16 actorId = ACTOR_EN_TEST;
        s16 posX = 70;
        s16 posY = 827;
        s16 posZ = -3383;
        s16 rotX = 0;
        s16 rotY = 0;
        s16 rotZ = 0;
        s16 params = 5;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor* enemy1 = Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        actorId = ACTOR_EN_TEST;
        posX = 170;
        posY = 827;
        posZ = -3260;
        rotX = 0;
        rotY = 0;
        rotZ = 0;
        params = 5;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor* enemy2 = Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        moriBigst->dyna.actor.home.rot.z = 2;

        ObjectExtension::GetInstance().Set<CustomStalfosPairFightData>(
            enemy1, CustomStalfosPairFightData{ .moriBigst = moriBigst, .originalDestroy = enemy1->destroy });
        ObjectExtension::GetInstance().Set<CustomStalfosPairFightData>(
            enemy2, CustomStalfosPairFightData{ .moriBigst = moriBigst, .originalDestroy = enemy2->destroy });

        enemy1->destroy = CustomStalfosPairFightDestroy;
        enemy2->destroy = CustomStalfosPairFightDestroy;

        *should = false;
    });

    COND_VB_SHOULD(VB_AFTER_ACTOR_UPDATE_BGCHECKINFO, isActive, {
        Actor* enemy = va_arg(args, Actor*);
        Player* player = GET_PLAYER(gPlayState);

        if (enemy->category != ACTORCAT_ENEMY) {
            return;
        }

        bool isMQ = ResourceMgr_IsSceneMasterQuest(gPlayState->sceneNum);
        s16 roomNum = gPlayState->roomCtx.curRoom.num;

        if ((EnemyRando_IsClearRoom(isMQ, gPlayState->sceneNum, roomNum) ||
             EnemyRando_IsTimedRoom(isMQ, gPlayState->sceneNum, roomNum)) &&
            enemy->id != ACTOR_EN_FIREFLY && enemy->id != ACTOR_EN_CROW) {
            u32 floorProperty = func_80041EA4(&gPlayState->colCtx, enemy->floorPoly, enemy->floorBgId);

            // Let ground enemies air walk above voidout floor in clear/timed rooms
            // and move them up if setting is enabled after they're fallen down
            if (CVarGetInteger(CVAR_ENHANCEMENT("EnemyRandomizerAirWalkVoidouts"), false) &&
                (SurfaceType_GetFloorType(&gPlayState->colCtx, enemy->floorPoly, enemy->floorBgId) == 9 ||
                 floorProperty == FLOOR_PROPERTY_5 || floorProperty == FLOOR_PROPERTY_12)) {
                if (enemy->world.pos.y < player->actor.floorHeight) {
                    Math_StepToF(&enemy->world.pos.y, player->actor.floorHeight, 12.0f);
                    enemy->velocity.y = 0.0f;
                }
            }
            // Backup. If player really above enemy, probably fallen out of bounds
            // Extra distance for Flare Dancer elevator platform room (if spawning top in doorsanity)
            f32 killHeight = (gPlayState->sceneNum == SCENE_FIRE_TEMPLE && gPlayState->roomCtx.curRoom.num == 24)
                                 ? (player->actor.world.pos.y - 1500.0f)
                                 : (player->actor.world.pos.y - 1000.0f);
            if (enemy->world.pos.y < killHeight) {
                SPDLOG_INFO(
                    "AfterActorUpdateBgCheckInfo: Killing enemy, out of bounds (id {:#x}, pos x {:.1f} y {:.1f} z "
                    "{:.1f})",
                    enemy->id, enemy->world.pos.x, enemy->world.pos.y, enemy->world.pos.z);
                Actor_Kill(enemy);
            }
        }
    });

    COND_VB_SHOULD(VB_HAKA_HUTA_SPAWN_KEESE, isActive, {
        BgHakaHuta* hakaHuta = va_arg(args, BgHakaHuta*);
        PlayState* play = va_arg(args, PlayState*);

        s16 actorId = ACTOR_EN_FIREFLY;
        s16 posX =
            static_cast<s16>(hakaHuta->dyna.actor.world.pos.x + (-25.0f) * Math_CosS(hakaHuta->dyna.actor.shape.rot.y) +
                             40.0f * Math_SinS(hakaHuta->dyna.actor.shape.rot.y));
        s16 posY = static_cast<s16>(hakaHuta->dyna.actor.world.pos.y) - 10;
        s16 posZ =
            static_cast<s16>(hakaHuta->dyna.actor.world.pos.z - (-25.0f) * Math_SinS(hakaHuta->dyna.actor.shape.rot.y) +
                             40.0f * Math_CosS(hakaHuta->dyna.actor.shape.rot.y));
        s16 rotX = 0;
        s16 rotY = hakaHuta->dyna.actor.shape.rot.y + 0x8000;
        s16 rotZ = 0;
        s16 params = 2;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        actorId = ACTOR_EN_FIREFLY;
        posX =
            static_cast<s16>(hakaHuta->dyna.actor.world.pos.x + (-25.0f) * Math_CosS(hakaHuta->dyna.actor.shape.rot.y) +
                             80.0f * Math_SinS(hakaHuta->dyna.actor.shape.rot.y));
        posY = static_cast<s16>(hakaHuta->dyna.actor.world.pos.y) - 10;
        posZ =
            static_cast<s16>(hakaHuta->dyna.actor.world.pos.z - (-25.0f) * Math_SinS(hakaHuta->dyna.actor.shape.rot.y) +
                             80.0f * Math_CosS(hakaHuta->dyna.actor.shape.rot.y));
        rotX = 0;
        rotY = hakaHuta->dyna.actor.shape.rot.y;
        rotZ = 0;
        params = 2;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        *should = false;
    });

    COND_VB_SHOULD(VB_HAKA_HUTA_SPAWN_REDEAD, isActive, {
        BgHakaHuta* hakaHuta = va_arg(args, BgHakaHuta*);
        PlayState* play = va_arg(args, PlayState*);

        s16 actorId = ACTOR_EN_RD;
        s16 posX =
            static_cast<s16>(hakaHuta->dyna.actor.home.pos.x + (-25.0f) * Math_CosS(hakaHuta->dyna.actor.shape.rot.y) +
                             100.0f * Math_SinS(hakaHuta->dyna.actor.shape.rot.y));
        s16 posY = static_cast<s16>(hakaHuta->dyna.actor.home.pos.y) - 40;
        s16 posZ =
            static_cast<s16>(hakaHuta->dyna.actor.home.pos.z - (-25.0f) * Math_SinS(hakaHuta->dyna.actor.shape.rot.y) +
                             100.0f * Math_CosS(hakaHuta->dyna.actor.shape.rot.y));
        s16 rotX = 0;
        s16 rotY = hakaHuta->dyna.actor.shape.rot.y;
        s16 rotZ = 0;
        s16 params = 0xFD;

        if (!GetRandomizedEnemy(play, &actorId, &posX, &posY, &posZ, &rotX, &rotY, &rotZ, &params)) {
            assert(false);
        }

        Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(posX), static_cast<f32>(posY),
                    static_cast<f32>(posZ), rotX, rotY, rotZ, params);

        *should = false;
    });

    COND_VB_SHOULD(VB_PEEHAT_SPAWN_LARVAS, isActive, {
        EnPeehat* peehat = va_arg(args, EnPeehat*);
        PlayState* play = va_arg(args, PlayState*);

        // 3 is MAX_LARVA
        for (s32 i = 3 - peehat->unk_2FA; i > 0; i--) {
            s16 actorId = ACTOR_EN_PEEHAT;
            s16 homePosX = static_cast<s16>(peehat->actor.home.pos.x);
            s16 homePosY = static_cast<s16>(peehat->actor.home.pos.y) + 50;
            s16 homePosZ = static_cast<s16>(peehat->actor.home.pos.z);
            s16 rotX = 0;
            s16 rotY = 0;
            s16 rotZ = 0;
            s16 params = PEAHAT_TYPE_LARVA;

            if (!GetRandomizedEnemy(play, &actorId, &homePosX, &homePosY, &homePosZ, &rotX, &rotY, &rotZ, &params,
                                    i * 1000)) {
                assert(false);
            }

            Actor* enemy =
                Actor_Spawn(&play->actorCtx, play, actorId, static_cast<f32>(homePosX), static_cast<f32>(homePosY),
                            static_cast<f32>(homePosZ), rotX, rotY, rotZ, params);

            if (enemy == NULL) {
                assert(false);
            } else {
                peehat->unk_2FA++;
                ObjectExtension::GetInstance().Set<CustomPeehatLarvaData>(
                    enemy, CustomPeehatLarvaData{ .peehat = peehat, .originalDestroy = enemy->destroy });
                enemy->destroy = CustomPeehatLarvaDestroy;
            }
        }
    });
}

static const std::map<s32, const char*> enemyRandomizerModes = {
    { ENEMY_RANDOMIZER_OFF, "Disabled" },
    { ENEMY_RANDOMIZER_RANDOM, "Random" },
    { ENEMY_RANDOMIZER_RANDOM_SEEDED, "Random (Seeded)" },
};

static void DrawCustomEnemyCheckboxes(WidgetInfo& info) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0)) {
        return;
    }
    UIWidgets::CheckboxOptions options = UIWidgets::CheckboxOptions().DefaultValue(true).Color(THEME_COLOR);

    options.disabled = CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemyList.All"), 0);
    options.disabledTooltip = "These options are disabled because \"Select All Enemies\" is enabled.";
    for (const EnemyKind& enemy : EnemyCatalog_GetPlaceableCustoms()) {
        if (UIWidgets::CVarCheckbox(enemy.name, GetCustomEnemyCvar(enemy.key).c_str(), options)) {
            UpdateSelectedEnemies();
        }
    }
}

void RegisterEnemyRandomizerWidgets() {
    WidgetPath path = { "Enhancements", "Extra Modes", SECTION_COLUMN_2 };

    SohGui::mSohMenu->AddWidget(path, "Enemy Randomizer", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("RandomizedEnemies"))
        .Callback([](WidgetInfo& info) { UpdateSelectedEnemies(); })
        .Options(
            UIWidgets::ComboboxOptions()
                .DefaultIndex(ENEMY_RANDOMIZER_OFF)
                .ComboMap(enemyRandomizerModes)
                .Tooltip("Replaces fixed enemies throughout the game with a random enemy. Bosses, Mini-Bosses and a "
                         "few specific regular enemies are excluded.\n"
                         "Enemies that need more than Deku Nuts & either Deku Sticks or a sword to kill are excluded "
                         "from spawning in \"clear enemy\" rooms.\n\n"
                         "- Random: Enemies are randomized every time you load a room.\n"
                         "- Random (Seeded): Enemies are randomized based on the current randomizer seed/file.\n"));

    SohGui::mSohMenu->AddWidget(path, "Randomized Enemy Sizes", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("RandomizedEnemySizes"))
        .Options(UIWidgets::CheckboxOptions().Tooltip("Enemies and Bosses spawn with random sizes."));

    SohGui::mSohMenu->AddWidget(path, "Scale Health with Size", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("EnemySizeScalesHealth"))
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled = !CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemySizes"), 0);
        })
        .Options(UIWidgets::CheckboxOptions().Tooltip("Scales normal enemies Health with their randomized size.\n"
                                                      "*This will NOT affect Bosses!*"));

    SohGui::mSohMenu->AddWidget(path, "Enemies Walk over Voidout Pits", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("EnemyRandomizerAirWalkVoidouts"))
        .Options(
            UIWidgets::CheckboxOptions().Tooltip("Ground enemies will air walk over pits/floors that void out Link\n"
                                                 "to prevent softlocks. Can be toggled to make enemies fall down\n"
                                                 "or fly back up from a void pit."));

    SohGui::mSohMenu->AddWidget(path, "Enemy List", WIDGET_SEPARATOR_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0);
    });

    SohGui::mSohMenu->AddWidget(path, "Select all Enemies", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("RandomizedEnemyList.All"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0); })
        .Callback([](WidgetInfo& info) { UpdateSelectedEnemies(); });

    SohGui::mSohMenu->AddWidget(path, "Enemy List", WIDGET_SEPARATOR).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0);
    });

    for (const EnemyKind& enemy : EnemyCatalog_GetNatives()) {
        if (!enemy.spawnable) {
            continue;
        }
        SohGui::mSohMenu->AddWidget(path, enemy.name, WIDGET_CVAR_CHECKBOX)
            .CVar(enemy.enhancementCvar)
            .Options(UIWidgets::CheckboxOptions().DefaultValue(true))
            .PreFunc([](WidgetInfo& info) {
                info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemies"), 0);
                info.options->disabled = CVarGetInteger(CVAR_ENHANCEMENT("RandomizedEnemyList.All"), 0);
                info.options->disabledTooltip = "These options are disabled because \"Select All Enemies\" is enabled.";
            })
            .Callback([](WidgetInfo& info) { UpdateSelectedEnemies(); });
    }

    SohGui::mSohMenu->AddWidget(path, "Mod Enemies", WIDGET_CUSTOM).CustomFunction(DrawCustomEnemyCheckboxes);
}

// Remove object dependency for Enemy Randomizer and Crowd Control to allow Like-likes to
// drop equipment correctly in rooms where Like-likes normally don't spawn.
static void RegisterItem00WithoutObject() {
    bool required = IsEnemyRandomizerActive() || CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 0);
    COND_VB_SHOULD(VB_ITEM00_REQUIRE_OBJECT, required, {
        EnItem00* item = va_arg(args, EnItem00*);

        *should = false;
        item->actor.objBankIndex = 0;
    });
}

static RegisterShipInitFunc initFunc(RegisterEnemyRandomizer, { CVAR_ENEMY_RANDOMIZER_NAME, "IS_RANDO" });
static RegisterShipInitFunc initFuncItem00(RegisterItem00WithoutObject, { CVAR_ENEMY_RANDOMIZER_NAME, "IS_RANDO",
                                                                          CVAR_REMOTE_CROWD_CONTROL("Enabled") });
static RegisterMenuInitFunc menuInitFunc(RegisterEnemyRandomizerWidgets);
