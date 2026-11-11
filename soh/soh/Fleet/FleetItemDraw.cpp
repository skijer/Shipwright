#include "FleetItemDraw.h"
#include "FleetGame.h"

#include <cstring>
#include <iterator>
#include <fast/Fast3dGui.h>
#include <fast/resource/type/Texture.h>
#include <ship/Context.h>
#include <ship/GameRegistry.h>
#include <ship/window/Window.h>
#include <soh_assets.h>

#include "soh/Enhancements/randomizer/Plandomizer.h"
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/frame_interpolation.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kGame = "oot";

// A parked game state keeps the matrix stack and the archives alive; before the first one there is nothing to draw
// with.
bool CanDraw() {
    const auto game = Ship::GameRegistry::Get(kGame);
    return gGameState != nullptr && game != nullptr && game->Resources != nullptr;
}

// Draw functions read only the graphics context, the frame counters and the billboard from the play state.
PlayState& BorrowPlayState(FleetItemDraw& request, GraphicsContext& gfx) {
    static PlayState play;
    // OPEN_DISPS marks the overlay list too; item drawings never submit to it, so the marks are discarded.
    static Gfx discardedOverlay[1024];
    // Draws that scan back through their list (for the last env color) stop at bufp: the start of the lent arena.
    gfx.overlay.bufp = discardedOverlay;
    gfx.overlay.p = discardedOverlay;
    gfx.overlay.d = discardedOverlay + std::size(discardedOverlay);
    gfx.polyOpa.bufp = static_cast<Gfx*>(request.opaHead);
    gfx.polyOpa.p = static_cast<Gfx*>(request.opaHead);
    gfx.polyOpa.d = static_cast<Gfx*>(request.opaTail);
    gfx.polyXlu.bufp = static_cast<Gfx*>(request.xluHead);
    gfx.polyXlu.p = static_cast<Gfx*>(request.xluHead);
    gfx.polyXlu.d = static_cast<Gfx*>(request.xluTail);
    play.state.gfxCtx = &gfx;
    play.state.frames = request.frames;
    play.gameplayFrames = request.frames;
    std::memcpy(&play.billboardMtxF, request.billboard, sizeof(play.billboardMtxF));
    return play;
}

void ReturnArenas(FleetItemDraw& request, const GraphicsContext& gfx) {
    request.opaHead = gfx.polyOpa.p;
    request.opaTail = gfx.polyOpa.d;
    request.xluHead = gfx.polyXlu.p;
    request.xluTail = gfx.polyXlu.d;
}

} // namespace

namespace {

std::shared_ptr<Fast::Fast3dGui> Gui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}

// Mod icons are never preloaded into the GUI: each is loaded on first use, under its own path.
ImTextureID LoadIconTexture(const char* path) {
    const auto gui = Gui();
    if (gui->HasTextureByName(path)) {
        return gui->GetTextureByName(path);
    }
    Ship::GameScope scope(kGame);
    const auto texture = std::dynamic_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true));
    if (texture == nullptr) {
        return 0;
    }
    gui->LoadGuiTexture(path, *texture, "", ImVec4(1, 1, 1, 1));
    return gui->GetTextureByName(path);
}

// A mod item, or the mod item that replaces a vanilla one, shows the icon its mod registered.
const char* FindCustomIconPath(RandomizerGet item, const GetItemEntry& entry) {
    const SOHCustomItemDefinition* definition = nullptr;
    if (const char* key = RandoItems_GetKey(item)) {
        definition = CustomItemRegistry_Find(key);
        // A progressive chain's key is no item: it shows the level it would grant next.
        const char* next = definition == nullptr ? ProgressiveItems_GetNextItem(key, SOH_RANDO_INF_SAVE) : nullptr;
        if (next != nullptr) {
            definition = CustomItemRegistry_Find(next);
        }
    } else if (entry.itemId == ITEM_CUSTOM) {
        definition = CustomItemRegistry_FindByRuntimeId(entry.tableId);
    } else {
        definition = CustomItemRegistry_FindReplacement(entry.itemId);
    }
    return definition != nullptr ? CustomItemRegistry_ResolveTexture(definition->key, SOH_ITEM_ICON_INVENTORY)
                                 : nullptr;
}

