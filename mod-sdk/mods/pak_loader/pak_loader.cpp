#include "soh/ModApi/ModApi.h"
#include "soh/ModApi/ModMenu/ModUI.h"
#include "soh/ResourceManagerHelpers.h"
extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
const char* CVarGetString(const char*, const char*);
void CVarSetString(const char*, const char*);
float CVarGetFloat(const char*, float);
void CVarSetFloat(const char*, float);
void CVarSave(void);
int32_t CVarGetInteger(const char*, int32_t);
void CVarSetInteger(const char*, int32_t);
}
namespace pak {
using Api = SOHModApi;
using UserFile = SOHUserFile;
} // namespace pak
#include "appearance.h"

enum Appearance { APPEARANCE_NONE = -1, APPEARANCE_CHILD, APPEARANCE_ADULT, APPEARANCE_DEITY, APPEARANCE_COUNT };

static const SOHModApi* sApi;
static pak::Library sLibrary;
static pak::Mixer sMixer;
static pak::Composer sComposer;
static pak::EmotePlayer sEmotePlayer;
static pak::ModelPicker sModelPicker;
static bool sOtherPlayer = false;
static bool sEmoteRequested = false;
static pak::ChoiceList sVoiceChoices("Default voice");
static pak::ChoiceList sEmoteChoices("No emote");
static std::vector<pak::EquipSlot> sEquipSlots;
static pak::VoicePack* sVoices[APPEARANCE_COUNT] = {};
static pak::Emote* sEmote = nullptr;
static const char* const sCVars[] = { "gMods.PakLoader.Child", "gMods.PakLoader.Adult", "gMods.PakLoader.Deity" };
static const char* const sScaleCVars[] = { "gMods.PakLoader.ChildRoot", "gMods.PakLoader.AdultRoot",
                                           "gMods.PakLoader.DeityRoot" };
static const pak::AppearanceRow sRows[] = {
    { "Child Link Model", sCVars[0], "gMods.PakLoader.ChildType", sScaleCVars[0], pak::BODY_OOT_CHILD },
    { "Adult Link Model", sCVars[1], "gMods.PakLoader.AdultType", sScaleCVars[1], pak::BODY_OOT_ADULT },
    { "Fierce Deity Model", sCVars[2], "gMods.PakLoader.DeityType", sScaleCVars[2], pak::BODY_MM_DEITY },
};
static const char* const sVoiceMenuCVars[] = { "gMods.PakLoader.VoiceMenu.Child", "gMods.PakLoader.VoiceMenu.Adult",
                                               "gMods.PakLoader.VoiceMenu.Deity" };
static const char* const sVoiceCVars[] = { "gMods.PakLoader.Voice.Child", "gMods.PakLoader.Voice.Adult",
                                           "gMods.PakLoader.Voice.Deity" };
static const char* const sSlotNames[] = { "Child Link", "Adult Link", "Fierce Deity" };

namespace pak {
const char* BaseSkeletonPath(bool childSized) {
    return childSized ? "objects/object_link_child/gLinkChildSkel" : "objects/object_link_boy/gLinkAdultSkel";
}

const char* ResolveKeepTexture(const char* name) {
    static std::set<std::string> paths;
    std::string path = std::string("objects/gameplay_keep/") + name;
    if (!sApi->HasResource(path.c_str())) {
        return nullptr;
    }
    return paths.insert("__OTR__" + path).first->c_str();
}

void Log(const Api* api, const char* message) {
    if (SOH_MOD_API_HAS(api, Log)) {
        api->Log(message);
    }
}

bool CanReadModsFolder(const Api* api) {
    return SOH_MOD_API_HAS(api, ReadModsFolderFiles);
}
} // namespace pak

