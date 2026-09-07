/**
 * o2r_loader.cpp - Generalist .o2r player-model loader
 *
 * See o2r_loader.h for design notes.
 */

#include "o2r_loader.h"
#include "soh/ResourceManagerHelpers.h"
#include "macros.h"
#include "variables.h"

#include <cstring>
#include <vector>
#include <spdlog/spdlog.h>

#define O2R_LOG(...) SPDLOG_INFO("[O2rLoader] " __VA_ARGS__)

namespace {

struct O2rEntry {
    char name[32];
    char skelOtrPath[128];
    char skelOtrPathChild[128];    // optional child-age variant ("" = use adult skel for both ages)
    FlexSkeletonHeader* skel;      // lazy-loaded on first force
    FlexSkeletonHeader* skelChild; // lazy-loaded on first force (child variant)
    bool loaded;
    bool loadedChild;
};

std::vector<O2rEntry> sModels;
s32 sForcedIdx = -1;
bool sInitialized = false;

// Saved player skeleton state during swap.
void** sSavedSkeleton = nullptr;
s32 sSavedDListCount = 0;

// Forward decl so EnsureInit can call the public Register.
void RegisterImpl(const char* name, const char* skelOtrPath);
void RegisterAgedImpl(const char* name, const char* skelOtrPath, const char* skelOtrPathChild);

void EnsureInit() {
    if (sInitialized)
        return;
    sInitialized = true;
    // Register known o2r-based models. Add additional entries here as needed.
    RegisterImpl("garo", "__OTR__objects/forms/garo/gGaroSkel");
    // Gerudo Player — Link-rigged gerudo body skin bundled inside soh.o2r.
    // The skel IS Link's 21-bone adult skel (`gLinkAdultSkel` Flex skeleton),
    // just with gerudo mesh + textures attached to each limb's DL. Repackaged
    // originally by tools/repack_gerudo_player.py from the artist-authored
    // "00 - Gerudo Player.o2r" out of its hijacking `alt/objects/object_link_boy/`
    // path into a non-conflicting namespace `objects/forms/gerudo/`.
    //
    // Because the skel IS Link-compatible, all of Player_DrawImpl works
    // naturally — no DrawNullBody, no hybrid render, no anim retargeting.
    // The body renders gerudo, animations play Link's vanilla, equipment
    // stays Link's vanilla (sword/shield/etc., since those resolve from
    // oot.o2r via paths the gerudo o2r doesn't shadow).
    //
    // The o2r also carries 11 baked PlayerAnimation resources at
    // `objects/forms/gerudo/gPlayerAnim_gerudo_*` (visible in the anim viewer).
    RegisterAgedImpl("gerudo", "__OTR__objects/forms/gerudo/object_link_boy/gLinkAdultSkel",
                     "__OTR__objects/forms/gerudo/object_link_child/gLinkChildSkel");
    // Kafei — converted from the retired N64_Kafei.pak by apps/pak_to_o2r.py and
    // bundled inside soh.o2r. Like every form it MIRRORS the vanilla player
    // object: whatever it wants to replace ships under the vanilla resource
    // name, and CustomForms_OverrideLimbDraw redirects to it at draw time.
    // Anything it doesn't ship keeps rendering vanilla, equipment included.
    RegisterAgedImpl("kafei", "__OTR__objects/forms/kafei/object_link_boy/gLinkAdultSkel",
                     "__OTR__objects/forms/kafei/object_link_child/gLinkChildSkel");
    // Keaton / Rito — visual forms; their models will ship in soh.o2r once the
    // Blender projects (form_models/keaton_form.blend, rito_form.blend) are
    // painted and exported. Until then LazyLoad fails gracefully → no swap.
    RegisterAgedImpl("keaton", "__OTR__objects/forms/keaton/object_link_boy/gLinkAdultSkel",
                     "__OTR__objects/forms/keaton/object_link_child/gLinkChildSkel");
    RegisterAgedImpl("rito", "__OTR__objects/forms/rito/object_link_boy/gLinkAdultSkel",
                     "__OTR__objects/forms/rito/object_link_child/gLinkChildSkel");
}

s32 FindByName(const char* name) {
    if (!name || !*name)
        return -1;
    for (size_t i = 0; i < sModels.size(); i++) {
        if (std::strcmp(sModels[i].name, name) == 0) {
            return (s32)i;
        }
    }
    return -1;
}

// Sanity gate, ported from pak_loader's IsValidLinkSkel. ResourceMgr_LoadSkeletonByName
// can return a non-NULL pointer to an UNRELATED resource when the requested path does
// not actually ship a Flex skeleton (e.g. the .o2r is missing/mismatched). Reading
// limbCount/segment off that gives garbage, and swapping the player skeleton to it is a
// guaranteed crash inside the flex walker (SkelAnime_DrawFlexLod). A real OOT-Link skel
// has limbCount in [1, 32] and a non-NULL segment (the limb/dList pointer table).
bool IsValidLinkSkel(SkeletonHeader* hdr) {
    if (hdr == nullptr)
        return false;
    if (hdr->limbCount == 0 || hdr->limbCount > 32)
        return false;
    if (hdr->segment == nullptr)
        return false;
    return true;
}

// Attempt to resolve the skeleton resource. Returns true on success.
bool LazyLoad(O2rEntry& e) {
    if (e.loaded)
        return true;
    SkeletonHeader* hdr = ResourceMgr_LoadSkeletonByName(e.skelOtrPath, nullptr);
    if (!IsValidLinkSkel(hdr)) {
        O2R_LOG("LazyLoad FAIL: '{}' could not resolve a valid Link skel at '{}' "
                "(hdr={}, limbCount={}) — falling back to vanilla Link, no swap",
                e.name, e.skelOtrPath, (void*)hdr, hdr ? hdr->limbCount : -1);
        return false;
    }
    e.skel = (FlexSkeletonHeader*)hdr;
    e.loaded = true;
    O2R_LOG("LazyLoad OK: '{}' (limbCount={}, dListCount={})", e.name, e.skel->sh.limbCount, e.skel->dListCount);
    return true;
}

// Resolve the child-age variant if the entry registered one. Non-fatal: on
// failure the adult skel is used for both ages (old single-skel behavior).
void LazyLoadChild(O2rEntry& e) {
    if (e.loadedChild || e.skelOtrPathChild[0] == '\0')
        return;
    SkeletonHeader* hdr = ResourceMgr_LoadSkeletonByName(e.skelOtrPathChild, nullptr);
    if (!IsValidLinkSkel(hdr)) {
        O2R_LOG("LazyLoadChild: '{}' has no valid child skel at '{}' — using adult skel for both ages", e.name,
                e.skelOtrPathChild);
        e.skelOtrPathChild[0] = '\0'; // don't retry every frame
        return;
    }
    e.skelChild = (FlexSkeletonHeader*)hdr;
    e.loadedChild = true;
    O2R_LOG("LazyLoadChild OK: '{}' (limbCount={}, dListCount={})", e.name, e.skelChild->sh.limbCount,
            e.skelChild->dListCount);
}

// The skeleton to draw with right now, honoring the current Link age.
FlexSkeletonHeader* ActiveSkelForAge(O2rEntry& e) {
    if (!LINK_IS_ADULT) {
        LazyLoadChild(e);
        if (e.loadedChild && e.skelChild)
            return e.skelChild;
    }
    return e.skel;
}

void RegisterAgedImpl(const char* name, const char* skelOtrPath, const char* skelOtrPathChild) {
    if (!name || !*name || !skelOtrPath || !*skelOtrPath)
        return;
    if (FindByName(name) >= 0)
        return; // already registered

    O2rEntry e{};
    std::strncpy(e.name, name, sizeof(e.name) - 1);
    std::strncpy(e.skelOtrPath, skelOtrPath, sizeof(e.skelOtrPath) - 1);
    if (skelOtrPathChild && *skelOtrPathChild) {
        std::strncpy(e.skelOtrPathChild, skelOtrPathChild, sizeof(e.skelOtrPathChild) - 1);
    }
    e.skel = nullptr;
    e.skelChild = nullptr;
    e.loaded = false;
    e.loadedChild = false;
    sModels.push_back(e);
}

void RegisterImpl(const char* name, const char* skelOtrPath) {
    RegisterAgedImpl(name, skelOtrPath, nullptr);
}

} // namespace

