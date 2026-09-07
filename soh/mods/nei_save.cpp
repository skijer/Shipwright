// Skijer's NEI — per-save state moved out of the (now 100% vanilla) SaveContext
// into a dedicated "nei" SaveManager section. Old saves lose this state (accepted).
#include <string.h>
#include <string>
#include <fstream>    // pictograph OoT<->MM picture file
#include <filesystem> // Save directory path

#include "nei_save.h"
#include "items/custom_bottles.h" // Bottle_WheelResetTracking (wheel session trackers)
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h" // Nei_TrirodNotify bridge
#include <soh/OTRGlobals.h>                // gSaveContext
#include <libultraship/libultraship.h>     // Ship::Context (full def for GetPathRelativeToAppDirectory)

static NeiSaveData gNeiSave;

extern "C" SaveContext gSaveContext;

extern "C" NeiSaveData* Nei_Save(void) {
    return &gNeiSave;
}

// Skijer's NEI hookshot overhaul — tiny accessor for decomp TUs that don't pull nei_save.h in
// (z_player_lib.c doubles the Longshot reticle raycast while the Ultrashot unlock is owned).
extern "C" uint8_t Nei_UltrashotOwned(void) {
    return gNeiSave.ultrashotOwned;
}

extern "C" uint16_t Nei_GetOwnedItem(uint8_t slot) {
    if (slot >= 24 && slot < 72) {
        return gNeiSave.ownedItems[slot - 24];
    }
    return 0;
}

extern "C" void Nei_SetOwnedItem(uint8_t slot, uint16_t v) {
    if (slot >= 24 && slot < 72) {
        gNeiSave.ownedItems[slot - 24] = v;
    }
}

// ── Pictograph OoT <-> MM shared picture (Skijer's NEI) ─────────────────────
// In a COMBO file there is one pictograph, not two. Both games read and write the same two files in
// the shared fleet folder, in MM's EXACT format, so nothing is ever converted:
//   <ShipDir>/fleet/picture.bin    = pictoPhotoI5, raw I5 buffer (11200 bytes, byte-for-byte what MM
//                                    holds in gSaveContext.pictoPhotoI5; a byte array, no endianness).
//   <ShipDir>/fleet/pictoflags.bin = pictoFlags0 then pictoFlags1 (MM's two u32 PICTO_VALID_* bit-sets
//                                    saying WHICH mapped subject was validly photographed — the data
//                                    that drives the MM reward). 8 bytes, native order (both PC ports
//                                    hold them native). Flags travel WITH the picture: a new picture
//                                    replaces them wholesale, exactly like Snap_RecordPictographedActors.
// In a SOLO file none of this runs — see Picto_IsSharedPhoto below.
// <Save>/file<N><suffix> — a per-save sidecar (still used by the trade-item sync below).
static std::string Nei_SidecarPath(const char* suffix) {
    std::filesystem::path dir(Ship::Context::GetPathRelativeToAppDirectory("Save"));
    return (dir / ("file" + std::to_string(gSaveContext.fileNum + 1) + suffix)).string();
}

// <ShipDir>/fleet/<name> — the folder BOTH exes resolve to the same place. In a combo file the
// picture is not copied between the games, it IS the same picture, so it lives here and not next to
// one game's save file. That is also why it carries no slot number: one combo session, one picture.
extern "C" const char* FleetSync_SharedFilePath(const char* name);

static std::string Picto_SavePath(const char* name) {
    const char* shared = FleetSync_SharedFilePath(name);
    return (shared != nullptr) ? std::string(shared) : std::string();
}

// THE SIDECARS ARE THE FLEET-COMBO BRIDGE, NOTHING ELSE. In a combo file (QUEST_OOTXMM) OoT and MM
// share ONE picture: whatever you shoot in either game is THE pictograph, so it has to live in a file
// both of them read and write. In a solo file there is nothing to share — the picture belongs to that
// save and to no other, and it already rides inside the .sav with the rest of the NEI section
// (pictoPhotoI5 / pictoFlags0/1 / pictoHasPhoto).
//
// Reading them unconditionally is what leaked a pictograph between save slots, and granting ownership
// from "a picture file exists" leaked the BOX itself: a photo taken in slot 1 handed the Pictograph
// Box to slot 2. Ownership now comes only from where it belongs — the inventory (and FleetSync in a
// combo file). Skijer 2026-08-08
static bool Picto_IsSharedPhoto(void) {
    return IS_OOTXMM; // z64save.h: gSaveContext.ship.quest.id == QUEST_OOTXMM
}

extern "C" void Picto_SyncWrite(void) {
    if (!Picto_IsSharedPhoto()) {
        return; // solo file: the photo lives in this save only
    }
    {
        std::ofstream f(Picto_SavePath("picture.bin"), std::ios::binary | std::ios::trunc);
        if (f) {
            f.write(reinterpret_cast<const char*>(gNeiSave.pictoPhotoI5), sizeof(gNeiSave.pictoPhotoI5));
        }
    }
    {
        std::ofstream f(Picto_SavePath("pictoflags.bin"), std::ios::binary | std::ios::trunc);
        if (f) {
            f.write(reinterpret_cast<const char*>(&gNeiSave.pictoFlags0), sizeof(gNeiSave.pictoFlags0));
            f.write(reinterpret_cast<const char*>(&gNeiSave.pictoFlags1), sizeof(gNeiSave.pictoFlags1));
        }
    }
}

