#include "ResourceManagerHelpers.h"
#include "OTRGlobals.h"
#include "variables.h"
#include "z64.h"
#include "macros.h"
#include "cvar_prefixes.h"
#include "Enhancements/enhancementTypes.h"
#include "Enhancements/randomizer/dungeon.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include <soh/GameVersions.h>
#include "resource/type/SohResourceType.h"
#include "resource/type/Array.h"
#include "resource/type/Skeleton.h"
#include "resource/type/PlayerAnimation.h"
#include <fast/Fast3dWindow.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/DisplayList.h>
#include <libultraship/bridge/resourcebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>

#include <stb_image.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

extern "C" PlayState* gPlayState;

struct LinkTunicDListCacheKey {
    size_t operator()(const std::pair<std::string, const char*>& key) const {
        return std::hash<std::string>{}(key.first) ^ std::hash<const char*>{}(key.second);
    }
};

static const char* ResourceMgr_ResolveLinkTunicDListPath(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }

    const char* originalPath = path;
    constexpr std::string_view adultPrefix = "__OTR__objects/object_link_boy/";
    constexpr std::string_view childPrefix = "__OTR__objects/object_link_child/";

    std::string_view objectPrefix;
    const char* objectFolder;

    if (std::string_view(originalPath).starts_with(adultPrefix)) {
        objectPrefix = adultPrefix;
        objectFolder = "object_link_boy";
    } else if (std::string_view(originalPath).starts_with(childPrefix)) {
        objectPrefix = childPrefix;
        objectFolder = "object_link_child";
    } else {
        return path;
    }

    const char* tunicSuffix = nullptr;
    switch (TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC))) {
        case PLAYER_TUNIC_KOKIRI:
            tunicSuffix = "kokiri";
            break;
        case PLAYER_TUNIC_GORON:
            tunicSuffix = "goron";
            break;
        case PLAYER_TUNIC_ZORA:
            tunicSuffix = "zora";
            break;
        default:
            return path;
    }

    static std::unordered_map<std::pair<std::string, const char*>, std::string, LinkTunicDListCacheKey>
        sResolvedLinkTunicDListPaths;
    std::pair<std::string, const char*> cacheKey{ originalPath, tunicSuffix };
    if (auto it = sResolvedLinkTunicDListPaths.find(cacheKey); it != sResolvedLinkTunicDListPaths.end()) {
        return it->second.c_str();
    }

    const std::string candidate =
        fmt::format("__OTR__objects/{}_{}/{}", objectFolder, tunicSuffix, originalPath + objectPrefix.size());

    if (!ResourceMgr_IsAltAssetsEnabled() || !ResourceMgr_FileAltExists(candidate.c_str()) ||
        !ResourceGetIsCustomByName(candidate.c_str())) {
        return path;
    }

    auto it = sResolvedLinkTunicDListPaths.emplace(std::move(cacheKey), candidate).first;
    return it->second.c_str();
}

extern "C" uint32_t ResourceMgr_GetNumGameVersions() {
    return static_cast<u32>(
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetGameVersions().size());
}

// Helper: returns true if the version is any recognized OOT (NOT mm.o2r / mods).
static bool IsOotVersion(uint32_t version) {
    switch (version) {
        case OOT_NTSC_US_10:
        case OOT_NTSC_US_11:
        case OOT_NTSC_US_12:
        case OOT_NTSC_JP_GC:
        case OOT_NTSC_JP_GC_CE:
        case OOT_NTSC_US_GC:
        case OOT_NTSC_JP_MQ:
        case OOT_NTSC_US_MQ:
        case OOT_PAL_10:
        case OOT_PAL_11:
        case OOT_PAL_GC:
        case OOT_PAL_MQ:
        case OOT_PAL_GC_DBG1:
        case OOT_PAL_GC_DBG2:
        case OOT_PAL_GC_MQ_DBG:
            return true;
        default:
            return false;
    }
}

// Returns the version of the index-th OOT archive, skipping non-OOT (mm.o2r, mods).
extern "C" uint32_t ResourceMgr_GetGameVersion(int index) {
    auto versions = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->GetGameVersions();
    int ootIndex = 0;
    for (uint32_t version : versions) {
        if (IsOotVersion(version)) {
            if (ootIndex == index)
                return version;
            ootIndex++;
        }
    }
    return 0;
}