static bool Has(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

static bool IsLocal(Player* player) {
    return gPlayState != nullptr && player == GET_PLAYER(gPlayState) && !sOtherPlayer;
}

static Appearance ActiveSlot() {
    if (sOtherPlayer || gPlayState == nullptr) {
        return APPEARANCE_NONE;
    }
    const char* form = sApi->GetActiveForm();
    if (form != nullptr && !strcmp(form, "nei.fierce_deity")) {
        return APPEARANCE_DEITY;
    }
    if (form != nullptr && *form) {
        return APPEARANCE_NONE;
    }
    return LINK_IS_ADULT ? APPEARANCE_ADULT : APPEARANCE_CHILD;
}

static pak::Model* Selected(Appearance slot) {
    if (slot == APPEARANCE_NONE || !CVarGetInteger("gMods.PakLoader.Enabled", 1)) {
        return nullptr;
    }
    std::string key = CVarGetString(sCVars[slot], "");
    if (key.empty()) {
        return nullptr;
    }
    pak::Model* model = sLibrary.Load(key);
    if (model == nullptr && sLibrary.Find(key) != nullptr) {
        CVarSetString(sCVars[slot], "");
    }
    return model;
}

static Gfx* LoadVanilla(const std::string& path) {
    return sApi->HasResource(path.c_str()) ? ResourceMgr_LoadGfxByName(path.c_str()) : nullptr;
}

static Gfx* LeftHandEquipment(pak::Model& model, const std::string& original) {
    uint32_t key = Has(original, "Bottle")               ? 0x50A8
                   : Has(original, "Bgs")                ? 0x5458
                   : Has(original, "GiantsKnife")        ? 0x54F0
                   : Has(original, "HoldingKokiriSword") ? 0x5440
                   : Has(original, "HoldingMasterSword") ? 0x5448
                   : Has(original, "Hammer")             ? 0x5460
                   : Has(original, "Closed")             ? 0x50A0
                                                         : 0x5098;
    return pak::FindAlias(model, key);
}

static Gfx* RightHandEquipment(pak::Model& model, const std::string& original) {
    uint32_t key = Has(original, "Mirror")       ? 0x5478
                   : Has(original, "DekuShield") ? 0x5468
                   : Has(original, "Hylian")     ? 0x5470
                   : Has(original, "Bow")        ? 0x5480
                   : Has(original, "Hookshot")   ? 0x5488
                   : Has(original, "Ocarina")    ? 0x5490
                   : Has(original, "Closed")     ? 0x50B8
                                                 : 0x50B0;
    return pak::FindAlias(model, key);
}

static bool IsSheathed(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_16 || player->sheathType == PLAYER_MODELTYPE_SHEATH_18;
}

static bool IsShieldOnBack(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_18 || player->sheathType == PLAYER_MODELTYPE_SHEATH_19;
}

static Gfx* SheathEquipment(pak::Model& model, Player* player) {
    int sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    int shield = player->currentShield;
    bool sheathed = IsSheathed(player);
    bool back = IsShieldOnBack(player);
    bool hasSword = sword >= 1 && sword <= 3;
    bool hasShield = shield >= 1 && shield <= 3;
    if (back && hasShield && sheathed && hasSword) {
        Gfx* combined = pak::FindAlias(model, 0x55C0 + (sword - 1) * 24 + (shield - 1) * 8);
        return combined != nullptr ? combined : pak::FindAlias(model, 0x5400 + (sword - 1) * 24 + (shield - 1) * 8);
    }
    if (back && hasShield) {
        return pak::FindAlias(model, 0x53E8 + (shield - 1) * 8);
    }
    if (hasSword) {
        return pak::FindAlias(model, (sheathed ? 0x53D0 : 0x50C0) + (sword - 1) * 8);
    }
    return nullptr;
}

static Gfx* Equipment(pak::Model& model, Player* player, int limb, const std::string& original) {
    switch (limb) {
        case PLAYER_LIMB_L_HAND:
            return LeftHandEquipment(model, original);
        case PLAYER_LIMB_R_HAND:
            return RightHandEquipment(model, original);
        case PLAYER_LIMB_SHEATH:
            return SheathEquipment(model, player);
        default:
            return nullptr;
    }
}

static bool IsHoldingItem(const std::string& original) {
    static const char* const kHeld[] = { "Holding", "Bgs",      "GiantsKnife", "Bottle",  "Hammer",
                                         "Bow",     "Hookshot", "Slingshot",   "Ocarina", "Boomerang" };
    for (const char* held : kHeld) {
        if (Has(original, held)) {
            return true;
        }
    }
    return false;
}

static bool IsChildList(const std::string& original) {
    return pak::Leaf(original).rfind("gLinkChild", 0) == 0;
}

static std::string VanillaList(const std::string& original, const char* adultLeaf, const char* childLeaf) {
    bool child = IsChildList(original);
    const char* leaf = child ? childLeaf : adultLeaf;
    if (leaf == nullptr) {
        return "";
    }
    return pak::Directory(pak::ResourcePath(original)) + (child ? "gLinkChild" : "gLinkAdult") + leaf;
}

static Gfx* HandBase(pak::Model* model, pak::PieceKind kind, int limb, const std::string& original) {
    uint32_t alias = pak::HandAlias(kind);
    if (model != nullptr && !model->native) {
        Gfx* own = pak::FindAlias(*model, alias);
        return own != nullptr ? own : model->limbs[limb - 1].dLists[0];
    }
    std::string path = alias == pak::kLeftFist ? VanillaList(original, "LeftHandClosedNearDL", "LeftFistNearDL")
                       : alias == pak::kRightFist
                           ? VanillaList(original, "RightHandClosedNearDL", "RightHandClosedNearDL")
                           : VanillaList(original, "RightHandNearDL", "RightHandNearDL");
    if (model != nullptr) {
        std::string native = model->NativeResourceFor(path);
        if (Gfx* list = native.empty() ? nullptr : LoadVanilla(native)) {
            return list;
        }
    }
    return LoadVanilla(path);
}

static Gfx* ComposeHand(pak::Model* model, int limb, const std::string& original) {
    pak::HeldItem item;
    if (!pak::ParseHeldItem(pak::Leaf(original), item) ||
        pak::IsLeftHandItem(item.kind) != (limb == PLAYER_LIMB_L_HAND)) {
        return nullptr;
    }
    pak::Piece* piece = pak::FindEquipped(sEquipSlots, item);
    Gfx* hand = piece != nullptr ? HandBase(model, item.kind, limb, original) : nullptr;
    if (hand == nullptr) {
        return nullptr;
    }
    if (item.kind == pak::PieceKind::Sword) {
        return sComposer.Compose({ { hand }, { piece->hilt }, { piece->blade } });
    }
    return sComposer.Compose({ { hand }, { piece->main } });
}

static const char* ShieldAndSheathLeaf(int shield, bool child) {
    static const char* const kAdult[] = { nullptr, "HylianShieldAndSheathNearDL", "MirrorShieldAndSheathNearDL" };
    static const char* const kChild[] = { "DekuShieldAndSheathNearDL", "HylianShieldAndSheathNearDL", nullptr };
    return (child ? kChild : kAdult)[shield - 1];
}

static const Mtx* ShieldBackMatrix(const pak::Model* body, bool child, int variant) {
    bool ownBackShield = body != nullptr ? pak::IsBackShield(*body, variant) : child && variant == 1;
    int slot = ownBackShield ? 0 : variant;
    if (body != nullptr && body->shieldBack[slot] != nullptr) {
        return body->shieldBack[slot];
    }
    return pak::DefaultShieldBack(child);
}

static Gfx* ComposeSheath(pak::Model* model, Player* player, const std::string& original) {
    int sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    int shield = player->currentShield;
    bool hasSword = sword >= 1 && sword <= 3;
    bool hasShield = IsShieldOnBack(player) && shield >= 1 && shield <= 3;
    pak::Piece* swordPiece = hasSword ? pak::FindEquipped(sEquipSlots, { pak::PieceKind::Sword, sword - 1 }) : nullptr;
    pak::Piece* shieldPiece =
        hasShield ? pak::FindEquipped(sEquipSlots, { pak::PieceKind::Shield, shield - 1 }) : nullptr;
    if (swordPiece == nullptr && shieldPiece == nullptr) {
        return nullptr;
    }
    bool child = IsChildList(original);
    bool sheathed = IsSheathed(player);
    const pak::Model* body = model != nullptr && !model->native ? model : nullptr;
    Gfx* sheath = swordPiece != nullptr ? swordPiece->sheath : nullptr;
    Gfx* hilt = swordPiece != nullptr ? swordPiece->hilt : nullptr;
    Gfx* backShield = shieldPiece != nullptr ? shieldPiece->main : nullptr;
    bool backSpace = shieldPiece != nullptr && shieldPiece->backSpace;
    if (body != nullptr && hasSword) {
        sheath = sheath != nullptr ? sheath : pak::FindAlias(*body, 0x50C0 + (sword - 1) * 8);
        hilt = hilt != nullptr ? hilt : pak::FindAlias(*body, 0x50D8 + (sword - 1) * 8);
    }
    if (body != nullptr && hasShield && backShield == nullptr) {
        backShield = pak::FindAlias(*body, 0x5108 + (shield - 1) * 8);
        backSpace = pak::IsBackShield(*body, shield - 1);
    }
    std::vector<pak::Part> parts;
    if (hasShield && backShield == nullptr) {
        parts.push_back({ LoadVanilla(
            VanillaList(original, ShieldAndSheathLeaf(shield, false), ShieldAndSheathLeaf(shield, true))) });
    } else {
        if (sheath == nullptr) {
            bool vanillaSword = hasSword && sheathed && hilt == nullptr;
            sheath =
                LoadVanilla(vanillaSword ? VanillaList(original, "MasterSwordAndSheathNearDL", "SwordAndSheathNearDL")
                                         : VanillaList(original, "SheathNearDL", "SheathNearDL"));
        }
        parts.push_back({ sheath });
        if (hasShield) {
            parts.push_back({ backShield, backSpace ? nullptr : ShieldBackMatrix(body, child, shield - 1) });
        }
    }
    if (hasSword && sheathed && hilt != nullptr) {
        const Mtx* matrix = body != nullptr && body->swordBack[sword - 1] != nullptr ? body->swordBack[sword - 1]
                                                                                     : pak::DefaultSwordBack(child);
        parts.push_back({ hilt, matrix });
    }
    return sComposer.Compose(parts);
}

static Gfx* ComposeEquipment(pak::Model* model, Player* player, int limb, const std::string& original) {
    if (original.empty()) {
        return nullptr;
    }
    if (limb == PLAYER_LIMB_SHEATH) {
        return ComposeSheath(model, player, original);
    }
    if (limb == PLAYER_LIMB_L_HAND || limb == PLAYER_LIMB_R_HAND) {
        return ComposeHand(model, limb, original);
    }
    return nullptr;
}

static void ScaleRoot(const pak::Model& model, Appearance slot, Vec3f* pos) {
    // The host's FD form applies its own child root multiplier; dividing it out keeps the hooks commutative.
    float correction = (slot == APPEARANCE_DEITY && !LINK_IS_ADULT) ? (3377.f / 2376.f) : 1.f;
    float target = model.rootRatio;
    if (model.child && LINK_IS_ADULT) {
        target *= 0.64f;
    }
    if (!model.child && !LINK_IS_ADULT) {
        target /= 0.64f;
    }
    float factor = target * CVarGetFloat(sScaleCVars[slot], 1.f) / correction;
    pos->x *= factor;
    pos->y *= factor;
    pos->z *= factor;
}

static void PlaceLimb(const pak::Model& model, Appearance slot, int32_t limb, Vec3f* pos) {
    if (pos == nullptr) {
        return;
    }
    if (limb == PLAYER_LIMB_ROOT) {
        ScaleRoot(model, slot, pos);
        return;
    }
    Vec3s joint = model.limbs[limb - 1].jointPos;
    *pos = { float(joint.x), float(joint.y), float(joint.z) };
}

static bool ResolveBody(Player* player, int32_t limb, Gfx** dList, Gfx*, Vec3f* pos) {
    if (!IsLocal(player) || dList == nullptr || limb <= 0 || limb > int32_t(pak::kLimbCount)) {
        return false;
    }
    Appearance slot = ActiveSlot();
    pak::Model* model = Selected(slot);
    bool equipment = slot != APPEARANCE_NONE && slot != APPEARANCE_DEITY && pak::HasEquipped(sEquipSlots);
    if (model == nullptr && !equipment) {
        return false;
    }
    if (model != nullptr) {
        PlaceLimb(*model, slot, limb, pos);
    }
    if (*dList == nullptr) {
        return model != nullptr;
    }
    std::string original;
    if (ResourceMgr_OTRSigCheck(reinterpret_cast<char*>(*dList))) {
        original = reinterpret_cast<const char*>(*dList);
    }
    if (equipment) {
        if (Gfx* composed = ComposeEquipment(model, player, limb, original)) {
            *dList = composed;
            return true;
        }
    }
    if (model == nullptr) {
        return false;
    }
    if (model->native && !original.empty()) {
        std::string path = model->NativeResourceFor(original);
        if (!path.empty() && sApi->HasResource(path.c_str())) {
            *dList = ResourceMgr_LoadGfxByName(path.c_str());
            return true;
        }
    }
    bool equipmentLimb = limb == PLAYER_LIMB_L_HAND || limb == PLAYER_LIMB_R_HAND || limb == PLAYER_LIMB_SHEATH;
    if (equipmentLimb) {
        if (Gfx* equipped = Equipment(*model, player, limb, original)) {
            *dList = equipped;
            return true;
        }
        if (IsHoldingItem(original) || limb == PLAYER_LIMB_SHEATH) {
            return true;
        }
    }
    *dList = model->limbs[limb - 1].dLists[0];
    return true;
}

static void ResolveFaceTexture(const pak::Model& model, const char** texture, bool eyes) {
    static std::set<std::string> paths;
    int index = pak::FaceIndex(*texture, eyes);
    if (!model.native) {
        index = index < 0 ? 0 : index;
        const uint8_t* data = eyes ? model.EyeTexture(index) : model.MouthTexture(index);
        *texture = reinterpret_cast<const char*>(data);
        return;
    }
    if (index < 0 && !model.deity) {
        return;
    }
    std::string path = model.NativeFaceName(index, eyes);
    if (sApi->HasResource(path.c_str())) {
        *texture = paths.insert("__OTR__" + path).first->c_str();
    }
}

static bool ResolveFace(const char** eyes, const char** mouth) {
    pak::Model* model = Selected(ActiveSlot());
    if (model == nullptr || eyes == nullptr || mouth == nullptr) {
        return false;
    }
    ResolveFaceTexture(*model, eyes, true);
    ResolveFaceTexture(*model, mouth, false);
    return true;
}

static bool FdActive() {
    return ActiveSlot() == APPEARANCE_DEITY && Selected(APPEARANCE_DEITY) != nullptr;
}

static bool FdBody(Player* player, int32_t limb, Gfx** dList, Gfx* original, Vec3f* pos) {
    return ActiveSlot() == APPEARANCE_DEITY && ResolveBody(player, limb, dList, original, pos);
}

static bool FdFace(const char** eyes, const char** mouth) {
    return ActiveSlot() == APPEARANCE_DEITY && ResolveFace(eyes, mouth);
}

static void Body(Player* player, int32_t limb, Gfx** dList, Gfx* original, Vec3f* pos) {
    if (ActiveSlot() != APPEARANCE_DEITY) {
        ResolveBody(player, limb, dList, original, pos);
    }
}

static void Face(const char** eyes, const char** mouth) {
    if (ActiveSlot() != APPEARANCE_DEITY) {
        ResolveFace(eyes, mouth);
    }
}

static void ReplaceVoice(Actor* actor, int32_t, uint16_t* sfxId, bool* handled) {
    if (*handled || gPlayState == nullptr || actor != &GET_PLAYER(gPlayState)->actor || sOtherPlayer) {
        return;
    }
    Appearance slot = ActiveSlot();
    pak::VoicePack* voice = slot == APPEARANCE_NONE ? nullptr : sVoices[slot];
    if (voice == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    uint16_t id = *sfxId;
    bool isVoice = id >= 0x6800 && id < 0x6900;
    uint16_t offset = isVoice && player->ageProperties != nullptr ? player->ageProperties->unk_92 : 0;
    uint32_t roll = pak::NextRoll();
    const pak::Pcm* clip = voice->Pick(uint16_t(id - offset), roll);
    if (clip == nullptr) {
        clip = voice->Pick(id, roll);
    }
    if (clip != nullptr) {
        sMixer.Play(clip, 1.f);
        *handled = true;
    }
}

static void MixVoices(int16_t* stereo, uint32_t frames) {
    sMixer.Mix(stereo, frames);
}

static bool WantsToAct(const Input* input) {
    constexpr uint16_t kActions = BTN_A | BTN_B | BTN_Z | BTN_R | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT;
    return (input->press.button & kActions) != 0 || std::abs(input->rel.stick_x) > 20 ||
           std::abs(input->rel.stick_y) > 20;
}

static void UpdateEmote(Player* player) {
    const Input* input = &gPlayState->state.input[0];
    bool shortcut = CHECK_BTN_ALL(input->cur.button, BTN_L) && CHECK_BTN_ALL(input->press.button, BTN_DDOWN);
    if (sEmotePlayer.IsPlaying()) {
        if (WantsToAct(input) || player->actor.speedXZ > 0.5f) {
            sEmotePlayer.Stop();
            return;
        }
        sEmotePlayer.Advance();
        return;
    }
    if (sEmote != nullptr && (shortcut || sEmoteRequested)) {
        sEmotePlayer.Start(sEmote, sMixer, 1.f);
    }
    sEmoteRequested = false;
}

static void Draw(Actor* actor, PlayState* play, bool*) {
    if (actor->id != ACTOR_PLAYER && actor->id != ACTOR_EN_OE2) {
        return;
    }
    sOtherPlayer = actor != &GET_PLAYER(play)->actor;
    if (!sOtherPlayer && sEmotePlayer.IsPlaying()) {
        sEmotePlayer.Pose(reinterpret_cast<Player*>(actor)->skelAnime.jointTable);
    }
}

static void DrawEnd(Actor* actor, PlayState*) {
    if (actor->id == ACTOR_PLAYER || actor->id == ACTOR_EN_OE2) {
        sEmotePlayer.Unpose();
        sOtherPlayer = false;
    }
}

static void Tint(bool* should, va_list) {
    if (Selected(ActiveSlot()) != nullptr) {
        *should = false;
    }
}

static void DrawModelRow(Appearance slot) {
    if (!sModelPicker.Draw(sRows[slot], sLibrary)) {
        return;
    }
    pak::Model* model = Selected(slot);
    sLibrary.status = std::string(sSlotNames[slot]) + ": " +
                      (model != nullptr ? pak::Library::Describe(model->label, model->author) : "default appearance.");
}

static void ChildRow() {
    DrawModelRow(APPEARANCE_CHILD);
}

static void AdultRow() {
    DrawModelRow(APPEARANCE_ADULT);
}

static void DeityRow() {
    DrawModelRow(APPEARANCE_DEITY);
}

static void ResolveVoices() {
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        sVoices[slot] = sLibrary.FindVoice(CVarGetString(sVoiceCVars[slot], ""));
        if (sVoices[slot] != nullptr) {
            sVoices[slot]->Decode();
        }
    }
}

static void VoicesChanged() {
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        sVoiceChoices.Apply(sVoiceMenuCVars[slot], sVoiceCVars[slot]);
    }
    ResolveVoices();
}

