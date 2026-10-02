#include "FormModel.h"

#include <cstring>
#include <string>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
}

namespace {

constexpr const char* OtrPrefix = "__OTR__";
constexpr const char* VanillaObjectPrefix = "objects/object_link_";
constexpr const char* AdultSkeleton = "/object_link_boy/gLinkAdultSkel";
constexpr const char* ChildSkeleton = "/object_link_child/gLinkChildSkel";
constexpr const char* DisableLodCvar = CVAR_ENHANCEMENT("DisableLOD");

std::string sModelPath;
float sRootScaleAdult = 1.0f;
float sRootScaleChild = 1.0f;
float sRootDrop = 0.0f;
FlexSkeletonHeader* sSkeletonAdult = nullptr;
FlexSkeletonHeader* sSkeletonChild = nullptr;
bool sTriedAdult = false;
bool sTriedChild = false;
void** sSavedSkeleton = nullptr;
s32 sSavedDListCount = 0;
bool sForcedNearLod = false;

void ForceNearLod(bool formActive) {
    if (formActive && !sForcedNearLod && CVarGetInteger(DisableLodCvar, 0) == 0) {
        CVarSetInteger(DisableLodCvar, 1);
        sForcedNearLod = true;
    } else if (!formActive && sForcedNearLod) {
        CVarSetInteger(DisableLodCvar, 0);
        sForcedNearLod = false;
    }
}

bool IsLinkSkeleton(SkeletonHeader* header) {
    return header != nullptr && header->limbCount > 0 && header->limbCount <= 32 && header->segment != nullptr;
}

FlexSkeletonHeader* LoadSkeleton(const char* suffix) {
    const std::string path = sModelPath + suffix;
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(path.c_str(), nullptr);
    if (!IsLinkSkeleton(header)) {
        SPDLOG_WARN("[FormModel] '{}' is not a Link skeleton; keeping Link's body", path);
        return nullptr;
    }
    return reinterpret_cast<FlexSkeletonHeader*>(header);
}

FlexSkeletonHeader* ActiveSkeleton() {
    if (!LINK_IS_ADULT) {
        if (!sTriedChild) {
            sTriedChild = true;
            sSkeletonChild = LoadSkeleton(ChildSkeleton);
        }
        if (sSkeletonChild != nullptr) {
            return sSkeletonChild;
        }
    }
    if (!sTriedAdult) {
        sTriedAdult = true;
        sSkeletonAdult = LoadSkeleton(AdultSkeleton);
    }
    return sSkeletonAdult;
}

const char* VanillaPlayerLeaf(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }
    const char* leaf = path;
    if (std::strncmp(leaf, OtrPrefix, std::strlen(OtrPrefix)) == 0) {
        leaf += std::strlen(OtrPrefix);
    }
    if (std::strncmp(leaf, VanillaObjectPrefix, std::strlen(VanillaObjectPrefix)) != 0) {
        return nullptr;
    }
    return leaf + std::strlen("objects/");
}

Gfx* ResolveMirrored(const char* vanillaPath) {
    const char* leaf = VanillaPlayerLeaf(vanillaPath);
    if (leaf == nullptr) {
        return nullptr;
    }
    const std::string path = sModelPath + "/" + leaf;
    if (!ResourceMgr_FileExists(path.c_str())) {
        return nullptr;
    }
    return ResourceMgr_LoadGfxByName(path.c_str());
}

bool IsModelPath(const char* path) {
    return path != nullptr && std::strstr(path, sModelPath.c_str()) != nullptr;
}

void SwapSkeleton(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (sModelPath.empty()) {
        return;
    }
    FlexSkeletonHeader* skeleton = ActiveSkeleton();
    if (skeleton == nullptr) {
        return;
    }
    Player* player = reinterpret_cast<Player*>(actor);
    sSavedSkeleton = player->skelAnime.skeleton;
    sSavedDListCount = player->skelAnime.dListCount;
    player->skelAnime.skeleton = skeleton->sh.segment;
    player->skelAnime.dListCount = skeleton->dListCount;
}