static void Picto_SyncRead(void) {
    if (!Picto_IsSharedPhoto()) {
        return; // solo file: whatever this save loaded is the picture, period
    }
    bool gotPhoto = false;
    {
        std::ifstream f(Picto_SavePath("picture.bin"), std::ios::binary);
        if (f) {
            f.read(reinterpret_cast<char*>(gNeiSave.pictoPhotoI5), sizeof(gNeiSave.pictoPhotoI5));
            gotPhoto = (bool)f; // a full 11200-byte read succeeded
        }
    }
    {
        std::ifstream f(Picto_SavePath("pictoflags.bin"), std::ios::binary);
        if (f) {
            f.read(reinterpret_cast<char*>(&gNeiSave.pictoFlags0), sizeof(gNeiSave.pictoFlags0));
            f.read(reinterpret_cast<char*>(&gNeiSave.pictoFlags1), sizeof(gNeiSave.pictoFlags1));
        }
    }
    // NO shared picture means the RUN has no picture — not "keep the one in this save". There is a
    // single pictograph in a combo, so MM throwing it away has to throw it away here too, or OoT goes
    // on offering a print that no longer exists and the next picture taken in MM looks like it never
    // arrived because OoT was still holding the old one. Absence has to travel like presence.
    if (!gotPhoto) {
        memset(gNeiSave.pictoPhotoI5, 0, sizeof(gNeiSave.pictoPhotoI5));
        gNeiSave.pictoFlags0 = 0;
        gNeiSave.pictoFlags1 = 0;
        gNeiSave.pictoHasPhoto = 0;
        return;
    }

    // A sidecar that is all zeros is not a picture — it is a leftover from a cleared slot, and
    // claiming it would arm MM's "you already have a picture" branch (the pictograph button shows
    // the stored photo instead of opening the lens) over an empty image.
    // NOTE: this sets pictoHasPhoto ONLY. It must never grant pictoboxOwned — the Box is an
    // inventory item, and in a combo file FleetSync is what carries it across. Skijer's NEI
    gNeiSave.pictoHasPhoto = 0;
    for (size_t i = 0; i < sizeof(gNeiSave.pictoPhotoI5); i++) {
        if (gNeiSave.pictoPhotoI5[i] != 0) {
            gNeiSave.pictoHasPhoto = 1;
            break;
        }
    }
    if (!gNeiSave.pictoHasPhoto) {
        gNeiSave.pictoFlags0 = 0;
        gNeiSave.pictoFlags1 = 0;
    }
}

// ── The COLOUR half of the print ────────────────────────────────────────────
// The I5 buffer is greyscale by construction (it is all MM ever stored), so the colour print needs a
// file of its own or a picture arrives in sepia no matter what was on screen. It is picto_box.c's
// sPictoColorTex raw: 160x112 RGBA16, byte-swapped, byte-identical to what 2Ship's ColorPictograph
// holds — so between the games it is a straight copy, and inside one game it is what makes the colour
// survive a reload (11200 bytes of I5 fit in the .sav json; 35840 of RGBA would bloat it).
//   combo: <ShipDir>/fleet/picture_rgba.bin  (shared — same print in both games)
//   solo:  <Save>/file<N>_picture_rgba.bin   (this save's own, like MM's per-slot colour PNG)
static std::string Picto_ColorPath(void) {
    return Picto_IsSharedPhoto() ? Picto_SavePath("picture_rgba.bin") : Nei_SidecarPath("_picture_rgba.bin");
}

extern "C" void Picto_SyncWriteColor(const void* rgba16, int size) {
    if (rgba16 == nullptr || size <= 0) {
        return;
    }
    std::string path = Picto_ColorPath();
    if (path.empty()) {
        return;
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(reinterpret_cast<const char*>(rgba16), size);
    }
}

// Returns 1 when a full colour print was read into the buffer. 0 leaves it untouched — the caller
// then shows MM's sepia, which is the correct fallback and never a black frame.
extern "C" int Picto_SyncReadColor(void* rgba16, int size) {
    if (rgba16 == nullptr || size <= 0) {
        return 0;
    }
    std::string path = Picto_ColorPath();
    if (path.empty()) {
        return 0;
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return 0;
    }
    f.read(reinterpret_cast<char*>(rgba16), size);
    return f ? 1 : 0;
}

// Throw the stored picture away, sidecars included (MM's REMOVE_QUEST_ITEM(QUEST_PICTOGRAPH) when you
// answer "No"). Without this the files survive and Picto_SyncRead resurrects the photo on the next
// load — so "No" would only delete it until you reloaded, and every session would open on the
// "you already have a picture" prompt instead of the lens. Skijer's NEI
extern "C" void Picto_SyncClear(void) {
    std::error_code ec;
    std::filesystem::remove(Picto_SavePath("picture.bin"), ec);
    std::filesystem::remove(Picto_SavePath("pictoflags.bin"), ec);
    std::filesystem::remove(Picto_ColorPath(), ec); // the colour half goes with it
    memset(gNeiSave.pictoPhotoI5, 0, sizeof(gNeiSave.pictoPhotoI5));
    gNeiSave.pictoHasPhoto = 0;
}

// MM adult trade-quest items OoT<->MM sync (Skijer's NEI). A sidecar next to the save holds the
// tradeAdultOwned bitmask (4 bytes, native) so 2Ship can mirror which trade items the player owns for
// the Anju exchange. <Save>/file<N>_tradeitems.bin. The Pendant of Memories crossing over also re-grants
// its Ext Boots 2 combat moveset on the OoT side.
static void TradeItems_SyncWrite(void) {
    std::ofstream f(Nei_SidecarPath("_tradeitems.bin"), std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(reinterpret_cast<const char*>(&gNeiSave.tradeAdultOwned), sizeof(gNeiSave.tradeAdultOwned));
    }
}