extern "C" uint32_t ResourceMgr_GetGamePlatform(int index) {
    uint32_t version = ResourceMgr_GetGameVersion(index);

    switch (version) {
        case OOT_NTSC_US_10:
        case OOT_NTSC_US_11:
        case OOT_NTSC_US_12:
        case OOT_PAL_10:
        case OOT_PAL_11:
            return GAME_PLATFORM_N64;
        case OOT_NTSC_JP_GC:
        case OOT_NTSC_JP_GC_CE:
        case OOT_NTSC_US_GC:
        case OOT_PAL_GC:
        case OOT_NTSC_JP_MQ:
        case OOT_NTSC_US_MQ:
        case OOT_PAL_MQ:
        case OOT_PAL_GC_DBG1:
        case OOT_PAL_GC_DBG2:
        case OOT_PAL_GC_MQ_DBG:
            return GAME_PLATFORM_GC;
    }
    // No OOT found at this index — default to N64 for backwards compat
    return GAME_PLATFORM_N64;
}

extern "C" uint32_t ResourceMgr_GetGameRegion(int index) {
    uint32_t version = ResourceMgr_GetGameVersion(index);

    switch (version) {
        case OOT_NTSC_US_10:
        case OOT_NTSC_US_11:
        case OOT_NTSC_US_12:
        case OOT_NTSC_JP_GC:
        case OOT_NTSC_JP_GC_CE:
        case OOT_NTSC_US_GC:
        case OOT_NTSC_JP_MQ:
        case OOT_NTSC_US_MQ:
            return GAME_REGION_NTSC;
        case OOT_PAL_10:
        case OOT_PAL_11:
        case OOT_PAL_GC:
        case OOT_PAL_MQ:
        case OOT_PAL_GC_DBG1:
        case OOT_PAL_GC_DBG2:
        case OOT_PAL_GC_MQ_DBG:
            return GAME_REGION_PAL;
    }
    return GAME_REGION_NTSC;
}

extern "C" char* _message_0xFFFC_nes;
extern "C" bool ResourceMgr_IsPalLoaded() {
    return _message_0xFFFC_nes != NULL;
}

u32 IsSceneMasterQuest(s16 sceneNum) {
    u8 mqMode = CVarGetInteger(CVAR_GENERAL("BetterDebugWarpScreenMQMode"), WARP_MODE_OVERRIDE_OFF);
    if (mqMode == WARP_MODE_OVERRIDE_MQ_AS_VANILLA) {
        return true;
    }

    if (mqMode == WARP_MODE_OVERRIDE_VANILLA_AS_MQ) {
        return false;
    }

    if (OTRGlobals::Instance->HasMasterQuest()) {
        if (!OTRGlobals::Instance->HasOriginal()) {
            return true;
        }

        if (IS_MASTER_QUEST) {
            return true;
        }

        if (IS_RANDO) {
            auto dungeon = OTRGlobals::Instance->gRandoContext->GetDungeons()->GetDungeonFromScene(sceneNum);
            if (dungeon != nullptr && dungeon->IsMQ()) {
                return true;
            }
        }
    }

    return false;
}

extern "C" uint32_t ResourceMgr_GameHasMasterQuest() {
    return OTRGlobals::Instance->HasMasterQuest();
}

extern "C" uint32_t ResourceMgr_GameHasOriginal() {
    return OTRGlobals::Instance->HasOriginal();
}

extern "C" uint32_t ResourceMgr_IsSceneMasterQuest(s16 sceneNum) {
    return IsSceneMasterQuest(sceneNum);
}

extern "C" uint32_t ResourceMgr_IsGameMasterQuest() {
    return gPlayState != NULL ? IsSceneMasterQuest(gPlayState->sceneNum) : 0;
}

extern "C" void ResourceMgr_LoadDirectory(const char* resName) {
    Ship::Context::GetRawInstance()->GetResourceManager()->LoadResources(resName);
}

extern "C" void ResourceMgr_DirtyDirectory(const char* resName) {
    Ship::Context::GetRawInstance()->GetResourceManager()->DirtyResources(resName);
}

