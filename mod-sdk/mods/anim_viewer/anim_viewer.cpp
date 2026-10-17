#include "clip.h"
#include <algorithm>
#include <map>
#include <set>
#include <cstdio>
#include <cstring>
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "regs.h"
const char* CVarGetString(const char*, const char*);
void CVarSetString(const char*, const char*);
void Player_SetupActionPreserveItemAction(PlayState*, Player*, PlayerActionFunc, s32);
void Player_UseItem(PlayState*, Player*, s32);
void func_80839FFC(Player*, PlayState*);
}
#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "soh/ResourceManagerHelpers.h"

static const SOHModApi* sApi;
struct Entry { std::string name, raw; int frames, archive; };
static std::vector<Entry> sClips;
static std::vector<size_t> sVisible;
static std::vector<int16_t> sData;
static std::map<std::string, viewer::ClipInfo> sFound;
static std::set<std::string> sMarks;
static std::string sStatus = "Refresh to list animations from mounted archives.", sFilter;
static int32_t sSelected = 0, sSpeed = 100, sArchive = -1;
static bool sRequested, sActive, sPaused, sLoop = true, sRootLock = true, sScanned;
static bool sFullScan;
static float sFrame, sGravity;
static uint8_t sMovementFlags;
static Vec3f sOrigin;
static Player* sPlayer;
static std::string sLoaded;