static void TradeItems_SyncRead(void) {
    std::ifstream f(Nei_SidecarPath("_tradeitems.bin"), std::ios::binary);
    if (f) {
        uint32_t v = 0;
        f.read(reinterpret_cast<char*>(&v), sizeof(v));
        if (f) {
            gNeiSave.tradeAdultOwned |= v; // merge cross-game ownership
            // REMOVED: this used to also light extEquipOwnedBits bit 26 when the Pendant trade bit
            // (index 19) arrived, back when the Pendant lived in the Ext Boots 2 cell. After the
            // 2026-07-29 kaleido re-layout, bit 26 is CLIMB BOOTS — so obtaining the Pendant in MM
            // handed the OoT save a Climb Boots it never got, and it also undid ExtEquip_Init's
            // migration off that slot. Ownership of the Pendant is now latched from the trade bit by
            // ExtEquip_PendantOwned() (extended_equipment.c:354), which is the single source of
            // truth and needs nothing here. Skijer's NEI
        }
    }
}

namespace {

constexpr const char* kSaveSectionName = "nei";

void NeiSave_Init(bool isDebug) {
    memset(&gNeiSave, 0, sizeof(gNeiSave));
    // Empty custom slots = ITEM_NONE (0xFF), not 0 (=ITEM_STICK). Skijer's NEI
    // ownedItems is u16 now, so memset(0xFF) would write 0xFFFF per entry — and the empty marker
    // is ITEM_NONE (0xFF), not 0xFFFF. Fill it element by element. Skijer's NEI
    for (int i = 0; i < (int)(sizeof(gNeiSave.ownedItems) / sizeof(gNeiSave.ownedItems[0])); i++) {
        gNeiSave.ownedItems[i] = 0xFF;
    }
    // Bottle slots start empty.
    memset(gNeiSave.bottleSlots, 0xFF, sizeof(gNeiSave.bottleSlots));
    gNeiSave.bottomlessContent = 0xFF; // empty (0 would read as ITEM_STICK)
    // Session wheel trackers must not leak across files (per-frame reconcile would ghost-write).
    Bottle_WheelResetTracking();
}

void NeiSave_Save(SaveContext* saveContext, int sectionID, bool fullSave) {
    SaveManager::Instance->SaveArray("ownedItems", 48,
                                     [](size_t i) { SaveManager::Instance->SaveData("", gNeiSave.ownedItems[i]); });
    SaveManager::Instance->SaveData("shovelOwned", gNeiSave.shovelOwned);
    SaveManager::Instance->SaveData("dominionOwned", gNeiSave.dominionOwned);
    SaveManager::Instance->SaveData("pokeballOwned", gNeiSave.pokeballOwned);
    SaveManager::Instance->SaveData("extEquipOwnedBits", gNeiSave.extEquipOwnedBits);
    SaveManager::Instance->SaveData("lanternFireType", gNeiSave.lanternFireType);
    SaveManager::Instance->SaveData("lanternCapturedTypes", gNeiSave.lanternCapturedTypes);
    SaveManager::Instance->SaveData("twilightUpgrade", gNeiSave.twilightUpgrade);
    SaveManager::Instance->SaveData("ultrashotOwned", gNeiSave.ultrashotOwned); // Skijer's NEI hookshot overhaul
    SaveManager::Instance->SaveData("clawshotModeActive", gNeiSave.clawshotModeActive);
    SaveManager::Instance->SaveData("galeBoomerangModeActive", gNeiSave.galeBoomerangModeActive);
    SaveManager::Instance->SaveData("weaponUpgrades", gNeiSave.weaponUpgrades);
    SaveManager::Instance->SaveData("extEquipSword", gNeiSave.extEquipSword);
    SaveManager::Instance->SaveData("extEquipShield", gNeiSave.extEquipShield);
    SaveManager::Instance->SaveData("extEquipTunic", gNeiSave.extEquipTunic);
    SaveManager::Instance->SaveData("extEquipBoots", gNeiSave.extEquipBoots);
    // Bottle randomizer
    SaveManager::Instance->SaveArray("bottleSlots", 8,
                                     [](size_t i) { SaveManager::Instance->SaveData("", gNeiSave.bottleSlots[i]); });
    SaveManager::Instance->SaveData("bottomlessBottleMode", gNeiSave.bottomlessBottleMode);
    SaveManager::Instance->SaveData("netEquipped", gNeiSave.netEquipped);
    SaveManager::Instance->SaveData("bottomlessContent", gNeiSave.bottomlessContent);
    SaveManager::Instance->SaveData("bottomlessCount", gNeiSave.bottomlessCount);
    SaveManager::Instance->SaveData("powerKegOwned", gNeiSave.powerKegOwned);
    SaveManager::Instance->SaveData("powerKegCount", gNeiSave.powerKegCount);
    SaveManager::Instance->SaveData("powerKegMode", gNeiSave.powerKegMode);
    SaveManager::Instance->SaveData("tradeAdultOwned", gNeiSave.tradeAdultOwned);
    // Pictograph Box (MM-format). Flags are tiny; the 11200-byte I5 photo is only written when the
    // pictobox is owned so non-users don't bloat their save.
    SaveManager::Instance->SaveData("pictoboxOwned", gNeiSave.pictoboxOwned);
    SaveManager::Instance->SaveData("pictoHasPhoto", gNeiSave.pictoHasPhoto);
    SaveManager::Instance->SaveData("pictoFlags0", gNeiSave.pictoFlags0);
    SaveManager::Instance->SaveData("pictoFlags1", gNeiSave.pictoFlags1);
    if (gNeiSave.pictoboxOwned) {
        SaveManager::Instance->SaveArray(
            "pictoPhotoI5", 11200, [](size_t i) { SaveManager::Instance->SaveData("", gNeiSave.pictoPhotoI5[i]); });
    }
    // Fleet Ship Combo cross-game fields (layouts in FleetShipCombo/FleetComboIds.h)
    SaveManager::Instance->SaveData("shieldOwned", gNeiSave.shieldOwned);
    SaveManager::Instance->SaveData("mmQuestItems", gNeiSave.mmQuestItems);
    SaveManager::Instance->SaveArray("comboObtained", 128,
                                     [](size_t i) { SaveManager::Instance->SaveData("", gNeiSave.comboObtained[i]); });
    // Generic fcId-indexed cross-item store (comboObtainedFc synced, comboAppliedFc local — both persisted)
    SaveManager::Instance->SaveArray("comboObtainedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->SaveData("", gNeiSave.comboObtainedFc[i]);
    });
    SaveManager::Instance->SaveArray("comboAppliedFc", FC_COMBO_OBTAINED_FC_SIZE,
                                     [](size_t i) { SaveManager::Instance->SaveData("", gNeiSave.comboAppliedFc[i]); });
    SaveManager::Instance->SaveData("comboTriforce", gNeiSave.comboTriforce);
    SaveManager::Instance->SaveData("comboGoalFlags", gNeiSave.comboGoalFlags);
    SaveManager::Instance->SaveData("capeHidden", gNeiSave.capeHidden);
    SaveManager::Instance->SaveData("pendantEffectOff", gNeiSave.pendantEffectOff);
    SaveManager::Instance->SaveData("capeOwned", gNeiSave.capeOwned);
    SaveManager::Instance->SaveData("extTunicLayoutVersion", gNeiSave.extTunicLayoutVersion);
    // Dual Cane (Skijer's NEI). caneSkills is the whole progression — without these
    // three lines the mask reloads as 0, the cane looks unowned, and the in-game
    // self-heal in Handle_CaneOfSomaria re-grants only the base Statue skill. That
    // is exactly the "I saved with upgrades and came back with just the cane and
    // Statue" symptom.
    SaveManager::Instance->SaveData("caneSkills", gNeiSave.caneSkills);
    SaveManager::Instance->SaveData("caneType", gNeiSave.caneType);
    SaveManager::Instance->SaveArray("caneSkillSel", 2,
                                     [](size_t i) { SaveManager::Instance->SaveData("", gNeiSave.caneSkillSel[i]); });
    // Trirod echoes: the learned bitmask over gTrirodEchoes rows (two u32 — the
    // save layer has no u64 path) and the echo the C button summons.
    SaveManager::Instance->SaveData("trirodEchoesLo", gNeiSave.trirodEchoesLo);
    SaveManager::Instance->SaveData("trirodEchoesHi", gNeiSave.trirodEchoesHi);
    SaveManager::Instance->SaveData("trirodSel", gNeiSave.trirodSel);
    SaveManager::Instance->SaveData("trirodLayoutVersion", gNeiSave.trirodLayoutVersion);
    SaveManager::Instance->SaveData("trirodFullList", gNeiSave.trirodFullList);
    SaveManager::Instance->SaveData("season", gNeiSave.season);
    SaveManager::Instance->SaveData("seasonsOwned", gNeiSave.seasonsOwned);
    SaveManager::Instance->SaveData("quartzOwned", gNeiSave.quartzOwned);
    SaveManager::Instance->SaveData("quartzCategory", gNeiSave.quartzCategory);
    SaveManager::Instance->SaveData("quartzSubcat", gNeiSave.quartzSubcat);
    SaveManager::Instance->SaveData("pendantOwned", gNeiSave.pendantOwned);
    SaveManager::Instance->SaveData("extBootsLayoutVersion", gNeiSave.extBootsLayoutVersion);
    SaveManager::Instance->SaveData("sw97BowElement", gNeiSave.sw97BowElement);
    SaveManager::Instance->SaveData("sw97SlingElement", gNeiSave.sw97SlingElement);
    SaveManager::Instance->SaveData("bombArrowsOwned", gNeiSave.bombArrowsOwned);
    SaveManager::Instance->SaveData("wandMode", gNeiSave.wandMode);
    SaveManager::Instance->SaveData("wandRodsOwned", gNeiSave.wandRodsOwned);
    SaveManager::Instance->SaveData("sw97LayoutVersion", gNeiSave.sw97LayoutVersion);
    SaveManager::Instance->SaveData("ootMasksOwned", gNeiSave.ootMasksOwned);
    SaveManager::Instance->SaveData("slateMode", gNeiSave.slateMode);
    SaveManager::Instance->SaveData("slateRunesOwned", gNeiSave.slateRunesOwned);
    SaveManager::Instance->SaveData("ritoMaskFlags", gNeiSave.ritoMaskFlags);
    TradeItems_SyncWrite(); // mirror the MM trade-item flags next to the save for 2Ship
}