static void EquipmentChanged() {
    for (auto& slot : sEquipSlots) {
        slot.selected = sLibrary.FindPiece(slot.choices.Apply(slot.indexCVar.c_str(), slot.keyCVar.c_str()));
    }
}

static void EmoteChanged() {
    sEmote = sLibrary.FindEmote(sEmoteChoices.Apply("gMods.PakLoader.EmoteMenu", "gMods.PakLoader.Emote"));
    sEmotePlayer.Stop();
}

static void ApplyPreset() {
    static const char* const kKeys[] = { "otr:objects/object_link_child/gLinkHumanSkel",
                                         "otr:objects/object_link_boy/gLinkFierceDeitySkel",
                                         "otr:objects/object_link_boy/gLinkAdultSkel" };
    for (const char* key : kKeys) {
        if (sLibrary.Find(key) == nullptr) {
            sLibrary.status = "MM models unavailable. Install mm.o2r through MM Assets and restart Unbound.";
            return;
        }
    }
    CVarSetInteger("gMods.PakLoader.Enabled", 1);
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        CVarSetString(sCVars[slot], kKeys[slot]);
        CVarSetInteger(sRows[slot].typeCVar, pak::BodyTypeOf(*sLibrary.Find(kKeys[slot])));
        CVarSetFloat(sScaleCVars[slot], 1.f);
    }
    CVarSave();
    sLibrary.status = "MM Child / MM Fierce Deity / OoT Adult selected. Fierce Deity keeps its abilities.";
}