extern "C" void ResourceMgr_UnloadResource(const char* resName) {
    std::string path = resName;
    if (path.substr(0, 7) == "__OTR__") {
        path = path.substr(7);
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->UnloadResource(path);
}

// OTRTODO: There is probably a more elegant way to go about this...
// Caller must free each string and the array itself when done.
extern "C" char** ResourceMgr_ListFiles(const char* searchMask, int* resultSize) {
    auto lst = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->ListFiles(searchMask);
    char** result = (char**)malloc(lst->size() * sizeof(char*));

    for (size_t i = 0; i < lst->size(); i++) {
        char* str = (char*)malloc(lst.get()[0][i].size() + 1);
        memcpy(str, lst.get()[0][i].data(), lst.get()[0][i].size());
        str[lst.get()[0][i].size()] = '\0';
        result[i] = str;
    }
    *resultSize = static_cast<int>(lst->size());

    return result;
}

extern "C" uint8_t ResourceMgr_FileExists(const char* filePath) {
    std::string path = filePath;
    if (path.substr(0, 7) == "__OTR__") {
        path = path.substr(7);
    }

    return ExtensionCache.contains(path);
}

extern "C" uint8_t ResourceMgr_FileAltExists(const char* filePath) {
    std::string path = filePath;
    if (path.substr(0, 7) == "__OTR__") {
        path = path.substr(7);
    }

    if (path.substr(0, 4) != "alt/") {
        path = "alt/" + path;
    }

    return ExtensionCache.contains(path);
}

extern "C" bool ResourceMgr_IsAltAssetsEnabled() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->IsAltAssetsEnabled();
}

// Unloads a resource if an alternate version exists when alt assets are enabled
// The resource is only removed from the internal cache to prevent it from used in the next resource lookup
extern "C" void ResourceMgr_UnloadOriginalWhenAltExists(const char* resName) {
    if (ResourceMgr_IsAltAssetsEnabled() && ResourceMgr_FileAltExists((char*)resName)) {
        ResourceMgr_UnloadResource((char*)resName);
    }
}

std::shared_ptr<Ship::IResource> ResourceMgr_GetResourceByNameHandlingMQ(const char* path) {
    std::string Path = path;
    if (ResourceMgr_IsGameMasterQuest()) {
        size_t pos = 0;
        if ((pos = Path.find("/nonmq/", 0)) != std::string::npos) {
            Path.replace(pos, 7, "/mq/");
        }
    }
    return Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(Path.c_str());
}

extern "C" char* ResourceMgr_GetResourceDataByNameHandlingMQ(const char* path) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(path);

    if (res == nullptr) {
        return nullptr;
    }

    return (char*)res->GetRawPointer();
}

extern "C" uint8_t ResourceMgr_TexIsRaw(const char* texPath) {
    auto res = std::static_pointer_cast<Fast::Texture>(ResourceMgr_GetResourceByNameHandlingMQ(texPath));
    return res->Flags & TEX_FLAG_LOAD_AS_RAW;
}

extern "C" uint8_t ResourceMgr_ResourceIsBackground(char* texPath) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(texPath);
    return res->GetInitData()->Type == static_cast<uint32_t>(SOH::ResourceType::SOH_Background);
}

extern "C" char* ResourceMgr_LoadJPEG(char* data, size_t dataSize) {
    static char* finalBuffer = 0;

    if (finalBuffer == 0) {
        finalBuffer = (char*)malloc(dataSize);
    }

    int w;
    int h;
    int comp;

    unsigned char* pixels =
        stbi_load_from_memory((const unsigned char*)data, 320 * 240 * 2, &w, &h, &comp, STBI_rgb_alpha);
    // unsigned char* pixels = stbi_load_from_memory((const unsigned char*)data, 480 * 240 * 2, &w, &h, &comp,
    // STBI_rgb_alpha);
    int idx = 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t* bufferTest = (uint16_t*)finalBuffer;
            int pixelIdx = ((y * w) + x) * 4;

            uint8_t r = pixels[pixelIdx + 0] / 8;
            uint8_t g = pixels[pixelIdx + 1] / 8;
            uint8_t b = pixels[pixelIdx + 2] / 8;

            uint8_t alphaBit = pixels[pixelIdx + 3] != 0;

            uint16_t data = (r << 11) + (g << 6) + (b << 1) + alphaBit;

            finalBuffer[idx++] = (data & 0xFF00) >> 8;
            finalBuffer[idx++] = (data & 0x00FF);
        }
    }

    return (char*)finalBuffer;
}

