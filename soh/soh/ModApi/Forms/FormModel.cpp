#include "FormModel.h"

#include <cstring>
#include <string>
#include <unordered_map>
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

struct FormBody {
    std::string modelPath;
    float rootScaleAdult = 1.0f;
    float rootScaleChild = 1.0f;
    float rootDrop = 0.0f;
    FlexSkeletonHeader* skeletonAdult = nullptr;
    FlexSkeletonHeader* skeletonChild = nullptr;
    bool triedAdult = false;
    bool triedChild = false;
};

struct DrawnPlayer {
    Player* player = nullptr;
    FormBody* body = nullptr;
    const SOHFormDefinition* otherPlayerForm = nullptr;
    bool isChild = false;
    bool isLocal = false;
};

std::unordered_map<std::string, FormBody> sBodies;
FormBody* sLocalBody = nullptr;
DrawnPlayer sDrawn;
bool sIsDrawingPlayerActor = false;
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

FormBody* FindBody(const SOHFormDefinition* form) {
    const char* modelPath = form == nullptr ? nullptr : form->modelPath;
    if (modelPath == nullptr || modelPath[0] == '\0') {
        return nullptr;
    }
    FormBody& body = sBodies[modelPath];
    body.modelPath = modelPath;
    body.rootScaleAdult = form->rootScaleAdult > 0.0f ? form->rootScaleAdult : 1.0f;
    body.rootScaleChild = form->rootScaleChild > 0.0f ? form->rootScaleChild : 1.0f;
    body.rootDrop = form->rootDrop;
    return &body;
}

// TAKEOVER forms draw through the mod's own callback, which only knows the local player: others stay Link.
const SOHFormDefinition* FindOtherPlayerForm(Player* player) {
    const char* formKey = nullptr;
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPlayerResolveForm>(player, &formKey);
    const SOHFormDefinition* form = formKey == nullptr ? nullptr : FormRegistry_Find(formKey);
    return form != nullptr && form->kind == SOH_FORM_KIND_LINK ? form : nullptr;
}

bool IsLinkSkeleton(SkeletonHeader* header) {
    return header != nullptr && header->limbCount > 0 && header->limbCount <= 32 && header->segment != nullptr;
}

// Another player's dummy swaps gSaveContext.linkAge only inside its own draw, so read the age off its skeleton.
bool HasChildSkeleton(Player* player) {
    SkeletonHeader* child =
        ResourceMgr_LoadSkeletonByName(reinterpret_cast<const char*>(gPlayerSkelHeaders[LINK_AGE_CHILD]), nullptr);
    return child != nullptr && player->skelAnime.skeleton == child->segment;
}

FlexSkeletonHeader* LoadSkeleton(const FormBody& body, const char* suffix) {
    const std::string path = body.modelPath + suffix;
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName(path.c_str(), nullptr);
    if (!IsLinkSkeleton(header)) {
        SPDLOG_WARN("[FormModel] '{}' is not a Link skeleton; keeping Link's body", path);
        return nullptr;
    }
    return reinterpret_cast<FlexSkeletonHeader*>(header);
}