static void ResetAll() {
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        CVarSetString(sCVars[slot], "");
        CVarSetInteger(sRows[slot].typeCVar, sRows[slot].defaultType);
        CVarSetString(sVoiceCVars[slot], "");
        CVarSetFloat(sScaleCVars[slot], 1.f);
        sVoiceChoices.Sync(sVoiceMenuCVars[slot], sVoiceCVars[slot]);
        sVoices[slot] = nullptr;
    }
    for (auto& slot : sEquipSlots) {
        CVarSetString(slot.keyCVar.c_str(), "");
        slot.choices.Sync(slot.indexCVar.c_str(), slot.keyCVar.c_str());
        slot.selected = nullptr;
    }
    CVarSave();
    sLibrary.status = "Default models, voices, equipment and root adjustments restored.";
}

static void AppearanceHeading() {
    ModUI_TextWrapped("Custom Player Models");
}

static void EquipmentHeading() {
    ModUI_TextWrapped("Equipment and Items");
}

static void EmoteHeading() {
    ModUI_TextWrapped("Emotes");
}

static void LibraryHeading() {
    ModUI_TextWrapped("Pack Library");
}

static void RestartNote() {
    ModUI_TextWrapped("New files appear in the lists after restarting Unbound.");
}

static void EmoteNote() {
    ModUI_TextWrapped("Hold L and press D-Pad Down to play the emote. Moving or acting stops it.");
}

