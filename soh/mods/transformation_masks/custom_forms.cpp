/**
 * custom_forms.cpp — registry of custom player forms. See custom_forms.h.
 */
#include "custom_forms.h"
#include "z64.h"
#include "z64item.h"
#include "functions.h"
#include "variables.h" // gSaveContext (Farore's Wind cell contents)
#include "macros.h"    // LINK_IS_ADULT (root-limb scale is age-dependent)
#include "mods/o2r_loader/o2r_loader.h"
#include "mods/extended_inventory.h"
#include "mods/nei_save.h" // Nei_Save()->ritoMaskFlags (Rito Mask ownership)
#include "soh/ResourceManagerHelpers.h"

#include <libultraship/bridge.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// The single source of truth for custom forms. Asset namespace for each row
// with a modelName: objects/forms/<modelName>/ inside soh.o2r.
extern "C" const CustomFormDef gCustomForms[] = {
    // Full MmForm transformations (state machine in mm_player_form.cpp)
    { "garo",
      "Garo",
      CUSTOM_FORM_FULL,
      "gMods.GaroMaskTransform",
      1,
      { ITEM_MM_MASK_GARO, ITEM_NONE, ITEM_NONE },
      1.0f },
    { "gerudo",
      "Gerudo",
      CUSTOM_FORM_FULL,
      "gMods.GerudoMaskTransform",
      0,
      { ITEM_MASK_GERUDO, ITEM_NONE, ITEM_NONE },
      1.0f },
    // Pikachu is its own thing (SSBB engine, pikachu_form.cpp); listed so the
    // trigger item + gate live in the same table. Pokeball ONLY — the Keaton
    // Mask now belongs to the Keaton form.
    { NULL, "Pikachu", CUSTOM_FORM_FULL, "gPikachuMode", 0, { ITEM_POKEBALL, ITEM_NONE, ITEM_NONE }, 1.0f },
    // Wolf Link reuses Pikachu's internal custom-skeleton form slot, but owns a
    // separate runtime renderer and a loose binary asset instead of an o2r model.
    { NULL,
      "Wolf Link",
      CUSTOM_FORM_FULL,
      "gMods.WolfLink.Enabled",
      1,
      { EXT_ITEM_SHADOW_CRYSTAL, ITEM_NONE, ITEM_NONE },
      1.0f },

    // Kafei: FULL transformation, but the ONLY form that keeps vanilla Link's draw
    // path. He ships a complete 1126-file mirror of object_link_boy AND
    // object_link_child — every weapon, shield, gauntlet, boot and eye/mouth texture —
    // and MmForm_Draw would never ask for any of it. MmForm_IsKafeiFormActive() makes
    // MmForm_IsTransformed() report 0 for him so the vanilla path stays in charge; see
    // that function in mm_player_form.cpp for the full reasoning.
    { "kafei",
      "Kafei",
      CUSTOM_FORM_FULL,
      "gMods.KafeiMaskTransform",
      0,
      { ITEM_MM_MASK_KAFEI, ITEM_NONE, ITEM_NONE },
      1.0f },
    // Keaton: FULL transformation (cutscene + flash + form state) like Rito —
    // MM_PLAYER_FORM_KEATON in mm_player_form.cpp. Body exported from
    // form_models/keaton_link_rig.blend.
    // 0.335, not the rito's 2376/3377: this rig's legs are deliberately short, so
    // the body hangs only 1091 below the root instead of 2336 and anything higher
    // leaves it floating. Keep it equal to
    // sFormProps[MM_PLAYER_FORM_KEATON].rootAnimScale, which carries the measurement.
    { "keaton",
      "Keaton",
      CUSTOM_FORM_FULL,
      "gMods.KeatonMaskTransform",
      1,
      { ITEM_MASK_KEATON, ITEM_MM_MASK_KEATON, ITEM_NONE },
      0.335f },
    // Rito: FULL transformation (cutscene + flash + form state) like Gerudo, not a
    // bare skin — the state machine lives in mm_player_form.cpp as
    // MM_PLAYER_FORM_RITO. Model exported from form_models/rito_form.blend by
    // apps/form_blend_dump.py + apps/form_blend_to_assets.py; the mesh is rigged to
    // MM's human-Link skeleton, whose jointPos are byte-identical to OoT's CHILD
    // skeleton — hence the adult root scale (2376/3377), which is duplicated in
    // sFormProps[MM_PLAYER_FORM_RITO].rootAnimScale. Keep the two values equal: the
    // MmForm draw path uses that one, and this one covers the few frames of the
    // cutscene fade where the model is already forced but the form is not ACTIVE yet.
    // Trigger item shares the Farore's Wind cell (see ITEM_RITO_MASK).
    { "rito", "Rito", CUSTOM_FORM_FULL, "gMods.RitoForm", 1, { ITEM_RITO_MASK, ITEM_NONE, ITEM_NONE }, 0.7036f },
};
extern "C" const s32 gCustomFormCount = (s32)(sizeof(gCustomForms) / sizeof(gCustomForms[0]));