void NeiSave_Load() {
    // memset first so a save lacking this section loads as a clean new game.
    memset(&gNeiSave, 0, sizeof(gNeiSave));
    // ownedItems is u16 now, so memset(0xFF) would write 0xFFFF per entry — and the empty marker
    // is ITEM_NONE (0xFF), not 0xFFFF. Fill it element by element. Skijer's NEI
    for (int i = 0; i < (int)(sizeof(gNeiSave.ownedItems) / sizeof(gNeiSave.ownedItems[0])); i++) {
        gNeiSave.ownedItems[i] = 0xFF;
    }
    memset(gNeiSave.bottleSlots, 0xFF, sizeof(gNeiSave.bottleSlots)); // empty bottle slots
    SaveManager::Instance->LoadArray("ownedItems", 48, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.ownedItems[i], (uint16_t)0xFF); // ITEM_NONE (u16 store)
    });
    SaveManager::Instance->LoadData("shovelOwned", gNeiSave.shovelOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("dominionOwned", gNeiSave.dominionOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("pokeballOwned", gNeiSave.pokeballOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipOwnedBits", gNeiSave.extEquipOwnedBits, (uint32_t)0);
    SaveManager::Instance->LoadData("lanternFireType", gNeiSave.lanternFireType, (uint8_t)0);
    SaveManager::Instance->LoadData("lanternCapturedTypes", gNeiSave.lanternCapturedTypes, (uint8_t)0);
    SaveManager::Instance->LoadData("twilightUpgrade", gNeiSave.twilightUpgrade, (uint8_t)0);
    SaveManager::Instance->LoadData("ultrashotOwned", gNeiSave.ultrashotOwned,
                                    (uint8_t)0); // Skijer's NEI hookshot overhaul
    SaveManager::Instance->LoadData("clawshotModeActive", gNeiSave.clawshotModeActive, (uint8_t)0);
    SaveManager::Instance->LoadData("galeBoomerangModeActive", gNeiSave.galeBoomerangModeActive, (uint8_t)0);
    SaveManager::Instance->LoadData("weaponUpgrades", gNeiSave.weaponUpgrades, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipSword", gNeiSave.extEquipSword, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipShield", gNeiSave.extEquipShield, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipTunic", gNeiSave.extEquipTunic, (uint8_t)0);
    SaveManager::Instance->LoadData("extEquipBoots", gNeiSave.extEquipBoots, (uint8_t)0);
    // Bottle randomizer
    SaveManager::Instance->LoadArray("bottleSlots", 8, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.bottleSlots[i], (uint8_t)0xFF);
    });
    SaveManager::Instance->LoadData("bottomlessBottleMode", gNeiSave.bottomlessBottleMode, (uint8_t)0);
    SaveManager::Instance->LoadData("netEquipped", gNeiSave.netEquipped, (uint8_t)0);
    SaveManager::Instance->LoadData("bottomlessContent", gNeiSave.bottomlessContent, (uint8_t)0xFF); // empty
    SaveManager::Instance->LoadData("bottomlessCount", gNeiSave.bottomlessCount, (uint8_t)0);
    SaveManager::Instance->LoadData("powerKegOwned", gNeiSave.powerKegOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("powerKegCount", gNeiSave.powerKegCount, (uint8_t)0);
    SaveManager::Instance->LoadData("powerKegMode", gNeiSave.powerKegMode, (uint8_t)0);
    SaveManager::Instance->LoadData("tradeAdultOwned", gNeiSave.tradeAdultOwned, (uint32_t)0);
    // Pictograph Box (MM-format). Photo array only present when the pictobox is owned.
    SaveManager::Instance->LoadData("pictoboxOwned", gNeiSave.pictoboxOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("pictoHasPhoto", gNeiSave.pictoHasPhoto, (uint8_t)0);
    SaveManager::Instance->LoadData("pictoFlags0", gNeiSave.pictoFlags0, (uint32_t)0);
    SaveManager::Instance->LoadData("pictoFlags1", gNeiSave.pictoFlags1, (uint32_t)0);
    if (gNeiSave.pictoboxOwned) {
        SaveManager::Instance->LoadArray("pictoPhotoI5", 11200, [](size_t i) {
            SaveManager::Instance->LoadData("", gNeiSave.pictoPhotoI5[i], (uint8_t)0);
        });
    }
    // Fleet Ship Combo cross-game fields (older saves load as zero = nothing obtained)
    SaveManager::Instance->LoadData("shieldOwned", gNeiSave.shieldOwned, (uint16_t)0);
    SaveManager::Instance->LoadData("mmQuestItems", gNeiSave.mmQuestItems, (uint32_t)0);
    SaveManager::Instance->LoadArray("comboObtained", 128, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.comboObtained[i], (uint8_t)0);
    });
    // Generic fcId-indexed cross-item store (LoadArray tolerates absent/short arrays -> defaults 0,
    // so older saves and future FC-table growth stay backward compatible).
    SaveManager::Instance->LoadArray("comboObtainedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.comboObtainedFc[i], (uint8_t)0);
    });
    SaveManager::Instance->LoadArray("comboAppliedFc", FC_COMBO_OBTAINED_FC_SIZE, [](size_t i) {
        SaveManager::Instance->LoadData("", gNeiSave.comboAppliedFc[i], (uint8_t)0);
    });
    SaveManager::Instance->LoadData("comboTriforce", gNeiSave.comboTriforce, (uint16_t)0);
    SaveManager::Instance->LoadData("comboGoalFlags", gNeiSave.comboGoalFlags, (uint8_t)0);
    SaveManager::Instance->LoadData("capeHidden", gNeiSave.capeHidden, (uint8_t)0);
    SaveManager::Instance->LoadData("pendantEffectOff", gNeiSave.pendantEffectOff, (uint8_t)0);
    SaveManager::Instance->LoadData("capeOwned", gNeiSave.capeOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("extTunicLayoutVersion", gNeiSave.extTunicLayoutVersion, (uint8_t)0);
    // Quartz of Motion — absent keys = not owned, tracking the first category.
    // Dual Cane — absent keys mean a save from before the Dual Cane rework: mask 0,
    // which Handle_CaneOfSomaria heals into the base Statue skill on first equip.
    SaveManager::Instance->LoadData("caneSkills", gNeiSave.caneSkills, (uint8_t)0);
    SaveManager::Instance->LoadData("caneType", gNeiSave.caneType, (uint8_t)0);
    SaveManager::Instance->LoadArray(
        "caneSkillSel", 2, [](size_t i) { SaveManager::Instance->LoadData("", gNeiSave.caneSkillSel[i], (uint8_t)0); });
    // Trirod echoes — absent keys = nothing learned yet.
    SaveManager::Instance->LoadData("trirodEchoesLo", gNeiSave.trirodEchoesLo, (uint32_t)0);
    SaveManager::Instance->LoadData("trirodEchoesHi", gNeiSave.trirodEchoesHi, (uint32_t)0);
    SaveManager::Instance->LoadData("trirodSel", gNeiSave.trirodSel, (uint8_t)0);
    SaveManager::Instance->LoadData("trirodLayoutVersion", gNeiSave.trirodLayoutVersion, (uint8_t)0);
    SaveManager::Instance->LoadData("trirodFullList", gNeiSave.trirodFullList, (uint8_t)0);
    // v2 reordered the echo table: a v1 mask's bits point at DIFFERENT echoes now,
    // so carrying them over would "learn" the wrong things silently. Clearing is
    // the only honest migration (the extBootsLayoutVersion pattern).
    if (gNeiSave.trirodLayoutVersion < 2) {
        gNeiSave.trirodEchoesLo = 0;
        gNeiSave.trirodEchoesHi = 0;
        gNeiSave.trirodSel = 0;
        gNeiSave.trirodLayoutVersion = 2;
    }
    // Rod of Seasons — absent keys = rod not owned, and Spring is the index a fresh save falls back
    // to anyway (Seasons_GetSeason heals it to an owned one).
    SaveManager::Instance->LoadData("season", gNeiSave.season, (uint8_t)SEASON_SPRING);
    SaveManager::Instance->LoadData("seasonsOwned", gNeiSave.seasonsOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("quartzOwned", gNeiSave.quartzOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("quartzCategory", gNeiSave.quartzCategory, (uint8_t)0);
    SaveManager::Instance->LoadData("quartzSubcat", gNeiSave.quartzSubcat, (uint8_t)0);
    SaveManager::Instance->LoadData("pendantOwned", gNeiSave.pendantOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("extBootsLayoutVersion", gNeiSave.extBootsLayoutVersion, (uint8_t)0);
    SaveManager::Instance->LoadData("sw97BowElement", gNeiSave.sw97BowElement, (uint8_t)0);
    SaveManager::Instance->LoadData("sw97SlingElement", gNeiSave.sw97SlingElement, (uint8_t)0);
    SaveManager::Instance->LoadData("bombArrowsOwned", gNeiSave.bombArrowsOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("wandMode", gNeiSave.wandMode, (uint8_t)0);
    SaveManager::Instance->LoadData("wandRodsOwned", gNeiSave.wandRodsOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("sw97LayoutVersion", gNeiSave.sw97LayoutVersion, (uint8_t)0);
    SaveManager::Instance->LoadData("ootMasksOwned", gNeiSave.ootMasksOwned, (uint16_t)0);
    SaveManager::Instance->LoadData("slateMode", gNeiSave.slateMode, (uint8_t)0);
    SaveManager::Instance->LoadData("slateRunesOwned", gNeiSave.slateRunesOwned, (uint8_t)0);
    SaveManager::Instance->LoadData("ritoMaskFlags", gNeiSave.ritoMaskFlags, (uint8_t)0);
    // Cross-game: a pictograph synced from 2Ship (shared sidecar) wins over the SOH save copy.
    Picto_SyncRead();
    TradeItems_SyncRead(); // merge MM trade-item ownership from the cross-game sidecar
    // Session wheel trackers must not leak across files (per-frame reconcile would ghost-write).
    Bottle_WheelResetTracking();
}

void Register() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    SaveManager::Instance->AddInitFunction(NeiSave_Init);
    SaveManager::Instance->AddSaveFunction(kSaveSectionName, 1, NeiSave_Save, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction(kSaveSectionName, 1, NeiSave_Load);
}

static RegisterShipInitFunc gNeiSaveInit(Register);

} // namespace

// ─── Skijer's NEI Dual Cane (Somaria / Pacci) ────────────────────────────────────────────────────
// Six separate obtainable skills share ONE kaleido slot. `caneSkills` is the 6-bit ownership mask
// (bits 0..2 Somaria: Statue/Block/Platform, bits 3..5 Pacci: Flip/Stone/Ultrahand). `caneType` and
// `caneSkillSel` are the player's wheel selection, persisted so it survives save/load.
//
// Every getter AUTO-CORRECTS: the skills can be obtained in any order, so the stored selection is
// routinely pointing at something not owned yet (a fresh file starts at type 0 / slot 0 and the
// player's first pickup may well be Pacci-Ultrahand). Correcting on read keeps the wheel, the HUD
// and the cast path from ever disagreeing about what the button does.

// Mirrors item_cane_of_somaria.h — kept local so this TU doesn't pull the item headers in.
#define NEI_CANE_SKILL_MAX 6
#define NEI_CANE_TYPE_MAX 4
#define NEI_CANE_SOMARIA_MASK 0x07
#define NEI_CANE_PACCI_MASK 0x38

static uint8_t Nei_CaneTypeMask(uint8_t type) {
    return (type == 1) ? NEI_CANE_PACCI_MASK : NEI_CANE_SOMARIA_MASK;
}

extern "C" uint8_t Nei_CaneSkillMask(void) {
    return Nei_Save()->caneSkills;
}

extern "C" uint8_t Nei_CaneHasSkill(uint8_t skill) {
    if (skill >= NEI_CANE_SKILL_MAX) {
        return 0;
    }
    return (Nei_Save()->caneSkills & (1 << skill)) ? 1 : 0;
}

extern "C" void Nei_CaneGrantSkill(uint8_t skill) {
    if (skill >= NEI_CANE_SKILL_MAX) {
        return;
    }
    Nei_Save()->caneSkills |= (uint8_t)(1 << skill);
}

extern "C" uint8_t Nei_CaneOwned(void) {
    return Nei_Save()->caneSkills != 0;
}

// Four cane entries, each gated by one skill bit: Somaria(bit0), Trirod(bit2),
// Pacci(bit3), Ultrahand(bit5). Trirod and Ultrahand are ADDITIONAL wheel entries
// at the end of their chain, not replacements for the cane that leads to them.
static uint8_t Nei_CaneTypeGateBit(uint8_t type) {
    switch (type) {
        case 1:
            return 2; // Trirod    <- Somaria L3
        case 2:
            return 3; // Pacci     <- Pacci L1
        case 3:
            return 5; // Ultrahand <- Pacci L3
        case 0:
        default:
            return 0; // Somaria   <- Somaria L1
    }
}

extern "C" uint8_t Nei_CaneTypeOwned(uint8_t type) {
    if (type >= NEI_CANE_TYPE_MAX) {
        return 0;
    }
    return (Nei_Save()->caneSkills & (1 << Nei_CaneTypeGateBit(type))) ? 1 : 0;
}

// ── Wheel configuration (user 2026-08-06): which of the four ride the wheel ──
// Mirror of the MM side. Four wheel shapes via the shared CVar gItemEditor.CaneWheelMode:
//   0 = S-T-P-U (everything owned)   1 = T-U (end-items only)
//   2 = S-T-U (Pacci hides)          3 = T-P-U (Somaria hides)
// A base cane is only hidden when its own end-item is owned. Skijer's NEI
extern "C" uint8_t Nei_CaneTypeVisible(uint8_t type) {
    if (!Nei_CaneTypeOwned(type)) {
        return 0;
    }
    int mode = CVarGetInteger("gItemEditor.CaneWheelMode", 0);
    if (type == 0 && (mode == 1 || mode == 3) && Nei_CaneTypeOwned(1)) {
        return 0; // Somaria hidden behind an owned Trirod
    }
    if (type == 2 && (mode == 1 || mode == 2) && Nei_CaneTypeOwned(3)) {
        return 0; // Pacci hidden behind an owned Ultrahand
    }
    return 1;
}

// How many of the four the wheel actually shows — it only opens past one.
extern "C" uint8_t Nei_CaneTypeCount(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < NEI_CANE_TYPE_MAX; i++) {
        n += Nei_CaneTypeVisible(i);
    }
    return n;
}

// Next VISIBLE type in `dir`, wrapping. Returns the current one when nothing else
// is visible, so a single-entry wheel is a no-op rather than a crash.
extern "C" uint8_t Nei_CaneNextType(int8_t dir) {
    uint8_t cur = Nei_CaneGetType();
    for (uint8_t step = 1; step <= NEI_CANE_TYPE_MAX; step++) {
        int16_t probe = (int16_t)cur + (int16_t)(dir >= 0 ? step : -step);
        while (probe < 0) {
            probe += NEI_CANE_TYPE_MAX;
        }
        probe %= NEI_CANE_TYPE_MAX;
        if (Nei_CaneTypeVisible((uint8_t)probe)) {
            return (uint8_t)probe;
        }
    }
    return cur;
}

extern "C" uint8_t Nei_CaneGetType(void) {
    NeiSaveData* nei = Nei_Save();
    uint8_t type = (nei->caneType < NEI_CANE_TYPE_MAX) ? nei->caneType : 0;

    // Visible beats merely owned: switching the wheel shape can hide the stored type, and the cell
    // must follow the wheel or it shows a cane the wheel can no longer reach. Skijer's NEI
    if (Nei_CaneTypeVisible(type)) {
        return type;
    }
    for (uint8_t i = 0; i < NEI_CANE_TYPE_MAX; i++) {
        if (Nei_CaneTypeVisible(i)) {
            return i;
        }
    }
    // Nothing visible (fresh file / pre-split save): fall back to owned, then 0.
    if (Nei_CaneTypeOwned(type)) {
        return type;
    }
    for (uint8_t i = 0; i < NEI_CANE_TYPE_MAX; i++) {
        if (Nei_CaneTypeOwned(i)) {
            return i;
        }
    }
    return 0;
}

extern "C" void Nei_CaneSetType(uint8_t type) {
    if (type < NEI_CANE_TYPE_MAX) {
        Nei_Save()->caneType = type;
    }
}

// ── Sub-selection, which now ONLY the Cane of Somaria has ────────────────────
// These used to assume "2 cane types x 3 skill slots", indexing caneSkillSel[type]
// and computing the skill bit as type*3 + slot. Both broke the moment the wheel
// grew to four entries: caneSkillSel is 2 wide so type 2/3 read and wrote PAST it,
// and type*3 addressed bits 6..11 of a 6-bit mask. That corrupted the active type,
// which is why the wheel showed the wrong icon for an entry.
//
// In the four-entry model only Somaria picks between things (statue / block /
// platform). Trirod, Pacci and Ultrahand each do one thing, so they have no
// sub-selection at all and never touch the array. Slot 0 of caneSkillSel is the
// only one used, so the field's serialized size is unchanged.

// Which of Somaria's three summons are available. Bit 0 (L1) gives the statue;
// bit 1 (L2) gives blocks AND platforms together.
static uint8_t Nei_SomariaSummonOwned(uint8_t slot) {
    uint8_t skills = Nei_Save()->caneSkills;

    switch (slot) {
        case 0:
            return (skills & (1 << 0)) ? 1 : 0; // Statue
        case 1:
        case 2:
            return (skills & (1 << 1)) ? 1 : 0; // Block + Platform, same level
        default:
            return 0;
    }
}

extern "C" uint8_t Nei_CaneGetSkillSlot(uint8_t type) {
    NeiSaveData* nei = Nei_Save();

    if (type != 0) {
        return 0; // only the Cane of Somaria has anything to choose between
    }

    uint8_t slot = nei->caneSkillSel[0];
    if ((slot < 3) && Nei_SomariaSummonOwned(slot)) {
        return slot;
    }
    // Selection points at something not unlocked — snap to the first that is.
    for (uint8_t i = 0; i < 3; i++) {
        if (Nei_SomariaSummonOwned(i)) {
            return i;
        }
    }
    return 0;
}

extern "C" void Nei_CaneSetSkillSlot(uint8_t type, uint8_t slot) {
    if ((type == 0) && (slot < 3) && Nei_SomariaSummonOwned(slot)) {
        Nei_Save()->caneSkillSel[0] = slot;
    }
}

// The CANE_SKILL_* the button would cast right now, derived from WHICH ENTRY is in
// hand — not from arithmetic on the type index.
extern "C" uint8_t Nei_CaneActiveSkill(void) {
    switch (Nei_CaneGetType()) {
        case 1:
            return 7; // Trirod — CANE_SKILL_TRIROD, above CANE_SKILL_MAX (the swing's fire-nothing sentinel)
        case 2:
            return 3; // Cane of Pacci -> Flip (hold adds Lift once that level is owned)
        case 3:
            return 5; // Ultrahand
        case 0:
        default:
            return Nei_CaneGetSkillSlot(0); // Somaria: 0 statue / 1 block / 2 platform
    }
}

// ── Trirod echoes (Somaria L3 = Echoes of Wisdom's Tri Rod) — Skijer's NEI ───────────────────────
// The learned mask is bit-per-row over gTrirodEchoes, split Lo/Hi because the save
// layer has no u64 path. Row indices come validated from the item code; >= 64 is
// simply out of the mask and reads as unlearned.

extern "C" uint8_t Nei_TrirodEchoLearned(uint8_t idx) {
    if (idx >= 64) {
        return 0;
    }
    uint32_t word = (idx < 32) ? Nei_Save()->trirodEchoesLo : Nei_Save()->trirodEchoesHi;
    return (word >> (idx & 31)) & 1;
}

extern "C" void Nei_TrirodLearnEcho(uint8_t idx) {
    if (idx >= 64) {
        return;
    }
    if (idx < 32) {
        Nei_Save()->trirodEchoesLo |= (1u << idx);
    } else {
        Nei_Save()->trirodEchoesHi |= (1u << (idx & 31));
    }
}

extern "C" uint8_t Nei_TrirodLearnedCount(void) {
    uint32_t lo = Nei_Save()->trirodEchoesLo;
    uint32_t hi = Nei_Save()->trirodEchoesHi;
    uint8_t n = 0;

    for (; lo != 0; lo &= lo - 1) {
        n++;
    }
    for (; hi != 0; hi &= hi - 1) {
        n++;
    }
    return n;
}

extern "C" uint8_t Nei_TrirodGetSel(void) {
    return Nei_Save()->trirodSel;
}

extern "C" void Nei_TrirodSetSel(uint8_t idx) {
    Nei_Save()->trirodSel = idx;
}

extern "C" void Nei_TrirodGiveAll(void) {
    // All 64 possible rows; the item code ignores bits past the real table size.
    Nei_Save()->trirodEchoesLo = 0xFFFFFFFFu;
    Nei_Save()->trirodEchoesHi = 0xFFFFFFFFu;
}

extern "C" void Nei_TrirodClear(void) {
    Nei_Save()->trirodEchoesLo = 0;
    Nei_Save()->trirodEchoesHi = 0;
    Nei_Save()->trirodSel = 0;
}

extern "C" uint8_t Nei_TrirodFullList(void) {
    return Nei_Save()->trirodFullList;
}

extern "C" void Nei_TrirodSetFullList(uint8_t on) {
    Nei_Save()->trirodFullList = on ? 1 : 0;
}

extern "C" void Nei_TrirodNotify(const char* msg) {
    Notification::Emit({
        .message = (msg != nullptr) ? msg : "",
    });
}