extern "C" char* ResourceMgr_LoadTexOrDListByName(const char* filePath) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(filePath);

    // Defensive null guard. Mirrors the pattern already used in
    // ResourceMgr_LoadIfDListByName below (line 318). When pak_loader hot-
    // swaps equipment textures during a kaleido draw, the OTR lookup can
    // race-return nullptr while the prior cached pointer is still in flight,
    // causing the next `res->GetInitData()` deref to AV inside
    // gSPInvalidateTexCache / KaleidoScope_DrawEquipment. Returning nullptr
    // lets the caller skip the texture invalidation and keep its current
    // texAddr — the RSP will pick up the new resource on the next draw once
    // the cache settles.
    if (res == nullptr) {
        return nullptr;
    }

    if (res->GetInitData()->Type == static_cast<uint32_t>(Fast::ResourceType::DisplayList)) {
        return (char*)&((std::static_pointer_cast<Fast::DisplayList>(res))->Instructions[0]);
    }

    if (res->GetInitData()->Type == static_cast<uint32_t>(SOH::ResourceType::SOH_Array)) {
        return (char*)(std::static_pointer_cast<SOH::Array>(res))->Vertices.data();
    }

    return (char*)ResourceMgr_GetResourceDataByNameHandlingMQ(filePath);
}

extern "C" char* ResourceMgr_LoadIfDListByName(const char* filePath) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(filePath);

    if (res == nullptr) {
        return nullptr;
    }

    if (res->GetInitData()->Type == static_cast<uint32_t>(Fast::ResourceType::DisplayList)) {
        return (char*)&((std::static_pointer_cast<Fast::DisplayList>(res))->Instructions[0]);
    }

    return nullptr;
}

extern "C" char* ResourceMgr_LoadPlayerAnimByName(const char* animPath) {
    auto anim = std::static_pointer_cast<SOH::PlayerAnimation>(ResourceMgr_GetResourceByNameHandlingMQ(animPath));

    return (char*)&anim->limbRotData[0];
}

// Wrap a raw PlayerAnimation resource (the kind in misc/link_animetion/, which
// is a raw s16 payload with no LinkAnimationHeader struct attached) in a
// LinkAnimationHeader so it can be passed to Player_AnimPlayLoop/PlayOnce.
// Mirrors the animation viewer's wrapping logic at animationViewer.cpp:131-138.
// Cached by path so repeated calls return the same pointer.
extern "C" LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath) {
    if (animPath == nullptr)
        return nullptr;
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(animPath);
    if (res == nullptr)
        return nullptr;
    if (res->GetInitData()->Type != static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation)) {
        return nullptr;
    }
    auto playerAnim = std::static_pointer_cast<SOH::PlayerAnimation>(res);

    constexpr size_t kS16PerFrame = 67; // matches animationViewer.cpp::PLAYER_ANIM_S16_PER_FRAME

    static std::map<std::string, LinkAnimationHeader> sPlayerAnimWrappers;
    LinkAnimationHeader& wrapper = sPlayerAnimWrappers[animPath];
    size_t totalS16 = playerAnim->GetPointerSize() / sizeof(int16_t);
    wrapper.common.frameCount = (s16)(totalS16 / kS16PerFrame);
    wrapper.segment = (void*)playerAnim->GetPointer();
    return &wrapper;
}