static const CustomFormDef* sActiveSkin = nullptr;

extern "C" const CustomFormDef* CustomForms_ByName(const char* modelName) {
    if (modelName == nullptr)
        return nullptr;
    for (s32 i = 0; i < gCustomFormCount; i++) {
        if (gCustomForms[i].modelName != nullptr && std::strcmp(gCustomForms[i].modelName, modelName) == 0) {
            return &gCustomForms[i];
        }
    }
    return nullptr;
}

extern "C" const CustomFormDef* CustomForms_ByItem(s32 itemId) {
    if (itemId == ITEM_NONE)
        return nullptr;
    for (s32 i = 0; i < gCustomFormCount; i++) {
        for (s32 j = 0; j < 3; j++) {
            if (gCustomForms[i].items[j] == itemId) {
                return &gCustomForms[i];
            }
        }
    }
    return nullptr;
}

static bool GateOn(const CustomFormDef* def) {
    if (def->cvar == nullptr || def->cvar[0] == '\0')
        return true;
    return CVarGetInteger(def->cvar, def->cvarDefault) != 0;
}

extern "C" const CustomFormDef* CustomForms_SkinByItem(s32 itemId) {
    const CustomFormDef* def = CustomForms_ByItem(itemId);
    if (def == nullptr || def->kind != CUSTOM_FORM_SKIN)
        return nullptr;
    if (!GateOn(def))
        return nullptr;
    return def;
}

extern "C" u8 CustomForms_SkinAvailable(const CustomFormDef* def) {
    if (def == nullptr || def->modelName == nullptr)
        return 0;
    char path[160];
    std::snprintf(path, sizeof(path), "objects/forms/%s/*", def->modelName);
    int count = 0;
    char** list = ResourceMgr_ListFiles(path, &count);
    if (list != nullptr) {
        for (int i = 0; i < count; i++)
            free(list[i]);
        free(list);
    }
    return count > 0 ? 1 : 0;
}

extern "C" u8 CustomForms_ToggleSkin(const CustomFormDef* def) {
    if (def == nullptr || def->kind != CUSTOM_FORM_SKIN || def->modelName == nullptr)
        return 0;
    if (sActiveSkin == def) {
        O2rLoader_ClearForcedModel();
        sActiveSkin = nullptr;
        return 1;
    }
    O2rLoader_ForceModel(def->modelName);
    // ForceModel validates the skeleton (LazyLoad); only mark active on success.
    const char* forced = O2rLoader_GetForcedName();
    if (forced != nullptr && std::strcmp(forced, def->modelName) == 0) {
        sActiveSkin = def;
        return 1;
    }
    sActiveSkin = nullptr;
    return 0;
}

extern "C" const char* CustomForms_ActiveSkin(void) {
    return sActiveSkin != nullptr ? sActiveSkin->modelName : nullptr;
}

// ============================================================================
// Draw: vanilla-path mirroring
//
// A form's o2r mirrors the vanilla player object — every resource it wants to
// replace ships under `objects/forms/<model>/object_link_boy|child/<vanilla
// symbol>`. At draw time the vanilla override still runs and still decides WHAT
// Link should be showing (open hand, fist + Master Sword, shield on back, …);
// we merely redirect the resource it chose to the form's copy.
//
// The redirect only happens when the form actually ships that resource, so a
// model that only replaces the body keeps working: every piece of equipment it
// doesn't carry simply renders as vanilla. Nothing about equipment logic, item
// handling or future items has to know that forms exist.
// ============================================================================

typedef s32 (*OverrideLimbDrawFn)(PlayState*, s32, Gfx**, Vec3f*, Vec3s*, void*);
static OverrideLimbDrawFn sChainedOverride = nullptr;

static const char kOtrPrefix[] = "__OTR__";
static const char kVanillaObjects[] = "objects/object_link_";

// Returns the "object_link_boy/gFooDL" part of a vanilla player resource path,
// or NULL when the path isn't one (already-custom paths included).
static const char* VanillaPlayerLeaf(const char* path) {
    if (path == nullptr)
        return nullptr;
    const char* p = path;
    if (std::strncmp(p, kOtrPrefix, sizeof(kOtrPrefix) - 1) == 0) {
        p += sizeof(kOtrPrefix) - 1;
    }
    if (std::strncmp(p, kVanillaObjects, sizeof(kVanillaObjects) - 1) != 0) {
        return nullptr;
    }
    return p + (sizeof("objects/") - 1); // "object_link_boy/gFooDL"
}