static void PresetAction() {
    if (ModUI_Button("Apply MM Child / MM FD / OoT Adult", -1, 0)) {
        ApplyPreset();
    }
}

static void ResetAction() {
    if (ModUI_Button("Restore Defaults", -1, 0)) {
        ResetAll();
    }
}

static void PlayEmoteAction() {
    if (ModUI_Button("Play Emote", -1, 0)) {
        sEmoteRequested = true;
    }
}

static void ImportAction() {
    if (ModUI_Button("Import PAK / ZOBJ", -1, 0)) {
        sLibrary.PickAndImport();
    }
}

static void ArchiveAction() {
    if (ModUI_Button("Install O2R / OTR", -1, 0)) {
        sLibrary.InstallArchive();
    }
}

static void StatusRow() {
    ModUI_TextWrapped(sLibrary.status.c_str());
    if (sLibrary.restartNeeded && SOH_MOD_API_HAS(sApi, RequestRestart) &&
        ModUI_Button("Restart to Update Lists", -1, 0)) {
        sApi->RequestRestart("Load the newly installed packs.");
    }
}

static SOHModMenuWidget Widget(SOHModMenuWidgetType type, const char* label, uint8_t column = 0) {
    SOHModMenuWidget widget{};
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "PAK Loader";
    widget.type = type;
    widget.label = label;
    widget.column = column;
    return widget;
}