// Randomizer-only items the plandomizer draws nothing for borrow the icon of their vanilla family.
const char* FindVanillaIconName(RandomizerGet item) {
    if (item >= RG_DEATH_MOUNTAIN_CRATER_BEAN_SOUL && item <= RG_ZORAS_RIVER_BEAN_SOUL) {
        return "ITEM_BEAN";
    }
    if (item >= RG_GUARD_HOUSE_KEY && item <= RG_FISHING_HOLE_KEY) {
        return "ITEM_KEY_SMALL";
    }
    if (item >= RG_SPEAK_DEKU && item <= RG_SPEAK_ZORA) {
        return "ITEM_NUT";
    }
    switch (item) {
        case RG_MASTER_SWORD:
            return "ITEM_SWORD_MASTER";
        case RG_DEKU_STICK_BAG:
        case RG_STICK_UPGRADE_INF:
        case RG_DEKU_STICK_CAPACITY_20:
        case RG_DEKU_STICK_CAPACITY_30:
            return "ITEM_STICK";
        case RG_DEKU_NUT_BAG:
        case RG_NUT_UPGRADE_INF:
        case RG_DEKU_NUT_CAPACITY_30:
        case RG_DEKU_NUT_CAPACITY_40:
            return "ITEM_NUT";
        case RG_HINT:
            return "ITEM_MASK_TRUTH";
        case RG_TYCOON_WALLET:
        case RG_WALLET_INF:
            return "ITEM_WALLET_GIANT";
        case RG_CHILD_WALLET:
            return "ITEM_WALLET_ADULT";
        case RG_BRONZE_SCALE:
            return "ITEM_SCALE_SILVER";
        case RG_POWER_BRACELET:
            return "ITEM_BRACELET";
        case RG_CLIMB:
        case RG_CRAWL:
            return "ITEM_BOOTS_KOKIRI";
        case RG_OPEN_CHEST:
            return "ITEM_KEY_SMALL";
        case RG_QUIVER_INF:
            return "ITEM_QUIVER_50";
        case RG_BOMB_BAG_INF:
            return "ITEM_BOMB_BAG_40";
        case RG_BULLET_BAG_INF:
            return "ITEM_BULLET_BAG_50";
        case RG_MAGIC_INF:
            return "ITEM_MAGIC_LARGE";
        case RG_BOMBCHU_INF:
            return "ITEM_BOMBCHU";
        case RG_HOOKSHOT:
            return "ITEM_HOOKSHOT";
        case RG_LONGSHOT:
            return "ITEM_LONGSHOT";
        case RG_KEATON_MASK:
            return "ITEM_MASK_KEATON";
        case RG_SKULL_MASK:
            return "ITEM_MASK_SKULL";
        case RG_SPOOKY_MASK:
            return "ITEM_MASK_SPOOKY";
        case RG_BUNNY_HOOD:
            return "ITEM_MASK_BUNNY";
        case RG_GORON_MASK:
            return "ITEM_MASK_GORON";
        case RG_ZORA_MASK:
            return "ITEM_MASK_ZORA";
        case RG_GERUDO_MASK:
            return "ITEM_MASK_GERUDO";
        case RG_MASK_OF_TRUTH:
            return "ITEM_MASK_TRUTH";
        default:
            return nullptr;
    }
}

ImTextureID FindFallbackTexture(RandomizerGet item, const GetItemEntry& entry) {
    if (const char* path = FindCustomIconPath(item, entry)) {
        return LoadIconTexture(path);
    }
    if (item == RG_ROCS_FEATHER) {
        return LoadIconTexture(gRocsFeatherTex);
    }
    const char* name = FindVanillaIconName(item);
    return name != nullptr ? Gui()->GetTextureByName(name) : 0;
}

} // namespace