void RestoreSkeleton(Actor* actor, PlayState* play) {
    if (sSavedSkeleton == nullptr) {
        return;
    }
    Player* player = reinterpret_cast<Player*>(actor);
    player->skelAnime.skeleton = sSavedSkeleton;
    player->skelAnime.dListCount = sSavedDListCount;
    sSavedSkeleton = nullptr;
    sSavedDListCount = 0;
}

void ScaleRootLimb(int32_t limbIndex, Vec3f* pos) {
    const float scale = LINK_IS_ADULT ? sRootScaleAdult : sRootScaleChild;

    if (limbIndex != 1 || pos == nullptr) {
        return;
    }
    pos->x *= scale;
    pos->y *= scale;
    pos->z *= scale;
    pos->y -= sRootDrop;
}

bool HideEmptySheath(int32_t limbIndex, Gfx** dList) {
    if (limbIndex != PLAYER_LIMB_SHEATH || CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE ||
        CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE) {
        return false;
    }
    *dList = nullptr;
    return true;
}

void ResolveLimbDraw(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (sModelPath.empty()) {
        return;
    }
    ScaleRootLimb(limbIndex, pos);
    if (HideEmptySheath(limbIndex, dList) || *dList == nullptr) {
        return;
    }
    Gfx* mirrored = ResolveMirrored(reinterpret_cast<const char*>(*dList));
    if (mirrored != nullptr) {
        *dList = mirrored;
        return;
    }
    const bool isUnresolvedEquipment = VanillaPlayerLeaf(reinterpret_cast<const char*>(*dList)) != nullptr;
    if (isUnresolvedEquipment) {
        return;
    }
    if (IsModelPath(reinterpret_cast<const char*>(limbDList))) {
        *dList = limbDList;
    }
}

void MirrorFaceTexture(const char** texture) {
    static std::unordered_set<std::string> sMirroredPaths;
    const char* leaf = VanillaPlayerLeaf(*texture);

    if (leaf == nullptr) {
        return;
    }
    const std::string path = std::string(OtrPrefix) + sModelPath + "/" + leaf;
    if (!ResourceMgr_FileExists(path.c_str())) {
        return;
    }
    *texture = sMirroredPaths.insert(path).first->c_str();
}

void ResolveFaceTextures(const char** eyes, const char** mouth) {
    if (sModelPath.empty()) {
        return;
    }
    MirrorFaceTexture(eyes);
    MirrorFaceTexture(mouth);
}

} // namespace

void FormModel_Init() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(
        VB_APPLY_TUNIC_COLOR, [](GIVanillaBehavior, bool* should, va_list) {
            if (!sModelPath.empty()) {
                *should = false;
            }
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDraw>(ACTOR_PLAYER, SwapSkeleton);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDrawEnd>(ACTOR_PLAYER, RestoreSkeleton);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveLimbDraw>(ResolveLimbDraw);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveFaceTextures>(ResolveFaceTextures);
}

bool FormModel_IsActive(void) {
    return !sModelPath.empty();
}

void FormModel_SetActive(const SOHFormDefinition* form) {
    const char* modelPath = form == nullptr ? nullptr : form->modelPath;
    const bool hasModel = modelPath != nullptr && modelPath[0] != '\0';

    ForceNearLod(hasModel);
    if (!hasModel) {
        sModelPath.clear();
        sRootScaleAdult = 1.0f;
        sRootScaleChild = 1.0f;
        sRootDrop = 0.0f;
        return;
    }
    if (sModelPath != modelPath) {
        sSkeletonAdult = nullptr;
        sSkeletonChild = nullptr;
        sTriedAdult = false;
        sTriedChild = false;
    }
    sModelPath = modelPath;
    sRootScaleAdult = form->rootScaleAdult > 0.0f ? form->rootScaleAdult : 1.0f;
    sRootScaleChild = form->rootScaleChild > 0.0f ? form->rootScaleChild : 1.0f;
    sRootDrop = form->rootDrop;
}