static void RegisterWidget(SOHModMenuWidget widget) {
    sApi->RegisterMenuWidget(&widget);
}

static void RegisterCustom(const char* label, SOHModMenuCallback draw, uint8_t column = 0) {
    SOHModMenuWidget widget = Widget(SOH_MOD_MENU_CUSTOM, label, column);
    widget.onChange = draw;
    RegisterWidget(widget);
}

static void RegisterCombo(const char* label, const char* cvar, const pak::ChoiceList& choices, const char* tooltip,
                          SOHModMenuCallback changed, uint8_t column) {
    SOHModMenuWidget widget = Widget(SOH_MOD_MENU_COMBOBOX, label, column);
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    widget.values = choices.values.data();
    widget.valueCount = uint32_t(choices.values.size());
    widget.onChange = changed;
    RegisterWidget(widget);
}

static void RegisterAppearanceColumn() {
    RegisterCustom("Custom Player Models", AppearanceHeading);
    RegisterWidget(Widget(SOH_MOD_MENU_SEPARATOR, "Player Models Separator"));
    SOHModMenuWidget enable = Widget(SOH_MOD_MENU_CHECKBOX, "Enable Custom Player Model");
    enable.cvar = "gMods.PakLoader.Enabled";
    enable.defaultInt = 1;
    enable.tooltip = "Replace Link's appearance. Turn off to restore every original model without losing your "
                     "selections.";
    RegisterWidget(enable);
    static const SOHModMenuCallback kRows[] = { ChildRow, AdultRow, DeityRow };
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        RegisterCustom(sRows[slot].label, kRows[slot]);
    }
    RegisterWidget(Widget(SOH_MOD_MENU_SEPARATOR, "Voices Separator"));
    static const char* const kVoiceLabels[] = { "Child Link Voice", "Adult Link Voice", "Fierce Deity Voice" };
    for (int slot = 0; slot < APPEARANCE_COUNT; ++slot) {
        sVoiceChoices.Sync(sVoiceMenuCVars[slot], sVoiceCVars[slot]);
        RegisterCombo(kVoiceLabels[slot], sVoiceMenuCVars[slot], sVoiceChoices,
                      "Replace Link's voice clips with a ModLoader64 voice pack.", VoicesChanged, 0);
    }
    SOHModMenuWidget volume = Widget(SOH_MOD_MENU_INT_SLIDER, "Voice Pack Volume");
    volume.cvar = "gMods.PakLoader.VoiceVolume";
    volume.defaultInt = 100;
    volume.minInt = 0;
    volume.maxInt = 100;
    volume.tooltip = "Volume of voice pack and emote sounds, on top of the sound effects volume.";
    RegisterWidget(volume);
    RegisterCustom("MM Model Preset", PresetAction);
    RegisterCustom("Restore Defaults", ResetAction);
}