// Same as above, but the returned clip is IN PLACE: every frame keeps frame 0's
// root translation, so the animation no longer walks the body across the floor.
//
// Needed because the MHR dual-blade clips carry huge baked root motion (a run
// cycle nets ~20000 units, a dash ~22000). That is harmless while a form drives
// the clip by hand, but the moment such a clip is installed into OOT's own
// animation tables, OOT integrates the root delta through
// Player_StartAnimMovement/AnimationContext_SetMoveActor ON TOP of linearVelocity
// and the player double-moves — Link skates forward while running in place.
// Stripping the delta (not the offset — zeroing it would yank the body to the
// skeleton origin) leaves the pose intact and hands all travel back to OOT.
//
// stripY additionally pins the vertical root, which is what makes an otherwise
// grounded clip float or sink once OOT owns floor height.
// firstFrame/lastFrame (inclusive, -1 = the clip's own bounds) cut a SUB-RANGE out
// before the resample, so one packed clip can serve several engine slots. That is
// how Gerudo's guard works: DemonModeActivationFlourish is a single 46-frame
// flourish and OOT wants three separate animations for a shield (defense,
// defense_wait, defense_end), so the same file is sliced 1-20 / 21-30 / 31-45.
//
// Combined with targetFrames this is also the playback-speed knob: OOT plays these
// slots at a fixed 1.0, so a 20-frame range asked for as 10 frames simply runs at
// double speed. Doing it here instead of at the LinkAnimation_Change callsite keeps
// every engine path (raise, loop, release, and the interrupt path in
// Player_Action_808435C4) at the same speed without touching any of them.
extern "C" LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(const char* animPath, uint8_t stripY,
                                                                               int16_t firstFrame, int16_t lastFrame,
                                                                               int16_t targetFrames) {
    if (animPath == nullptr)
        return nullptr;
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(animPath);
    if (res == nullptr)
        return nullptr;
    if (res->GetInitData()->Type != static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation)) {
        return nullptr;
    }
    auto playerAnim = std::static_pointer_cast<SOH::PlayerAnimation>(res);

    constexpr size_t kS16PerFrame = 67; // 3 root translation + 64 limb rotation values

    // Keyed by path AND flag: the same clip can legitimately be wanted both with
    // and without its vertical root.
    struct InPlaceAnim {
        LinkAnimationHeader header;
        std::vector<int16_t> data;
    };
    static std::map<std::string, InPlaceAnim> sInPlaceAnims;

    const std::string key = std::string(animPath) + (stripY ? "#xyz" : "#xz") + "#" + std::to_string(targetFrames) +
                            "#" + std::to_string(firstFrame) + "-" + std::to_string(lastFrame);
    auto it = sInPlaceAnims.find(key);
    if (it != sInPlaceAnims.end()) {
        return &it->second.header;
    }

    const size_t totalS16 = playerAnim->GetPointerSize() / sizeof(int16_t);
    const size_t clipFrames = totalS16 / kS16PerFrame;
    if (clipFrames == 0)
        return nullptr;

    // Clamp the requested range into the clip. A range that lands entirely past the
    // end collapses to the last frame rather than returning null, so a mis-typed
    // window shows a frozen pose instead of silently reverting the slot to vanilla.
    size_t rangeBegin = (firstFrame > 0) ? (size_t)firstFrame : 0;
    if (rangeBegin >= clipFrames)
        rangeBegin = clipFrames - 1;
    size_t rangeEnd = ((lastFrame >= 0) && ((size_t)lastFrame < clipFrames)) ? (size_t)lastFrame : (clipFrames - 1);
    if (rangeEnd < rangeBegin)
        rangeEnd = rangeBegin;
    const size_t frameCount = rangeEnd - rangeBegin + 1;

    InPlaceAnim& entry = sInPlaceAnims[key];
    const int16_t* src = (const int16_t*)playerAnim->GetPointer() + rangeBegin * kS16PerFrame;

    // Resample to a fixed length when asked. OOT's locomotion is not a plain
    // playback: Player_Action_80840DE4 hard-sets animLength to 29 and the walk/run
    // blend rigs sample both clips at fixed frame RATIOS (16/29). Feed them clips
    // of 31 and 39 frames and the two are sampled out of phase with each other,
    // which is what throws a limb to a completely wrong angle mid-stride.
    // Nearest-frame resampling on purpose: these are packed s16 angles, and
    // interpolating them would smear any value that crosses the +-180 wrap.
    const size_t outFrames = (targetFrames > 0) ? (size_t)targetFrames : frameCount;
    entry.data.resize(outFrames * kS16PerFrame);
    for (size_t f = 0; f < outFrames; ++f) {
        size_t srcFrame = (outFrames == frameCount) ? f : (f * frameCount) / outFrames;
        if (srcFrame >= frameCount)
            srcFrame = frameCount - 1;
        std::copy(src + srcFrame * kS16PerFrame, src + (srcFrame + 1) * kS16PerFrame,
                  entry.data.begin() + f * kS16PerFrame);
    }

    const int16_t baseX = entry.data[0];
    const int16_t baseY = entry.data[1];
    const int16_t baseZ = entry.data[2];
    for (size_t f = 0; f < outFrames; ++f) {
        int16_t* frame = &entry.data[f * kS16PerFrame];
        frame[0] = baseX;
        frame[2] = baseZ;
        if (stripY) {
            frame[1] = baseY;
        }
    }

    entry.header.common.frameCount = (s16)outFrames;
    entry.header.segment = (void*)entry.data.data();
    return &entry.header;
}