static bool IsFormPath(const char* path, const char* model) {
    if (path == nullptr || model == nullptr)
        return false;
    char prefix[128];
    std::snprintf(prefix, sizeof(prefix), "objects/forms/%s/", model);
    return std::strstr(path, prefix) != nullptr;
}

// THE rule of the whole system: if the active form ships a resource under the
// vanilla name the engine just asked for, hand back the form's copy; otherwise
// return NULL and let vanilla answer. Every hook below is a one-line use of it.
//
// This runs at the points where a vanilla resource NAME is turned into a
// pointer, which is the only place a redirect can still happen — once the
// engine has resolved a path to a Gfx* there is no name left to match on.
extern "C" void* CustomForms_ResolveVanillaResource(const char* vanillaPath) {
    const char* model = O2rLoader_GetForcedName();
    if (model == nullptr || vanillaPath == nullptr)
        return nullptr;
    const char* leaf = VanillaPlayerLeaf(vanillaPath);
    if (leaf == nullptr)
        return nullptr;

    char path[256];
    std::snprintf(path, sizeof(path), "objects/forms/%s/%s", model, leaf);
    return ResourceMgr_LoadGfxByName(path);
}

// Same lookup for a plain texture symbol (eyes/mouth), which the engine binds
// to a segment instead of writing into a display list.
extern "C" void* CustomForms_ResolveVanillaTexture(const char* vanillaSymbol) {
    const char* model = O2rLoader_GetForcedName();
    if (model == nullptr || vanillaSymbol == nullptr)
        return nullptr;
    const char* leaf = VanillaPlayerLeaf(vanillaSymbol);
    if (leaf == nullptr)
        return nullptr;

    char path[256];
    std::snprintf(path, sizeof(path), "objects/forms/%s/%s", model, leaf);
    return ResourceMgr_LoadTexOrDListByName(path);
}

extern "C" void CustomForms_SetChainedOverride(void* fn) {
    sChainedOverride = (OverrideLimbDrawFn)fn;
}

extern "C" u8 CustomForms_WantsPathSwap(void) {
    const char* model = O2rLoader_GetForcedName();
    // Garo draws through its own non-Link rig, so it never takes this path.
    return (model != nullptr && std::strcmp(model, "garo") != 0) ? 1 : 0;
}

extern "C" s32 CustomForms_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* arg) {
    (void)play;
    const char* model = O2rLoader_GetForcedName();

    // Root limb — SkelAnime hands the override 1-based limb indices, so 1 IS the
    // root. Its position comes from the ANIMATION (jointTable[0]), not from the
    // skeleton, so a rig with different proportions than the animation expects
    // ends up floating (or sunk). MM's forms solve this by scaling the root
    // position rather than the model (sFormProps[].rootAnimScale,
    // mm_player_form.cpp:13898) — one model, correct height. Same trick here.
    if (limbIndex == 1 && pos != nullptr && LINK_IS_ADULT) {
        const CustomFormDef* def = CustomForms_ByName(model);
        if (def != nullptr && def->rootScaleAdult != 1.0f) {
            pos->x *= def->rootScaleAdult;
            pos->y *= def->rootScaleAdult;
            pos->z *= def->rootScaleAdult;
        }
    }

    // The limb's own display list, before the vanilla override gets to replace it.
    Gfx* ownDL = *dList;
    bool ownIsForm = IsFormPath((const char*)ownDL, model);

    s32 ret = 0;
    if (sChainedOverride != nullptr) {
        ret = sChainedOverride(play, limbIndex, dList, pos, rot, arg);
    }
    if (*dList == nullptr || model == nullptr) {
        return ret;
    }

    const char* leaf = VanillaPlayerLeaf((const char*)*dList);
    if (leaf == nullptr) {
        return ret; // not a vanilla player resource — leave it alone
    }

    // Does this form ship its own version of exactly that resource?
    char path[256];
    std::snprintf(path, sizeof(path), "objects/forms/%s/%s", model, leaf);
    Gfx* mirrored = ResourceMgr_LoadGfxByName(path);
    if (mirrored != nullptr) {
        *dList = mirrored;
        return ret;
    }

    // It doesn't. For body limbs, keep the model's own mesh rather than letting
    // vanilla paint Link's body part over it (that is how Link's belt used to
    // reappear on a custom waist). For the hands and the sheath we deliberately
    // fall through to vanilla instead: the held item stays visible, which is
    // what "the form still lets you use equipment" means.
    if (ownIsForm && limbIndex != PLAYER_LIMB_L_HAND && limbIndex != PLAYER_LIMB_R_HAND &&
        limbIndex != PLAYER_LIMB_SHEATH) {
        *dList = ownDL;
    }
    return ret;
}