static void RegisterEquipmentColumn() {
    static const std::pair<pak::HeldItem, const char*> kSlots[] = {
        { { pak::PieceKind::Sword, 0 }, "Kokiri Sword" },
        { { pak::PieceKind::Sword, 1 }, "Master Sword" },
        { { pak::PieceKind::Sword, 2 }, "Biggoron Sword" },
        { { pak::PieceKind::Shield, 0 }, "Deku Shield" },
        { { pak::PieceKind::Shield, 1 }, "Hylian Shield" },
        { { pak::PieceKind::Shield, 2 }, "Mirror Shield" },
        { { pak::PieceKind::Bow, 0 }, "Bow" },
        { { pak::PieceKind::Hookshot, 0 }, "Hookshot" },
        { { pak::PieceKind::Slingshot, 0 }, "Slingshot" },
        { { pak::PieceKind::Boomerang, 0 }, "Boomerang" },
        { { pak::PieceKind::Hammer, 0 }, "Megaton Hammer" },
        { { pak::PieceKind::FairyOcarina, 0 }, "Fairy Ocarina" },
        { { pak::PieceKind::Ocarina, 0 }, "Ocarina of Time" },
    };
    sEquipSlots.clear();
    sEquipSlots.reserve(std::size(kSlots));
    for (const auto& [item, label] : kSlots) {
        sEquipSlots.push_back({ item.kind, item.variant, label });
    }
    RegisterCustom("Equipment and Items", EquipmentHeading, 1);
    RegisterWidget(Widget(SOH_MOD_MENU_SEPARATOR, "Equipment Separator", 1));
    for (auto& slot : sEquipSlots) {
        pak::BuildEquipSlot(slot, sLibrary);
        RegisterCombo(slot.label, slot.indexCVar.c_str(), slot.choices,
                      "Draw this item from any imported pack. Pieces of the same kind fit every slot.",
                      EquipmentChanged, 1);
    }
    RegisterCustom("Emotes", EmoteHeading, 1);
    RegisterWidget(Widget(SOH_MOD_MENU_SEPARATOR, "Emote Separator", 1));
    sEmoteChoices.Sync("gMods.PakLoader.EmoteMenu", "gMods.PakLoader.Emote");
    RegisterCombo("Emote", "gMods.PakLoader.EmoteMenu", sEmoteChoices, "Animation played with the emote shortcut.",
                  EmoteChanged, 1);
    RegisterCustom("Play Emote", PlayEmoteAction, 1);
    RegisterCustom("Emote Shortcut", EmoteNote, 1);
}