FlexSkeletonHeader* SkeletonFor(FormBody& body, bool isChild) {
    if (isChild) {
        if (!body.triedChild) {
            body.triedChild = true;
            body.skeletonChild = LoadSkeleton(body, ChildSkeleton);
        }
        if (body.skeletonChild != nullptr) {
            return body.skeletonChild;
        }
    }
    if (!body.triedAdult) {
        body.triedAdult = true;
        body.skeletonAdult = LoadSkeleton(body, AdultSkeleton);
    }
    return body.skeletonAdult;
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

Gfx* ResolveMirrored(const FormBody& body, const char* vanillaPath) {
    const char* leaf = VanillaPlayerLeaf(vanillaPath);
    if (leaf == nullptr) {
        return nullptr;
    }
    const std::string path = body.modelPath + "/" + leaf;
    if (!ResourceMgr_FileExists(path.c_str())) {
        return nullptr;
    }
    return ResourceMgr_LoadGfxByName(path.c_str());
}

bool IsModelPath(const FormBody& body, const char* path) {
    return path != nullptr && std::strstr(path, body.modelPath.c_str()) != nullptr;
}

// The pause menu draws Link outside any actor draw; only the local player is drawn there.
DrawnPlayer DrawnPlayerFor(Player* player) {
    if (sIsDrawingPlayerActor) {
        return sDrawn;
    }
    const bool isLocal = gPlayState != nullptr && player == GET_PLAYER(gPlayState);
    return DrawnPlayer{ player, isLocal ? sLocalBody : nullptr, nullptr, !LINK_IS_ADULT, isLocal };
}

const FormBody* DrawnBody() {
    return sIsDrawingPlayerActor ? sDrawn.body : sLocalBody;
}

void SwapSkeleton(Actor* actor, PlayState* play, bool* drawVanilla) {
    Player* player = reinterpret_cast<Player*>(actor);
    const bool isLocal = player == GET_PLAYER(play);
    const SOHFormDefinition* otherPlayerForm = isLocal ? nullptr : FindOtherPlayerForm(player);
    sIsDrawingPlayerActor = true;
    sDrawn = DrawnPlayer{ player, isLocal ? sLocalBody : FindBody(otherPlayerForm), otherPlayerForm, false, isLocal };
    if (sDrawn.body == nullptr) {
        return;
    }
    sDrawn.isChild = isLocal ? !LINK_IS_ADULT : HasChildSkeleton(player);
    FlexSkeletonHeader* skeleton = SkeletonFor(*sDrawn.body, sDrawn.isChild);
    if (skeleton == nullptr) {
        return;
    }
    sSavedSkeleton = player->skelAnime.skeleton;
    sSavedDListCount = player->skelAnime.dListCount;
    player->skelAnime.skeleton = skeleton->sh.segment;
    player->skelAnime.dListCount = skeleton->dListCount;
}

void RestoreSkeleton(Actor* actor, PlayState* play) {
    sIsDrawingPlayerActor = false;
    sDrawn = DrawnPlayer{};
    if (sSavedSkeleton == nullptr) {
        return;
    }
    Player* player = reinterpret_cast<Player*>(actor);
    player->skelAnime.skeleton = sSavedSkeleton;
    player->skelAnime.dListCount = sSavedDListCount;
    sSavedSkeleton = nullptr;
    sSavedDListCount = 0;
}

void ScaleRootLimb(const DrawnPlayer& drawn, int32_t limbIndex, Vec3f* pos) {
    const float scale = drawn.isChild ? drawn.body->rootScaleChild : drawn.body->rootScaleAdult;

    if (limbIndex != 1 || pos == nullptr) {
        return;
    }
    pos->x *= scale;
    pos->y *= scale;
    pos->z *= scale;
    pos->y -= drawn.body->rootDrop;
}

bool HideEmptySheath(const DrawnPlayer& drawn, int32_t limbIndex, Gfx** dList) {
    if (!drawn.isLocal || limbIndex != PLAYER_LIMB_SHEATH ||
        CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE ||
        CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE) {
        return false;
    }
    *dList = nullptr;
    return true;
}

void ResolveLimbDraw(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    const DrawnPlayer drawn = DrawnPlayerFor(player);
    if (drawn.body == nullptr) {
        return;
    }
    ScaleRootLimb(drawn, limbIndex, pos);
    if (HideEmptySheath(drawn, limbIndex, dList) || *dList == nullptr) {
        return;
    }
    Gfx* mirrored = ResolveMirrored(*drawn.body, reinterpret_cast<const char*>(*dList));
    if (mirrored != nullptr) {
        *dList = mirrored;
        return;
    }
    const bool isUnresolvedEquipment = VanillaPlayerLeaf(reinterpret_cast<const char*>(*dList)) != nullptr;
    if (isUnresolvedEquipment) {
        return;
    }
    if (IsModelPath(*drawn.body, reinterpret_cast<const char*>(limbDList))) {
        *dList = limbDList;
    }
}

void MirrorFaceTexture(const FormBody& body, const char** texture) {
    static std::unordered_set<std::string> sMirroredPaths;
    const char* leaf = VanillaPlayerLeaf(*texture);

    if (leaf == nullptr) {
        return;
    }
    const std::string path = std::string(OtrPrefix) + body.modelPath + "/" + leaf;
    if (!ResourceMgr_FileExists(path.c_str())) {
        return;
    }
    *texture = sMirroredPaths.insert(path).first->c_str();
}

void ResolveFaceTextures(const char** eyes, const char** mouth) {
    const FormBody* body = DrawnBody();
    if (body == nullptr) {
        return;
    }
    MirrorFaceTexture(*body, eyes);
    MirrorFaceTexture(*body, mouth);
}

} // namespace

void FormModel_Init() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnVanillaBehavior>(
        VB_APPLY_TUNIC_COLOR, [](GIVanillaBehavior, bool* should, va_list) {
            if (DrawnBody() != nullptr) {
                *should = false;
            }
        });
    for (int16_t actorId : { ACTOR_PLAYER, ACTOR_EN_OE2 }) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDraw>(actorId, SwapSkeleton);
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorDrawEnd>(actorId, RestoreSkeleton);
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveLimbDraw>(ResolveLimbDraw);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerResolveFaceTextures>(ResolveFaceTextures);
}

bool FormModel_IsActive(void) {
    return sLocalBody != nullptr;
}

bool FormModel_IsDrawingOtherPlayer(void) {
    return sIsDrawingPlayerActor && !sDrawn.isLocal;
}

Player* FormModel_GetDrawnOtherPlayer(void) {
    return FormModel_IsDrawingOtherPlayer() ? sDrawn.player : nullptr;
}

const char* FormModel_GetDrawnOtherPlayerFormKey(void) {
    return FormModel_IsDrawingOtherPlayer() && sDrawn.otherPlayerForm != nullptr ? sDrawn.otherPlayerForm->key
                                                                                    : nullptr;
}

void FormModel_SetActive(const SOHFormDefinition* form) {
    sLocalBody = FindBody(form);
    ForceNearLod(sLocalBody != nullptr);
}