extern "C" u8 CustomForms_TrySkinItem(PlayState* play, Player* player, s32 itemId) {
    (void)play;
    const CustomFormDef* skin = CustomForms_SkinByItem(itemId);
    if (skin == nullptr || player == nullptr)
        return 0;
    if (!CustomForms_ToggleSkin(skin))
        return 0; // model not shipped — cosmetic fallback
    Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
    player->stateFlags2 |= PLAYER_STATE2_FOOTSTEP;
    return 1;
}

// ============================================================================
// Rito Mask — sharing the Farore's Wind cell
//
// Same shape as Roc's Feather sharing the Nayru's Love cell (RocsFeatherCycle.c):
// ONE inventory cell holds either item and the kaleido cycler (A on the cell,
// stick left/right) flips between them. Nothing new is stored — the cell itself
// is the state — so the pair is only offered to a save that owns Farore's Wind.
// Using the mask goes through the normal item-use path: ExtPlayer_GetItemAction
// aliases it to a mask action, and z_player.c's mask branch hands it to
// CustomForms_TrySkinItem, which finds it in the "rito" row above.
// ============================================================================

static const CustomFormDef* RitoRow() {
    const CustomFormDef* def = CustomForms_ByName("rito");
    return (def != nullptr && GateOn(def)) ? def : nullptr;
}

// Ownership lives in the save (Nei_Save()->ritoMaskFlags) for one reason: the cell
// shows ONE item, so it cannot answer "do you also own the other one?". Roc's Feather
// solves the same problem with its two RAND_INF flags; this is the non-rando version.
//
// Called once per pause frame before the cycler runs (the adult-trade wheel does the
// same at the top of KaleidoScope_HandleItemCycles). It only RECORDS: the mask is
// reached by cycling a cell that already holds Farore's Wind, never by seeding.
//
// Record that this file owns whatever is sitting in the shared cell. Public because
// the save editor overwrites that cell directly: dropping the mask onto a cell that
// held Farore's Wind would otherwise erase the spell with nothing remembering it
// existed, and the cycle could never give it back.
extern "C" void RitoItem_NoteCellItem(s32 item) {
    NeiSaveData* nei = Nei_Save();
    if (item == ITEM_FARORES_WIND) {
        nei->ritoMaskFlags |= RITO_FLAG_FARORES_OWNED;
    } else if (item == ITEM_RITO_MASK) {
        nei->ritoMaskFlags |= RITO_FLAG_MASK_OWNED;
    }
}

extern "C" void RitoItem_SyncCell(void) {
    if (RitoRow() == nullptr) {
        return;
    }
    NeiSaveData* nei = Nei_Save();
    u8* cell = &gSaveContext.inventory.items[SLOT_FARORES_WIND];

    RitoItem_NoteCellItem(*cell);
    // Enabling the form grants the mask, but it must NEVER put it in the cell: seeding an empty one
    // fabricated the mask into a slot the randomizer owns (every seed opened the pause menu already
    // holding it) and handed it out without the spell, against this file's own contract. Writing is
    // the cycler's job — this stays a pure query, exactly like RocsFeatherCycle.c.
    nei->ritoMaskFlags |= RITO_FLAG_MASK_OWNED;
}

// The item this cell can flip to, or ITEM_NONE when there is nothing to cycle.
// Both directions of a two-item cycle are the same answer, which is why the
// kaleido call passes it as prev AND next.
extern "C" s32 RitoItem_OtherItem(void) {
    if (RitoRow() == nullptr) {
        return ITEM_NONE;
    }
    const u8 flags = Nei_Save()->ritoMaskFlags;
    u8 cur = gSaveContext.inventory.items[SLOT_FARORES_WIND];
    if (cur == ITEM_FARORES_WIND) {
        return (flags & RITO_FLAG_MASK_OWNED) ? ITEM_RITO_MASK : ITEM_NONE;
    }
    if (cur == ITEM_RITO_MASK) {
        // Only offer the spell back to a save that actually had it — otherwise the
        // shared cell would hand out a Farore's Wind nobody ever earned.
        return (flags & RITO_FLAG_FARORES_OWNED) ? ITEM_FARORES_WIND : ITEM_NONE;
    }
    return ITEM_NONE;
}

extern "C" u8 RitoItem_CanCycle(void) {
    return RitoItem_OtherItem() != ITEM_NONE;
}

extern "C" void CustomForms_ClearSkin(void) {
    if (sActiveSkin != nullptr) {
        // Only clear the forced model if it's still ours (a FULL form like
        // gerudo may have replaced it via its own ForceModel).
        const char* forced = O2rLoader_GetForcedName();
        if (forced != nullptr && sActiveSkin->modelName != nullptr &&
            std::strcmp(forced, sActiveSkin->modelName) == 0) {
            O2rLoader_ClearForcedModel();
        }
        sActiveSkin = nullptr;
    }
}
