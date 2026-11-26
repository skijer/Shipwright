#include "item_category_adj.h"
#include "z64item.h"
#include "variables.h"
#include "macros.h"
#include "functions.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include "soh/OTRGlobals.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ObjectExtension/ObjectExtension.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

CheckIdentity ShuffleCows_IdentifyCow(const Actor* cow);

GetItemCategory Randomizer_AdjustItemCategory(GetItemEntry item) {
    GetItemCategory category = item.getItemCategory;

    // Downgrade bombchus to lesser if the player already has bombchus
    if (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU &&
        ((item.modIndex == MOD_RANDOMIZER && item.getItemId == RG_PROGRESSIVE_BOMBCHU_BAG) ||
         (item.modIndex == MOD_NONE &&
          (item.getItemId == GI_BOMBCHUS_5 || item.getItemId == GI_BOMBCHUS_10 || item.getItemId == GI_BOMBCHUS_20)))) {
        category = ITEM_CATEGORY_LESSER;
    }

    // Downgrade bottles to lesser if the player already has a bottle
    if ((item.modIndex == MOD_RANDOMIZER && item.getItemId >= RG_BOTTLE_WITH_RED_POTION &&
         item.getItemId <= RG_BOTTLE_WITH_POE) ||
        (item.modIndex == MOD_NONE && (item.getItemId == GI_BOTTLE || item.getItemId == GI_MILK_BOTTLE))) {
        if (gSaveContext.inventory.items[SLOT_BOTTLE_1] != ITEM_NONE) {
            category = ITEM_CATEGORY_LESSER;
        }
    }

    // Downgrade keys to junk if the player already has skeleton key
    if (category == ITEM_CATEGORY_SMALL_KEY && Flags_GetRandomizerInf(RAND_INF_HAS_SKELETON_KEY)) {
        category = ITEM_CATEGORY_JUNK;
    }

    return category;
}

static ContainerStyle GetCategoryStyle(GetItemCategory category) {
    switch (category) {
        case ITEM_CATEGORY_JUNK:
            return CONTAINER_STYLE_JUNK;
        case ITEM_CATEGORY_LESSER:
            return CONTAINER_STYLE_LESSER;
        case ITEM_CATEGORY_HEALTH:
            return CONTAINER_STYLE_HEALTH;
        case ITEM_CATEGORY_BOSS_KEY:
            return CONTAINER_STYLE_BOSS_KEY;
        case ITEM_CATEGORY_SMALL_KEY:
            return CONTAINER_STYLE_SMALL_KEY;
        case ITEM_CATEGORY_SKULLTULA_TOKEN:
            return CONTAINER_STYLE_TOKEN;
        case ITEM_CATEGORY_MAJOR:
            return CONTAINER_STYLE_MAJOR;
        default:
            SPDLOG_WARN("[Randomizer] Item category {} has no container style", static_cast<int>(category));
            return CONTAINER_STYLE_JUNK;
    }
}

static bool IsGoldSkulltula(const Actor* actor) {
    return actor->id == ACTOR_EN_SW && ((actor->params & 0xE000) >> 0xD) != 0;
}

// A gold Skulltula hands its params to the token it spawns, which is where the check lives.
static RandomizerCheck GetGoldSkulltulaCheck(const Actor* actor) {
    if (RAND_GET_OPTION(RSK_SHUFFLE_TOKENS).Is(RO_TOKENSANITY_OFF)) {
        return RC_UNKNOWN_CHECK;
    }
    return OTRGlobals::Instance->gRandomizer->GetCheckFromActor(ACTOR_EN_SI, gPlayState->sceneNum, actor->params);
}

static CheckIdentity IdentifyActorCheck(const Actor* actor) {
    if (actor->id == ACTOR_EN_COW && actor->params == 0 && RAND_GET_OPTION(RSK_SHUFFLE_COWS)) {
        return ShuffleCows_IdentifyCow(actor);
    }
    if (actor->id == ACTOR_OBJ_COMB) {
        const CheckIdentity* identity = ObjectExtension::GetInstance().Get<CheckIdentity>(actor);
        if (identity != nullptr) {
            return *identity;
        }
    }
    CheckIdentity identity;
    identity.randomizerInf = RAND_INF_MAX;
    identity.randomizerCheck = IsGoldSkulltula(actor) ? GetGoldSkulltulaCheck(actor) : RC_UNKNOWN_CHECK;
    return identity;
}

static bool IsCheckObtained(const CheckIdentity& identity) {
    return identity.randomizerInf != RAND_INF_MAX && Flags_GetRandomizerInf(identity.randomizerInf);
}

static uint8_t GetActorContainerStyle(Actor* actor) {
    if (!IS_RANDO || actor == nullptr ||
        !CVarGetInteger(CVAR_ENHANCEMENT("ChestSizeAndTextureMatchContents"), 0)) {
        return CONTAINER_STYLE_NONE;
    }
    const CheckIdentity identity = IdentifyActorCheck(actor);
    if (identity.randomizerCheck == RC_UNKNOWN_CHECK || identity.randomizerCheck == RC_MAX ||
        IsCheckObtained(identity)) {
        return CONTAINER_STYLE_NONE;
    }
    const bool hidesContents = CVarGetInteger(CVAR_ENHANCEMENT("ChestSizeDependsStoneOfAgony"), 0) &&
                               !CHECK_QUEST_ITEM(QUEST_STONE_OF_AGONY);
    if (hidesContents) {
        return CONTAINER_STYLE_MAJOR;
    }
    const GetItemEntry item = Rando::Context::GetInstance()->GetFinalGIEntry(identity.randomizerCheck, true, GI_NONE);
    return GetCategoryStyle(Randomizer_AdjustItemCategory(item));
}

static void PublishContainerStyleService() {
    ModApi_PublishHostService("rando.container_style", reinterpret_cast<void*>(GetActorContainerStyle));
}

static RegisterShipInitFunc registerContainerStyleService(PublishContainerStyleService);