bool FleetItemDraw_Icon(const char* item, FleetItemIcon* icon) {
    if (item == nullptr || icon == nullptr) {
        return false;
    }
    const auto found = Rando::StaticData::itemNameToEnum.find(item);
    if (found == Rando::StaticData::itemNameToEnum.end()) {
        return false;
    }
    const Rando::Item& randoItem = Rando::StaticData::RetrieveItem(found->second);
    PlandomizerItemImage image = Plandomizer_GetItemImage(randoItem);
    if (image.texture == 0) {
        image.texture = FindFallbackTexture(found->second, randoItem.GetGIEntry_Copy());
        image.uv0 = ImVec2(0, 0);
        image.uv1 = ImVec2(1, 1);
    }
    if (image.texture == 0) {
        return false;
    }
    *icon = { reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(image.texture)),
              { image.uv0.x, image.uv0.y },
              { image.uv1.x, image.uv1.y },
              { image.tint.x, image.tint.y, image.tint.z, image.tint.w } };
    return true;
}

bool FleetItemDraw_Draw(FleetItemDraw* request) {
    if (request == nullptr || request->item == nullptr || !CanDraw()) {
        return false;
    }
    const auto item = Rando::StaticData::itemNameToEnum.find(request->item);
    if (item == Rando::StaticData::itemNameToEnum.end()) {
        return false;
    }
    Ship::GameScope scope(kGame);
    static GraphicsContext gfx;
    PlayState& play = BorrowPlayState(*request, gfx);
    PlayState* runningPlay = gPlayState;
    gPlayState = &play;
    Matrix_Push();
    Matrix_Put(reinterpret_cast<MtxF*>(request->model));

    bool submitted = false;
    OPEN_DISPS(&gfx);
    gSPComboRMPush(POLY_OPA_DISP++, kGame);
    gSPComboRMPush(POLY_XLU_DISP++, kGame);
    const Gfx* opaBeforeDraw = POLY_OPA_DISP;
    const Gfx* xluBeforeDraw = POLY_XLU_DISP;
    GetItemEntry_Draw(&play, Rando::StaticData::RetrieveItem(item->second).GetGIEntry_Copy());
    submitted = POLY_OPA_DISP != opaBeforeDraw || POLY_XLU_DISP != xluBeforeDraw;
    gSPComboRMPop(POLY_OPA_DISP++);
    gSPComboRMPop(POLY_XLU_DISP++);
    CLOSE_DISPS(&gfx);

    Matrix_Pop();
    gPlayState = runningPlay;
    if (!submitted) {
        return false;
    }
    ReturnArenas(*request, gfx);
    return true;
}

bool FleetItemDraw_DrawForeign(PlayState* play, const char* owner, const char* item) {
    const auto draw = reinterpret_cast<FleetDrawForeignItemFn>(FleetGame_GetHostFunction("DrawForeignItem"));
    if (draw == nullptr) {
        return false;
    }
    GraphicsContext* gfx = play->state.gfxCtx;
    FleetItemDraw request = { .owner = owner,
                              .item = item,
                              .opaHead = gfx->polyOpa.p,
                              .opaTail = gfx->polyOpa.d,
                              .xluHead = gfx->polyXlu.p,
                              .xluTail = gfx->polyXlu.d,
                              .frames = play->state.frames };
    MtxF model;
    Matrix_Get(&model);
    std::memcpy(request.model, &model, sizeof(request.model));
    std::memcpy(request.billboard, &play->billboardMtxF, sizeof(request.billboard));
    if (!draw(&request)) {
        return false;
    }
    gfx->polyOpa.p = static_cast<Gfx*>(request.opaHead);
    gfx->polyOpa.d = static_cast<Gfx*>(request.opaTail);
    gfx->polyXlu.p = static_cast<Gfx*>(request.xluHead);
    gfx->polyXlu.d = static_cast<Gfx*>(request.xluTail);
    return true;
}