static std::string Lower(std::string value) {
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
static bool Visit(const char* path, const uint8_t* bytes, uint64_t size, void*) {
    std::string name(path), lower = Lower(name);
    // Other forms require different skeletons, not the human Link joint layout.
    if (lower.find("link_goron") != std::string::npos || lower.find("link_zora") != std::string::npos ||
        lower.find("link_deku") != std::string::npos || lower.find("link_nuts") != std::string::npos) return true;
    auto info = viewer::Inspect(bytes, size_t(size));
    if (info.frames) sFound[name] = info;
    return true;
}
static void Filter() {
    sVisible.clear();
    std::string filter = Lower(CVarGetString("gMods.AnimViewer.Filter", ""));
    sFilter = filter;
    for (size_t i = 0; i < sClips.size(); ++i)
        if ((sArchive < 0 || sClips[i].archive == sArchive) &&
            (filter.empty() || Lower(sClips[i].name).find(filter) != std::string::npos)) sVisible.push_back(i);
    sSelected = std::max(0, std::min(sSelected, int32_t(sVisible.size()) - 1));
}
static void Refresh() {
    sFound.clear(); sClips.clear();
    if (sFullScan) sApi->ReadArchiveFiles("*", Visit, nullptr);
    else {
        sApi->ReadArchiveFiles("*Anim*", Visit, nullptr);
        sApi->ReadArchiveFiles("*anim*", Visit, nullptr);
    }
    std::set<std::string> targets;
    for (const auto& pair : sFound) if (!pair.second.packed && !pair.second.target.empty()) targets.insert(pair.second.target);
    for (const auto& path : targets) if (!sFound.count(path)) sApi->ReadArchiveFiles(path.c_str(), Visit, nullptr);
    for (const auto& pair : sFound) {
        const auto& info = pair.second;
        if (info.packed) {
            if (!targets.count(pair.first)) sClips.push_back({pair.first, pair.first, info.frames,
                                                          ResourceMgr_GetFileArchiveIndex(pair.first.c_str())});
        } else {
            auto data = sFound.find(info.target);
            if (data != sFound.end() && data->second.packed && data->second.frames >= info.frames)
                sClips.push_back({pair.first, info.target, info.frames, ResourceMgr_GetFileArchiveIndex(info.target.c_str())});
        }
    }
    sScanned = true; Filter();
    sStatus = std::to_string(sClips.size()) + " compatible clips ready.";
}
static const Entry* Selected() {
    return sSelected >= 0 && size_t(sSelected) < sVisible.size() ? &sClips[sVisible[sSelected]] : nullptr;
}
static bool LoadBytes(const char*, const uint8_t* bytes, uint64_t size, void* output) {
    viewer::Decode(bytes, size_t(size), *static_cast<std::vector<int16_t>*>(output));
    return false;
}
static bool Load() {
    const Entry* clip = Selected();
    if (!clip) { sStatus = "No compatible clip selected."; return false; }
    std::vector<int16_t> data;
    sApi->ReadArchiveFiles(clip->raw.c_str(), LoadBytes, &data);
    if (data.size() < size_t(clip->frames) * viewer::Stride) {
        sStatus = "Cannot decode this clip safely."; return false;
    }
    data.resize(size_t(clip->frames) * viewer::Stride);
    sData.swap(data); sLoaded = clip->name; sFrame = 0;
    sStatus = "Loaded: " + sLoaded;
    return true;
}
static void HoldPose(Player* player, PlayState*) {
    player->actor.world.pos = sOrigin;
    player->actor.prevPos = sOrigin;
    player->actor.speedXZ = 0;
    player->actor.velocity = {0, 0, 0};
    player->actor.gravity = 0;
    player->skelAnime.movementFlags = 0;
    player->meleeWeaponState = 0;
}
static void Stop(bool restore) {
    if (restore && sActive && gPlayState && sPlayer == GET_PLAYER(gPlayState)) {
        sPlayer->actor.gravity = sGravity;
        if (sPlayer->actionFunc == HoldPose) {
            sPlayer->skelAnime.movementFlags = sMovementFlags;
            func_80839FFC(sPlayer, gPlayState);
        }
    }
    sActive = false; sRequested = false; sPlayer = nullptr;
    sApi->ReleasePlayerInput("nei.animviewer");
}
static void Tick() {
    if (!gPlayState) { if (sActive) Stop(false); return; }
    Player* player = GET_PLAYER(gPlayState);
    if (!player) return;
    if (sActive && (player != sPlayer || player->actionFunc != HoldPose || gSaveContext.health <= 0)) {
        Stop(true); sStatus = "Preview stopped: Link changed action.";
    }
    if (!sRequested) { if (sActive) Stop(true); return; }
    if (!sActive) {
        if (sData.empty() && !Load()) { sRequested = false; return; }
        if (player->skelAnime.limbCount != PLAYER_LIMB_MAX || !(player->actor.bgCheckFlags & 1) ||
            player->heldActor || player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM) ||
            gPlayState->csCtx.state != 0 || gSaveContext.health <= 0) {
            sRequested = false; sStatus = "Start on safe ground, outside dialogue/cutscenes, without carrying an actor."; return;
        }
        if (!sApi->BlockPlayerInput("nei.animviewer", uint16_t(0xffff & ~BTN_START), true)) {
            sRequested = false; sStatus = "Unable to reserve preview input."; return;
        }
        Player_UseItem(gPlayState, player, ITEM_NONE);
        sPlayer = player; sGravity = player->actor.gravity; sOrigin = player->actor.world.pos;
        sMovementFlags = player->skelAnime.movementFlags;
        Player_SetupActionPreserveItemAction(gPlayState, player, HoldPose, 0);
        if (player->actionFunc != HoldPose) {
            sApi->ReleasePlayerInput("nei.animviewer"); sRequested = false; sPlayer = nullptr;
            sStatus = "Link is busy. Put away the item and try again."; return;
        }
        sActive = true;
        HoldPose(player, gPlayState);
    }
    if (!sPaused) {
        int count = int(sData.size() / viewer::Stride);
        sFrame = viewer::Advance(sFrame, sSpeed * 0.01f * R_UPDATE_RATE / 3.0f, count, sLoop);
        if (!sLoop && ((sSpeed >= 0 && sFrame >= count - 1) || (sSpeed < 0 && sFrame <= 0))) sPaused = true;
    }
}
static void Draw(Actor* actor, PlayState*, bool*) {
    if (!sActive || actor != reinterpret_cast<Actor*>(sPlayer) || sData.empty()) return;
    size_t frame = std::min(size_t(std::max(0.0f, sFrame)), sData.size() / viewer::Stride - 1);
    // Native Link frames include 22 Vec3s and the appearance word in the padded joint buffer.
    static_assert(sizeof(sPlayer->jointTable) >= viewer::Stride * sizeof(int16_t), "Link pose buffer");
    std::memcpy(sPlayer->skelAnime.jointTable, sData.data() + frame * viewer::Stride, viewer::Stride * sizeof(int16_t));
    if (sRootLock) std::memcpy(sPlayer->skelAnime.jointTable, sData.data(), sizeof(Vec3s));
}
static void Scene(int16_t) { Stop(false); }
static void SaveMarks() {
    std::string text;
    for (const auto& mark : sMarks) text += mark + "\n";
    if (SOH_MOD_API_HAS(sApi, StorageSetString)) sApi->StorageSetString("nei.animviewer", "byrna", text.c_str());
}
static void Import() {
    if (SOH_MOD_API_HAS(sApi, MountUserArchive)) {
        if (sApi->MountUserArchive("Load a Link animation library")) {
            Refresh();
        }
        return;
    }
    if (!SOH_MOD_API_HAS(sApi, PickUserFile) || !SOH_MOD_API_HAS(sApi, FreeUserFile) || !SOH_MOD_API_HAS(sApi, SaveToModsFolder)) return;
    SOHUserFile file{}; file.structSize = sizeof(file);
    if (!sApi->PickUserFile("Import Link animation library (restart required)", "O2R archives", "*.o2r;*.zip", &file)) return;
    if (file.size < 4 || !file.data || file.data[0] != 'P' || file.data[1] != 'K') sStatus = "Not a ZIP/O2R archive.";
    else {
        uint32_t hash = 2166136261u;
        for (uint64_t i = 0; i < file.size; ++i) hash = (hash ^ file.data[i]) * 16777619u;
        char name[80]; std::snprintf(name, sizeof(name), "animviewer_library_%08x.o2r", hash);
        sStatus = sApi->SaveToModsFolder(name, file.data, file.size) ?
            "Library saved. Restart the game, then Refresh." : "Library import was not saved.";
    }
    sApi->FreeUserFile(&file);
}
static void Panel() {
    static char search[256]{};
    static bool searchLoaded;
    static int32_t preset;
    static bool revealSelected;
    if (!searchLoaded) {
        std::snprintf(search, sizeof(search), "%s", CVarGetString("gMods.AnimViewer.Filter", ""));
        searchLoaded = true;
    }
    ModUI_Text("Link Anim Viewer");
    if (ModUI_Button("Refresh", 0, 0)) Refresh();
    ModUI_SameLine(0, -1);
    if (ModUI_Button("Import O2R...", 0, 0)) Import();
    ModUI_SameLine(0, -1);
    if (SOH_MOD_API_HAS(sApi, RequestRestart) && ModUI_Button("Restart game", 0, 0)) {
        Stop(true); sApi->RequestRestart("Mount animation library for Anim Viewer");
    }
    ModUI_Checkbox("Include resources with custom names (full scan)", &sFullScan);
    ModUI_TextWrapped(sStatus.c_str());
    if (!sScanned) return;
    ModUI_Separator();
    ModUI_SetNextItemWidth(-1);
    if (ModUI_InputTextWithHint("##clip-search", "Search animation name or path...", search, sizeof(search), 0)) {
        CVarSetString("gMods.AnimViewer.Filter", search); preset = 4; Filter(); revealSelected = true;
    }
    static const char* const presets[] = {"All clips", "Vault", "Spin", "Slash / kiru", "Custom search"};
    ModUI_SetNextItemWidth(180);
    if (ModUI_Combo("Search preset", &preset, presets, 5) && preset != 4) {
        const char* terms[] = {"", "vault", "spin", "kiru"};
        std::snprintf(search, sizeof(search), "%s", terms[preset]);
        CVarSetString("gMods.AnimViewer.Filter", search); Filter(); revealSelected = true;
    }
    std::set<int> archiveSet;
    for (const auto& clip : sClips) archiveSet.insert(clip.archive);
    std::vector<int> archiveIds{-1};
    std::vector<std::string> archiveNames{"All archives"};
    for (int id : archiveSet) { archiveIds.push_back(id); archiveNames.push_back("Archive " + std::to_string(id)); }
    std::vector<const char*> archiveLabels;
    for (const auto& name : archiveNames) archiveLabels.push_back(name.c_str());
    int32_t archiveIndex = 0;
    for (size_t i = 1; i < archiveIds.size(); ++i) if (archiveIds[i] == sArchive) archiveIndex = int32_t(i);
    ModUI_SetNextItemWidth(180);
    if (ModUI_Combo("Source", &archiveIndex, archiveLabels.data(), int32_t(archiveLabels.size()))) {
        sArchive = archiveIds[archiveIndex]; Filter(); revealSelected = true;
    }
    if (sFilter != Lower(CVarGetString("gMods.AnimViewer.Filter", ""))) {
        std::snprintf(search, sizeof(search), "%s", CVarGetString("gMods.AnimViewer.Filter", "")); Filter();
    }
    ModUI_Text((std::to_string(sVisible.size()) + " matching clips").c_str());
    const Entry* current = Selected();
    ModUI_SetNextItemWidth(-1);
    if (ModUI_BeginCombo("##selected-clip", current ? current->name.c_str() : "No matching clips", 0)) {
        for (int32_t i = 0; i < int32_t(sVisible.size()); ++i) {
            const auto& entry = sClips[sVisible[i]];
            if (ModUI_Selectable(entry.name.c_str(), i == sSelected, 0, 0, 0)) {
                sSelected = i; Load(); revealSelected = true;
            }
            if (i == sSelected) ModUI_SetItemDefaultFocus();
        }
        ModUI_EndCombo();
    }
    if (ModUI_BeginListBox("##animation-list", -1, 230)) {
        for (int32_t i = 0; i < int32_t(sVisible.size()); ++i) {
            const auto& entry = sClips[sVisible[i]];
            if (ModUI_Selectable(entry.name.c_str(), i == sSelected, 0, 0, 0)) { sSelected = i; Load(); }
            if (ModUI_IsItemHovered()) ModUI_SetTooltip((entry.name + "\n" + std::to_string(entry.frames) + " frames").c_str());
            if (i == sSelected && revealSelected) { ModUI_SetScrollHereY(0.5f); revealSelected = false; }
        }
        ModUI_EndListBox();
    }
    ModUI_BeginDisabled(sVisible.empty());
    if (ModUI_Button("Previous", 0, 0) && !sVisible.empty()) {
        sSelected = (sSelected + int32_t(sVisible.size()) - 1) % int32_t(sVisible.size()); Load(); revealSelected = true;
    }
    ModUI_SameLine(0, -1);
    if (ModUI_Button("Next", 0, 0) && !sVisible.empty()) {
        sSelected = (sSelected + 1) % int32_t(sVisible.size()); Load(); revealSelected = true;
    }
    ModUI_EndDisabled();
    const Entry* clip = Selected();
    if (!clip) {
        if (sRequested && ModUI_Button("Stop preview", 0, 0)) Stop(true);
        return;
    }
    if (sRequested && sLoaded != clip->name && !Load()) sRequested = false;
    ModUI_TextWrapped(clip->name.c_str());
    ModUI_Text(("Frames: " + std::to_string(clip->frames) + " | Archive: " + std::to_string(clip->archive)).c_str());
    if (ModUI_Button("Copy animation path", 0, 0)) ModUI_SetClipboardText(clip->name.c_str());
    ModUI_SameLine(0, -1);
    if (ModUI_Button("Copy raw clip path", 0, 0)) ModUI_SetClipboardText(clip->raw.c_str());
    ModUI_Separator();
    if (ModUI_Button(!sRequested ? "Play on Link" : sPaused ? "Resume" : "Pause", 0, 0)) {
        if (!sRequested) { sRequested = Load(); sPaused = false; } else sPaused = !sPaused;
    }
    ModUI_SameLine(0, -1);
    if (ModUI_Button("Stop", 0, 0)) Stop(true);
    ModUI_SameLine(0, -1);
    if (ModUI_Button("Restart clip", 0, 0)) { Load(); sPaused = false; }
    ModUI_Checkbox("Loop", &sLoop);
    ModUI_SameLine(0, -1);
    ModUI_Checkbox("Lock root translation", &sRootLock);
    ModUI_SliderInt("Speed (%)", &sSpeed, -400, 400);
    int32_t frame = int32_t(sFrame);
    if (ModUI_SliderInt("Frame", &frame, 0, clip->frames - 1)) { sPaused = true; sFrame = float(frame); }
    if (ModUI_SmallButton("< Frame")) { sPaused = true; sFrame = std::max(0.0f, std::floor(sFrame) - 1); }
    ModUI_SameLine(0, -1);
    if (ModUI_SmallButton("Frame >")) { sPaused = true; sFrame = std::min(float(clip->frames - 1), std::floor(sFrame) + 1); }
    ModUI_Separator();
    if (ModUI_Button("Mark for Byrna", 0, 0)) {
        sMarks.insert(clip->name + " | frame " + std::to_string(int(sFrame))); SaveMarks();
    }
    ModUI_SameLine(0, -1);
    if (ModUI_Button("Export candidates...", 0, 0) && SOH_MOD_API_HAS(sApi, SaveToModsFolder)) {
        std::string text = "Byrna animation candidates\n";
        for (const auto& mark : sMarks) text += mark + "\n";
        sStatus = sApi->SaveToModsFolder("animviewer_byrna.txt", text.data(), text.size()) ? "Candidates exported to mods/animviewer_byrna.txt." : "Export not saved.";
    }
    if (ModUI_BeginSection("Byrna candidates")) {
        if (ModUI_BeginChild("byrna-marks", 0, 140, true, 0)) {
            for (const auto& mark : sMarks)
                if (ModUI_Selectable(mark.c_str(), false, 0, 0, 0)) ModUI_SetClipboardText(mark.c_str());
        }
        ModUI_EndChild();
        if (ModUI_Button("Clear candidates", 0, 0)) { sMarks.clear(); SaveMarks(); }
        ModUI_EndSection();
    }
}
static const char* const sHooks[] = {"OnPlayerUpdate", "OnActorDraw", "OnSceneInit"};
static const SOHModRequirements sRequirements = {sizeof(SOHModRequirements), sHooks, 3};
extern "C" SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) { sApi = api; }
extern "C" SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements() { return &sRequirements; }
extern "C" SOH_MOD_EXPORT void ModInit() {
    if (!SOH_MOD_API_HAS(sApi, ReadArchiveFiles) || !SOH_MOD_API_HAS(sApi, RegisterMenuSidebar) ||
        !SOH_MOD_API_HAS(sApi, ReleasePlayerInput)) return;
    if (SOH_MOD_API_HAS(sApi, StorageGetString)) {
        char buffer[32768]{};
        sApi->StorageGetString("nei.animviewer", "byrna", buffer, sizeof(buffer));
        std::string text(buffer); size_t start = 0, end;
        while ((end = text.find('\n', start)) != std::string::npos) { sMarks.insert(text.substr(start, end - start)); start = end + 1; }
    }
    if (!sApi->RegisterMenuSidebar("Dev Tools", "Anim Viewer", "gMods.AnimViewer.Page", 1)) return;
    SOHModMenuWidget widget{};
    widget.structSize = sizeof(widget); widget.section = "Dev Tools"; widget.sidebar = "Anim Viewer";
    widget.type = SOH_MOD_MENU_CUSTOM; widget.label = "Anim Viewer"; widget.onChange = Panel;
    sApi->RegisterMenuWidget(&widget);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, Tick);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, Draw);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, Scene);
}
