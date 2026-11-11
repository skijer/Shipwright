#include "VanillaItems.h"

#include <vector>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
GetItemID RetrieveGetItemIDFromItemID(ItemID itemID);
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);
}

namespace {

struct BlockedItem {
    uint16_t vanillaItem;
    int32_t randoItem;
};

std::vector<BlockedItem> sBlockedItems;

const SOHCustomItemDefinition* FindClaim(uint16_t vanillaItem) {
    const SOHCustomItemDefinition* claim = CustomItemRegistry_FindReplacement(vanillaItem);
    if (claim == nullptr || claim->vanillaMode == SOH_VANILLA_ITEM_UPGRADE) {
        return nullptr;
    }
    return claim;
}

const SOHCustomItemDefinition* FindRandoClaim(int32_t randoItem) {
    if (randoItem == 0) {
        return nullptr;
    }
    for (uint32_t index = 0; index < CustomItemRegistry_GetCount(); index++) {
        const SOHCustomItemDefinition* definition = CustomItemRegistry_GetAt(index);
        if (definition != nullptr && definition->randoItem == randoItem &&
            definition->vanillaMode != SOH_VANILLA_ITEM_UPGRADE) {
            return definition;
        }
    }
    return nullptr;
}

bool IsBlockedVanillaItem(uint16_t vanillaItem) {
    for (const BlockedItem& blocked : sBlockedItems) {
        if (blocked.vanillaItem == vanillaItem) {
            return true;
        }
    }
    return false;
}

bool IsBlockedRandoItem(int32_t randoItem) {
    if (randoItem == 0) {
        return false;
    }
    for (const BlockedItem& blocked : sBlockedItems) {
        if (blocked.randoItem == randoItem) {
            return true;
        }
    }
    return false;
}

bool GrantClaimedItem(const SOHCustomItemDefinition* claim, bool isBlocked) {
    if (claim == nullptr) {
        return isBlocked;
    }
    if (claim->vanillaMode == SOH_VANILLA_ITEM_REPLACE) {
        CustomItemRegistry_SetOwned(claim->key, true);
    }
    return true;
}

void ResolveItemGive(PlayState* play, uint8_t* item, bool* handled) {
    *handled = GrantClaimedItem(FindClaim(*item), IsBlockedVanillaItem(*item));
}

void SweepInventory() {
    for (size_t slot = 0; slot < ARRAY_COUNT(gSaveContext.inventory.items); slot++) {
        const uint16_t item = gSaveContext.inventory.items[slot];
        if (item == ITEM_NONE) {
            continue;
        }
        const SOHCustomItemDefinition* claim = FindClaim(item);
        if (claim == nullptr && !IsBlockedVanillaItem(item)) {
            continue;
        }
        Inventory_DeleteItem(item, (uint16_t)slot);
        if (claim != nullptr && claim->vanillaMode == SOH_VANILLA_ITEM_REPLACE) {
            CustomItemRegistry_SetOwned(claim->key, true);
        }
    }
}

void RefuseInSaveEditor(int32_t slot, uint16_t item, bool* allowed) {
    if (VanillaItems_IsVanillaSuppressed(item)) {
        *allowed = false;
    }
}

} // namespace

void VanillaItems_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnResolveItemGive>(ResolveItemGive);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadFile>([](int32_t fileNum) { SweepInventory(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoUpdate>(SweepInventory);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveEditorInventory>(
        [](SaveContext* save) { SweepInventory(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveEditorItemEligibility>(RefuseInSaveEditor);
}

bool VanillaItems_Block(uint16_t vanillaItem, int32_t randoItem) {
    if (vanillaItem == 0) {
        SPDLOG_ERROR("[VanillaItems] Cannot block item 0");
        return false;
    }
    if (!IsBlockedVanillaItem(vanillaItem)) {
        sBlockedItems.push_back({ vanillaItem, randoItem });
    }
    return true;
}

const char* VanillaItems_GetCustomKey(uint16_t vanillaItem) {
    const SOHCustomItemDefinition* claim = FindClaim(vanillaItem);
    return claim != nullptr && claim->vanillaMode == SOH_VANILLA_ITEM_REPLACE ? claim->key : nullptr;
}

const char* VanillaItems_GetCustomKeyForRandoItem(int32_t randoItem) {
    const SOHCustomItemDefinition* claim = FindRandoClaim(randoItem);
    return claim != nullptr && claim->vanillaMode == SOH_VANILLA_ITEM_REPLACE ? claim->key : nullptr;
}

bool VanillaItems_GrantRandoItem(int32_t randoItem) {
    return GrantClaimedItem(FindRandoClaim(randoItem), IsBlockedRandoItem(randoItem));
}

bool VanillaItems_Give(PlayState* play, uint16_t vanillaItem) {
    if (play == nullptr) {
        return false;
    }
    const GetItemID getItemId = RetrieveGetItemIDFromItemID((ItemID)vanillaItem);
    if (getItemId == GI_MAX) {
        return false;
    }
    return GiveItemEntryWithoutActor(play, ItemTable_RetrieveEntry(MOD_NONE, getItemId)) != 0;
}

bool VanillaItems_Drop(PlayState* play, uint16_t vanillaItem) {
    const char* key = VanillaItems_GetCustomKey(vanillaItem);
    if (play == nullptr || key == nullptr) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    Vec3f position = player->actor.world.pos;
    position.x += Math_SinS(player->actor.shape.rot.y) * 60.0f;
    position.y += 20.0f;
    position.z += Math_CosS(player->actor.shape.rot.y) * 60.0f;

    EnItem00* drop = Item_DropCollectible2(play, &position, ITEM00_SOH_CUSTOM);
    return drop != nullptr && CustomItemRegistry_AttachDrop(&drop->actor, key);
}

bool VanillaItems_IsVanillaSuppressed(uint16_t vanillaItem) {
    return FindClaim(vanillaItem) != nullptr || IsBlockedVanillaItem(vanillaItem);
}

bool VanillaItems_IsRandoItemSuppressed(int32_t randoItem) {
    return FindRandoClaim(randoItem) != nullptr || IsBlockedRandoItem(randoItem);
}