extern "C" void O2rLoader_Init(void) {
    // Idempotent — defaults register lazily anyway, but allow explicit init.
    EnsureInit();
}

extern "C" void O2rLoader_Register(const char* name, const char* skelOtrPath) {
    EnsureInit();
    RegisterImpl(name, skelOtrPath);
}

extern "C" void O2rLoader_ForceModel(const char* name) {
    EnsureInit();
    O2R_LOG("ForceModel('{}')", name ? name : "<null>");
    if (!name || !*name) {
        sForcedIdx = -1;
        return;
    }
    s32 idx = FindByName(name);
    if (idx < 0) {
        O2R_LOG("ForceModel FAIL: no registered entry named '{}'", name);
        return;
    }
    if (!LazyLoad(sModels[idx]))
        return;
    sForcedIdx = idx;
    O2R_LOG("ForceModel ACTIVE: '{}' (idx={})", name, idx);
}

extern "C" void O2rLoader_ClearForcedModel(void) {
    O2R_LOG("ClearForcedModel");
    sForcedIdx = -1;
}

extern "C" u8 O2rLoader_HasActiveModel(void) {
    return (sForcedIdx >= 0 && sForcedIdx < (s32)sModels.size() && sModels[sForcedIdx].loaded) ? 1 : 0;
}

extern "C" const char* O2rLoader_GetForcedName(void) {
    if (!O2rLoader_HasActiveModel())
        return nullptr;
    return sModels[sForcedIdx].name;
}

extern "C" void O2rLoader_SwapSkeleton(Player* player) {
    if (!O2rLoader_HasActiveModel() || !player)
        return;
    FlexSkeletonHeader* flex = ActiveSkelForAge(sModels[sForcedIdx]);
    // Re-validate before writing into player->skelAnime. Skipping the swap here
    // leaves Link's vanilla skeleton intact instead of crashing the flex walker.
    if (!flex || !IsValidLinkSkel(&flex->sh))
        return;

    sSavedSkeleton = player->skelAnime.skeleton;
    sSavedDListCount = player->skelAnime.dListCount;

    player->skelAnime.skeleton = flex->sh.segment;
    player->skelAnime.dListCount = flex->dListCount;
}

extern "C" void O2rLoader_RestoreSkeleton(Player* player) {
    if (!sSavedSkeleton || !player)
        return;
    player->skelAnime.skeleton = sSavedSkeleton;
    player->skelAnime.dListCount = sSavedDListCount;
    sSavedSkeleton = nullptr;
    sSavedDListCount = 0;
}