extern "C" LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(const char* animPath, uint8_t stripY,
                                                                                   int16_t targetFrames) {
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(animPath, stripY, -1, -1, targetFrames);
}

extern "C" LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlace(const char* animPath, uint8_t stripY) {
    return ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(animPath, stripY, -1, -1, 0);
}

extern "C" void ResourceMgr_PushCurrentDirectory(char* path) {
    Fast::gfx_push_current_dir(path);
}

extern "C" Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    path = ResourceMgr_ResolveLinkTunicDListPath(path);
    // When an alt resource exists for the DL, we need to unload the original asset
    // to clear the cache so the alt asset will be loaded instead
    // OTRTODO: If Alt loading over original cache is fixed, this line can most likely be removed
    ResourceMgr_UnloadOriginalWhenAltExists(path);

    auto res = std::static_pointer_cast<Fast::DisplayList>(ResourceMgr_GetResourceByNameHandlingMQ(path));
    if (!res)
        return nullptr;
    return (Gfx*)&res->Instructions[0];
}

extern "C" uint8_t ResourceMgr_FileIsCustomByName(const char* path) {
    auto res = std::static_pointer_cast<Fast::DisplayList>(ResourceMgr_GetResourceByNameHandlingMQ(path));
    return res->GetInitData()->IsCustom;
}

typedef struct {
    int index;
    Gfx instruction;
    const void* instructionsPtr;
    size_t instructionCount;
    bool isCustom;
} GfxPatch;

std::unordered_map<std::string, std::unordered_map<std::string, GfxPatch>> originalGfx;

// Attention! This is primarily for cosmetics & bug fixes. For things like mods and model replacement you should be
// using OTRs instead (When that is available). Index can be found using the commented out section below.
extern "C" void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction) {
    auto res = std::static_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));

    if (res == nullptr || static_cast<size_t>(index) >= res->Instructions.size()) {
        return;
    }

    // Leaving this here for people attempting to find the correct Dlist index to patch
    /*if (strcmp("__OTR__objects/object_gi_longsword/gGiBiggoronSwordDL", path) == 0) {
        for (int i = 0; i < res->instructions.size(); i++) {
            Gfx* gfx = (Gfx*)&res->instructions[i];
            // Log all commands
            // SPDLOG_INFO("index:{} command:{}", i, gfx->words.w0 >> 24);
            // Log only SetPrimColors
            if (gfx->words.w0 >> 24 == 250) {
                SPDLOG_INFO("index:{} r:{} g:{} b:{} a:{}", i, _SHIFTR(gfx->words.w1, 24, 8), _SHIFTR(gfx->words.w1, 16,
    8), _SHIFTR(gfx->words.w1, 8, 8), _SHIFTR(gfx->words.w1, 0, 8));
            }
        }
    }*/

    // Index refers to individual gfx words, which are half the size on 32-bit
    // if (sizeof(uintptr_t) < 8) {
    // index /= 2;
    // }

    // Do not patch custom assets as they most likely do not have the same instructions as authentic assets
    if (res->GetInitData()->IsCustom) {
        return;
    }

    Gfx* gfx = (Gfx*)&res->Instructions[index];

    if (!originalGfx.contains(path) || !originalGfx[path].contains(patchName)) {
        originalGfx[path][patchName] = { index, *gfx, res->Instructions.data(), res->Instructions.size(),
                                         res->GetInitData()->IsCustom };
    }

    *gfx = instruction;
}

extern "C" void ResourceMgr_PatchGfxCopyCommandByName(const char* path, const char* patchName, int destinationIndex,
                                                      int sourceIndex) {
    auto res = std::static_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));

    if (res == nullptr || static_cast<size_t>(destinationIndex) >= res->Instructions.size() ||
        static_cast<size_t>(sourceIndex) >= res->Instructions.size()) {
        return;
    }

    // Do not patch custom assets as they most likely do not have the same instructions as authentic assets
    if (res->GetInitData()->IsCustom) {
        return;
    }

    Gfx* destinationGfx = (Gfx*)&res->Instructions[destinationIndex];
    Gfx sourceGfx = *(Gfx*)&res->Instructions[sourceIndex];

    if (!originalGfx.contains(path) || !originalGfx[path].contains(patchName)) {
        originalGfx[path][patchName] = { destinationIndex, *destinationGfx, res->Instructions.data(),
                                         res->Instructions.size(), res->GetInitData()->IsCustom };
    }

    *destinationGfx = sourceGfx;
}