static void RegisterLibraryColumn() {
    RegisterCustom("Pack Library", LibraryHeading, 1);
    RegisterWidget(Widget(SOH_MOD_MENU_SEPARATOR, "Pack Library Separator", 1));
    RegisterCustom("Import Model File", ImportAction, 1);
    RegisterCustom("Install Model Archive", ArchiveAction, 1);
    RegisterCustom("Restart Note", RestartNote, 1);
    RegisterCustom("Pack Library Status", StatusRow, 1);
}

static void BuildChoices() {
    sModelPicker.Build(sLibrary);
    for (const auto& voice : sLibrary.voices) {
        sVoiceChoices.Add(voice->key, pak::Library::Describe(voice->label, voice->author));
    }
    for (const auto& emote : sLibrary.emotes) {
        sEmoteChoices.Add(emote->key, pak::Library::Describe(emote->label, emote->author));
    }
    sVoiceChoices.Finish();
    sEmoteChoices.Finish();
}

static void RegisterMenu() {
    sLibrary.Scan();
    BuildChoices();
    ResolveVoices();
    sEmote = sLibrary.FindEmote(CVarGetString("gMods.PakLoader.Emote", ""));
    sApi->RegisterMenuSidebar("Mods", "PAK Loader", "gMods.Menu.Page", 2);
    RegisterAppearanceColumn();
    RegisterEquipmentColumn();
    RegisterLibraryColumn();
}

static void Tick() {
    if (!sLibrary.scanned) {
        sLibrary.Scan();
    }
    if (gPlayState != nullptr) {
        UpdateEmote(GET_PLAYER(gPlayState));
    }
}

static const char* const sHooks[] = {
    "OnPlayerResolveLimbDraw", "OnPlayerResolveFaceTextures", "OnActorDraw", "OnActorDrawEnd", "OnPlayerUpdate",
    "OnActorPlaySfx"
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, 6 };

extern "C" SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
    sLibrary.Bind(api);
}

extern "C" SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements() {
    return &sRequirements;
}

extern "C" SOH_MOD_EXPORT void ModInit() {
    if (!SOH_MOD_API_HAS(sApi, ListArchiveFiles) || !SOH_MOD_API_HAS(sApi, PublishService) ||
        !SOH_MOD_API_HAS(sApi, GetActiveForm) || !SOH_MOD_API_HAS(sApi, HasResource) ||
        !SOH_MOD_API_HAS(sApi, RegisterMenuSidebar)) {
        return;
    }
    sApi->PublishService("nei.pak_loader.fd_body.v1", reinterpret_cast<void*>(FdBody));
    sApi->PublishService("nei.pak_loader.fd_face.v1", reinterpret_cast<void*>(FdFace));
    sApi->PublishService("nei.pak_loader.fd_active.v1", reinterpret_cast<void*>(FdActive));
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, Body);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveFaceTextures, Face);
    SOH_REGISTER_HOOK(sApi, OnActorDraw, Draw);
    SOH_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawEnd);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, Tick);
    SOH_REGISTER_HOOK(sApi, OnActorPlaySfx, ReplaceVoice);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, Tint);
    if (SOH_MOD_API_HAS(sApi, RegisterAudioMixInGroup)) {
        sApi->RegisterAudioMixInGroup(MixVoices, SOH_AUDIO_GROUP_SFX, "gMods.PakLoader.VoiceVolume");
    }
    RegisterMenu();
}