extern "C" void ResourceMgr_PatchCustomGfxByName(const char* path, const char* patchName, int index, Gfx instruction) {
    auto res = std::static_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));

    if (res == nullptr || static_cast<size_t>(index) >= res->Instructions.size()) {
        return;
    }

    Gfx* gfx = (Gfx*)&res->Instructions[index];

    if (!originalGfx.contains(path) || !originalGfx[path].contains(patchName)) {
        originalGfx[path][patchName] = { index, *gfx, res->Instructions.data(), res->Instructions.size(),
                                         res->GetInitData()->IsCustom };
    }

    *gfx = instruction;
}

extern "C" void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName) {
    if (originalGfx.contains(path) && originalGfx[path].contains(patchName)) {
        auto res = std::static_pointer_cast<Fast::DisplayList>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));

        // If the resource is unavailable (e.g. swapped out when toggling alt assets), clean up the record and bail.
        if (res == nullptr) {
            ResourceMgr_UnloadResource(path);
            originalGfx[path].erase(patchName);
            return;
        }

        const GfxPatch& patch = originalGfx[path][patchName];
        // Skip and clean up if the backing resource changed since we recorded the patch (e.g. alt<->vanilla swap)
        // to avoid writing instructions from a different asset onto the current one.
        if (res->Instructions.data() != patch.instructionsPtr || res->Instructions.size() != patch.instructionCount ||
            res->GetInitData()->IsCustom != patch.isCustom) {
            ResourceMgr_UnloadResource(path);
            originalGfx[path].erase(patchName);
            return;
        }

        // Skip and clean up if the loaded resource is smaller than the recorded patch index (can happen when alt assets
        // swap in shorter display lists).
        if (static_cast<size_t>(patch.index) >= res->Instructions.size()) {
            originalGfx[path].erase(patchName);
            return;
        }

        Gfx* gfx = (Gfx*)&res->Instructions[patch.index];
        *gfx = patch.instruction;

        originalGfx[path].erase(patchName);
    }
}

extern "C" char* ResourceMgr_LoadArrayByName(const char* path) {
    auto res = std::static_pointer_cast<SOH::Array>(ResourceMgr_GetResourceByNameHandlingMQ(path));

    return (char*)res->Scalars.data();
}

// Return of LoadArrayByNameAsVec3s must be freed by the caller
extern "C" char* ResourceMgr_LoadArrayByNameAsVec3s(const char* path) {
    auto res = std::static_pointer_cast<SOH::Array>(ResourceMgr_GetResourceByNameHandlingMQ(path));

    // if (res->CachedGameAsset != nullptr)
    //     return (char*)res->CachedGameAsset;
    // else
    // {
    Vec3s* data = (Vec3s*)malloc(sizeof(Vec3s) * res->Scalars.size());

    for (size_t i = 0; i < res->Scalars.size(); i += 3) {
        data[(i / 3)].x = res->Scalars[i + 0].s16;
        data[(i / 3)].y = res->Scalars[i + 1].s16;
        data[(i / 3)].z = res->Scalars[i + 2].s16;
    }

    // res->CachedGameAsset = data;

    return (char*)data;
    // }
}

extern "C" CollisionHeader* ResourceMgr_LoadColByName(const char* path) {
    return (CollisionHeader*)ResourceGetDataByName(path);
}

extern "C" Vtx* ResourceMgr_LoadVtxByName(char* path) {
    return (Vtx*)ResourceGetDataByName(path);
}

extern "C" SequenceData ResourceMgr_LoadSeqByName(const char* path) {
    SequenceData* sequence = (SequenceData*)ResourceGetDataByName(path);
    return *sequence;
}

extern "C" SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path) {
    SequenceData* sequence = (SequenceData*)ResourceGetDataByName(path);
    return sequence;
}

extern "C" SoundFontSample* ResourceMgr_LoadAudioSample(const char* path) {
    return (SoundFontSample*)ResourceGetDataByName(path);
}

extern "C" SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path) {
    return (SoundFont*)ResourceGetDataByName(path);
}

extern "C" int ResourceMgr_OTRSigCheck(char* imgData) {
    uintptr_t i = (uintptr_t)(imgData);

    // if (i == 0xD9000000 || i == 0xE7000000 || (i & 1) == 1)
    if ((i & 1) == 1)
        return 0;

    // if ((i & 0xFF000000) != 0xAB000000 && (i & 0xFF000000) != 0xCD000000 && i != 0) {
    if (i != 0) {
        if (imgData[0] == '_' && imgData[1] == '_' && imgData[2] == 'O' && imgData[3] == 'T' && imgData[4] == 'R' &&
            imgData[5] == '_' && imgData[6] == '_') {
            return 1;
        }
    }

    return 0;
}

// Load animation with explicit alt asset path checking.
// When Alt Assets is OFF: use original path directly (O2R or vanilla)
// When Alt Assets is ON: try alt/ prefix first, fall back to regular path if not found or invalid
extern "C" AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path) {
    bool isAlt = ResourceMgr_IsAltAssetsEnabled();

    if (isAlt) {
        if (ResourceMgr_FileAltExists(path)) {
            std::string pathStr = std::string(path);
            static const std::string sOtr = "__OTR__";

            if (pathStr.starts_with(sOtr)) {
                pathStr = pathStr.substr(sOtr.length());
            }

            // Try alt/ first
            pathStr = Ship::IResource::gAltAssetPrefix + pathStr;

            AnimationHeaderCommon* animHeader = (AnimationHeaderCommon*)ResourceGetDataByName(pathStr.c_str());

            // If alt loaded successfully, verify it has valid data
            if (animHeader != NULL) {
                // Check for valid frame count (> 0)
                if (animHeader->frameCount > 0) {
                    // For Normal animations: check frameData (comes after frameCount in AnimationHeader)
                    // For Link animations: check segment (comes after frameCount in LinkAnimationHeader)
                    // We check both to be safe - if either is valid, the animation is usable
                    AnimationHeader* normalAnim = (AnimationHeader*)animHeader;
                    LinkAnimationHeader* linkAnim = (LinkAnimationHeader*)animHeader;

                    // Valid if Normal animation has frameData OR Link animation has segment
                    if (normalAnim->frameData != NULL || linkAnim->segment != NULL) {
                        return animHeader;
                    }
                }
                // Alt loaded but is invalid (broken), fall through to original path
            }
        }

        // Fall back to original path
        return (AnimationHeaderCommon*)ResourceGetDataByName(path);
    }

    // Alt OFF: use original path directly
    return (AnimationHeaderCommon*)ResourceGetDataByName(path);
}

extern "C" SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime) {
    std::string pathStr = std::string(path);
    static const std::string sOtr = "__OTR__";

    if (pathStr.starts_with(sOtr)) {
        pathStr = pathStr.substr(sOtr.length());
    }

    bool isAlt = ResourceMgr_IsAltAssetsEnabled();

    if (isAlt) {
        pathStr = Ship::IResource::gAltAssetPrefix + pathStr;
    }

    SkeletonHeader* skelHeader = (SkeletonHeader*)ResourceGetDataByName(pathStr.c_str());

    // If there isn't an alternate model, load the regular one
    if (isAlt && skelHeader == NULL) {
        skelHeader = (SkeletonHeader*)ResourceGetDataByName(path);
    }

    // This function is only called when a skeleton is initialized.
    // Therefore we can take this opportunity to take note of the Skeleton that is created...
    if (skelAnime != nullptr) {
        auto stringPath = std::string(path);
        SOH::SkeletonPatcher::RegisterSkeleton(stringPath, skelAnime);
    }

    return skelHeader;
}

extern "C" void ResourceMgr_UnregisterSkeleton(SkelAnime* skelAnime) {
    if (skelAnime != nullptr) {
        SOH::SkeletonPatcher::UnregisterSkeleton(skelAnime);
    }
}

extern "C" void ResourceMgr_ClearSkeletons() {
    SOH::SkeletonPatcher::ClearSkeletons();
}

extern "C" s32* ResourceMgr_LoadCSByName(const char* path) {
    return (s32*)ResourceMgr_GetResourceDataByNameHandlingMQ(path);
}
