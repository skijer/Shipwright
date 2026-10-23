#include <math.h>
#include <stdio.h>
#include <string.h>
#include "z64items.h"
#include "z64wheel.h"
#include "easy_equipment_input.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "textures/icon_item_static/icon_item_static.h"

#define EASY_OWNER "nei.easy_equipment"
#define EASY_MAX_PIECES 128
#define EASY_KEY_SIZE 256
#define EASY_TAU 6.28318531f
#define EASY_VANILLA_COUNT 9

// A vanilla shield/tunic/boot (equipKey NULL) or a custom one, which rides C as this mod's hidden holder item.
typedef struct {
    const char* equipKey;
    char holderKey[EASY_KEY_SIZE];
    const char* icon;
    uint8_t item;
    uint8_t buttonSlot;
    uint8_t slot;
    uint8_t value;
    uint8_t age;
} EasyPiece;

static const EasyPiece sVanilla[EASY_VANILLA_COUNT] = {
    { NULL, "", gItemIconShieldDekuTex, ITEM_SHIELD_DEKU, SLOT_SHIELD_DEKU, 1, 1, SOH_CUSTOM_ITEM_AGE_CHILD },
    { NULL, "", gItemIconShieldHylianTex, ITEM_SHIELD_HYLIAN, SLOT_SHIELD_HYLIAN, 1, 2, SOH_CUSTOM_ITEM_AGE_ANY },
    { NULL, "", gItemIconShieldMirrorTex, ITEM_SHIELD_MIRROR, SLOT_SHIELD_MIRROR, 1, 3, SOH_CUSTOM_ITEM_AGE_ADULT },
    { NULL, "", gItemIconTunicKokiriTex, ITEM_TUNIC_KOKIRI, SLOT_TUNIC_KOKIRI, 2, 1, SOH_CUSTOM_ITEM_AGE_ANY },
    { NULL, "", gItemIconTunicGoronTex, ITEM_TUNIC_GORON, SLOT_TUNIC_GORON, 2, 2, SOH_CUSTOM_ITEM_AGE_ADULT },
    { NULL, "", gItemIconTunicZoraTex, ITEM_TUNIC_ZORA, SLOT_TUNIC_ZORA, 2, 3, SOH_CUSTOM_ITEM_AGE_ADULT },
    { NULL, "", gItemIconBootsKokiriTex, ITEM_BOOTS_KOKIRI, SLOT_BOOTS_KOKIRI, 3, 1, SOH_CUSTOM_ITEM_AGE_ANY },
    { NULL, "", gItemIconBootsIronTex, ITEM_BOOTS_IRON, SLOT_BOOTS_IRON, 3, 2, SOH_CUSTOM_ITEM_AGE_ADULT },
    { NULL, "", gItemIconBootsHoverTex, ITEM_BOOTS_HOVER, SLOT_BOOTS_HOVER, 3, 3, SOH_CUSTOM_ITEM_AGE_ADULT },
};

static const SOHModApi* sApi;
static EasyPiece sPieces[EASY_MAX_PIECES];
static uint32_t sPieceCount;
static uint32_t sKnownCustomCount;
static uint32_t sChoices[EASY_MAX_PIECES];
static uint32_t sChoiceCount;
static uint32_t sHighlight;
static int32_t sPressedPiece = -1;
static uint8_t sPressedButton;
static EasyHold sHold;
static const uint16_t sCButtons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
static const char* const sHooks[] = { "OnGameFrameUpdate", "OnPlayerFilterInput", "OnInterfaceDrawEnd", "OnSceneInit",
                                      "OnLoadFile" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, ARRAY_COUNT(sHooks) };

static bool IsOwned(const EasyPiece* piece) {
    if (piece->equipKey != NULL) {
        return sApi->IsEquipmentOwned(piece->equipKey);
    }
    return CHECK_OWNED_EQUIP(piece->slot, piece->value - 1) != 0;
}

static bool IsAgeAllowed(const EasyPiece* piece) {
    if (piece->equipKey != NULL) {
        return CustomEquipRegistry_IsAgeAllowed(piece->equipKey);
    }
    if (CVarGetInteger("gCheats.TimelessEquipment", 0)) {
        return true;
    }
    return piece->age == SOH_CUSTOM_ITEM_AGE_ANY ||
           (piece->age == SOH_CUSTOM_ITEM_AGE_CHILD) == (gSaveContext.linkAge == LINK_AGE_CHILD);
}

static bool IsWorn(const EasyPiece* piece) {
    if (piece->equipKey != NULL) {
        return sApi->IsEquipmentWorn(piece->equipKey);
    }
    return sApi->GetWornEquipment(piece->slot) == NULL && CUR_EQUIP_VALUE(piece->slot) == piece->value;
}

static bool RegisterHolderItem(EasyPiece* piece, const SOHCustomEquipDefinition* equip) {
    int length = snprintf(piece->holderKey, sizeof(piece->holderKey), EASY_OWNER ".%s", equip->key);
    if (length < 0 || (size_t)length >= sizeof(piece->holderKey)) {
        return false;
    }
    SOHCustomItemDefinition item = Z64Items_Define(piece->holderKey, equip->iconPath, equip->namePath);
    Z64Items_SetButtons(&item, SOH_CUSTOM_ITEM_C_BUTTON);
    Z64Items_SetAge(&item, equip->ageRequirement);
    item.flags |= SOH_CUSTOM_ITEM_INSTANT;
    item.presentationFlags |= SOH_ITEM_HIDE_FROM_KALEIDO | SOH_ITEM_HIDE_FROM_SAVE_EDITOR;
    return Z64Items_Register(sApi, &item);
}

// Custom equipment registers during every mod's init, so it is picked up once the registry stops growing.
static void DiscoverCustomPieces(void) {
    uint32_t count = CustomEquipRegistry_GetCount();
    for (uint32_t i = sKnownCustomCount; i < count && sPieceCount < EASY_MAX_PIECES; ++i) {
        const SOHCustomEquipDefinition* equip = CustomEquipRegistry_GetAt(i);
        if (equip == NULL || equip->slot == SOH_EQUIP_SLOT_SWORD || equip->slot >= SOH_EQUIP_SLOT_UPGRADE) {
            continue;
        }
        EasyPiece* piece = &sPieces[sPieceCount];
        piece->equipKey = equip->key;
        piece->icon = equip->iconPath;
        piece->item = ITEM_CUSTOM;
        piece->buttonSlot = SLOT_NONE;
        piece->slot = equip->slot;
        piece->value = 0;
        piece->age = equip->ageRequirement;
        if (RegisterHolderItem(piece, equip)) {
            ++sPieceCount;
        }
    }
    sKnownCustomCount = count;
}

// The holder items mirror ownership so the C button accepts them; they never grant the equipment.
static void SyncHolderOwnership(void) {
    for (uint32_t i = EASY_VANILLA_COUNT; i < sPieceCount; ++i) {
        bool isOwned = IsOwned(&sPieces[i]);
        if (sApi->IsCustomItemOwned(sPieces[i].holderKey) != isOwned) {
            sApi->SetCustomItemOwned(sPieces[i].holderKey, isOwned);
        }
    }
}

static int32_t FindPieceOnButton(uint8_t button) {
    uint8_t item = gSaveContext.equips.buttonItems[button];
    const char* key = item == ITEM_CUSTOM ? sApi->GetEquippedCustomItem(button) : NULL;
    for (uint32_t i = 0; i < sPieceCount; ++i) {
        bool isMatch = key != NULL ? sPieces[i].equipKey != NULL && strcmp(key, sPieces[i].holderKey) == 0
                                   : sPieces[i].equipKey == NULL && sPieces[i].item == item;
        if (isMatch) {
            return (int32_t)i;
        }
    }
    return -1;
}

static uint16_t GetBareValue(uint8_t slot) {
    if (slot == EQUIP_TYPE_TUNIC) {
        return EQUIP_VALUE_TUNIC_KOKIRI;
    }
    if (slot == EQUIP_TYPE_BOOTS) {
        return EQUIP_VALUE_BOOTS_KOKIRI;
    }
    return EQUIP_VALUE_SHIELD_NONE;
}

// TakeOff runs the outgoing custom piece's onUnequip before the vanilla value is written.
static void SetVanillaValue(uint8_t slot, uint16_t value) {
    sApi->TakeOffEquipment(slot);
    Inventory_ChangeEquipment(slot, value);
}

static bool WearPiece(const EasyPiece* piece) {
    if (!IsOwned(piece) || !IsAgeAllowed(piece)) {
        return false;
    }
    if (piece->equipKey != NULL) {
        return sApi->WearEquipment(piece->equipKey);
    }
    SetVanillaValue(piece->slot, piece->value);
    return true;
}

static bool TogglePiece(const EasyPiece* piece) {
    if (!IsWorn(piece)) {
        return WearPiece(piece);
    }
    SetVanillaValue(piece->slot, GetBareValue(piece->slot));
    return true;
}

static void AssignToButton(const EasyPiece* piece, uint8_t button) {
    if (piece->equipKey != NULL) {
        sApi->EquipCustomItem(button, piece->holderKey);
        return;
    }
    if (gSaveContext.equips.buttonItems[button] == ITEM_CUSTOM) {
        sApi->UnequipCustomItem(button);
    }
    for (uint8_t other = 1; other <= ARRAY_COUNT(sCButtons); ++other) {
        if (other != button && gSaveContext.equips.buttonItems[other] == piece->item) {
            gSaveContext.equips.buttonItems[other] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[other - 1] = SLOT_NONE;
            Interface_LoadItemIcon1(gPlayState, other);
        }
    }
    gSaveContext.equips.buttonItems[button] = piece->item;
    gSaveContext.equips.cButtonSlots[button - 1] = piece->buttonSlot;
    Interface_LoadItemIcon1(gPlayState, button);
}

static bool IsBusy(PlayState* play) {
    if (play == NULL || play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE) {
        return true;
    }
    return (GET_PLAYER(play)->stateFlags1 &
            (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING | PLAYER_STATE1_GETTING_ITEM |
             PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_TALKING | PLAYER_STATE1_ON_HORSE)) != 0;
}

static void FinishChange(bool isChanged) {
    if (isChanged) {
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    }
    Z64Wheel_PlaySfx(isChanged ? NA_SE_SY_DECIDE : NA_SE_SY_ERROR);
}

static void ReleaseWheel(void) {
    sApi->ReleasePlayerInput(EASY_OWNER);
    sApi->ReleaseTimeControl(EASY_OWNER);
    sChoiceCount = 0;
}

static void OpenWheel(void) {
    const EasyPiece* pressed = &sPieces[sPressedPiece];
    sChoiceCount = 0;
    sHighlight = 0;
    for (uint32_t i = 0; i < sPieceCount; ++i) {
        if (sPieces[i].slot != pressed->slot || !IsOwned(&sPieces[i])) {
            continue;
        }
        if ((int32_t)i == sPressedPiece) {
            sHighlight = sChoiceCount;
        }
        sChoices[sChoiceCount++] = i;
    }
    sApi->BlockPlayerInput(EASY_OWNER, Z64WHEEL_BLOCKED_BUTTONS, true);
    sApi->RequestTimeControl(EASY_OWNER, 1000, 0.0f, true);
    Z64Wheel_PlaySfx(NA_SE_SY_WIN_OPEN);
}

static void CommitWheel(void) {
    bool isChanged = false;
    if (sChoiceCount != 0) {
        const EasyPiece* chosen = &sPieces[sChoices[sHighlight]];
        isChanged = IsWorn(chosen) || WearPiece(chosen);
        if (isChanged) {
            AssignToButton(chosen, sPressedButton + 1);
        }
    }
    FinishChange(isChanged);
    ReleaseWheel();
}

// Multiple concentric rings keep every owned piece available even in a larger mod collection.
static void SelectUnderStick(Input* input) {
    float x = input->cur.stick_x;
    float y = input->cur.stick_y;
    float magnitude = sqrtf(x * x + y * y);
    if (magnitude < 20.0f || sChoiceCount == 0) {
        return;
    }
    uint32_t rings = (sChoiceCount + 11) / 12;
    uint32_t ring = rings == 1 ? 0 : (uint32_t)((magnitude - 20.0f) * rings / 60.0f);
    if (ring >= rings) {
        ring = rings - 1;
    }
    uint32_t first = ring * 12;
    uint32_t count = MIN(12u, sChoiceCount - first);
    int32_t slot = (int32_t)lroundf(atan2f(x, y) * count / EASY_TAU) % (int32_t)count;
    if (slot < 0) {
        slot += count;
    }
    uint32_t selection = first + slot;
    if (selection != sHighlight) {
        sHighlight = selection;
        Z64Wheel_PlaySfx(NA_SE_SY_CURSOR);
    }
}

static void ResetGesture(void) {
    ReleaseWheel();
    sPressedPiece = -1;
    memset(&sHold, 0, sizeof(sHold));
}

static bool BeginGesture(PlayState* play, Input* input) {
    if (IsBusy(play)) {
        return false;
    }
    for (uint8_t c = 0; c < ARRAY_COUNT(sCButtons); ++c) {
        if (!(input->press.button & sCButtons[c])) {
            continue;
        }
        int32_t index = FindPieceOnButton(c + 1);
        if (index >= 0 && IsOwned(&sPieces[index])) {
            sPressedPiece = index;
            sPressedButton = c;
            memset(&sHold, 0, sizeof(sHold));
            return true;
        }
    }
    return false;
}

static void Tick(void) {
    DiscoverCustomPieces();
    PlayState* play = gPlayState;
    if (play == NULL) {
        ResetGesture();
        return;
    }
    SyncHolderOwnership();
    Input* input = &play->state.input[0];
    if (sPressedPiece < 0 && !BeginGesture(play, input)) {
        return;
    }
    bool isDown = (input->cur.button & sCButtons[sPressedButton]) != 0;
    bool isAllowed = !IsBusy(play) && IsOwned(&sPieces[sPressedPiece]);
    bool isCancelled = (input->press.button & (BTN_B | BTN_START)) != 0;
    if (sHold.open && isAllowed) {
        SelectUnderStick(input);
    }
    switch (EasyHold_UpdateMode(&sHold, isDown, isCancelled, isAllowed,
                               CVarGetInteger("gMods.EasyEquipment.WheelMode",0)==1)) {
        case EASY_OPEN:
            OpenWheel();
            break;
        case EASY_TAP:
            FinishChange(TogglePiece(&sPieces[sPressedPiece]));
            break;
        case EASY_COMMIT:
            CommitWheel();
            break;
        case EASY_CANCEL:
            Z64Wheel_PlaySfx(NA_SE_SY_CANCEL);
            ReleaseWheel();
            break;
        default:
            break;
    }
    if (!isDown && !sHold.waitRelease) {
        sPressedPiece = -1;
    }
}

// Assignable Tunics and Boots would toggle the same press; this mod owns those buttons.
static void FilterInput(Player* player, Input* input) {
    if (IsBusy(gPlayState)) {
        return;
    }
    for (uint8_t c = 0; c < ARRAY_COUNT(sCButtons); ++c) {
        if (FindPieceOnButton(c + 1) >= 0) {
            input->cur.button &= ~sCButtons[c];
            input->press.button &= ~sCButtons[c];
            input->rel.button &= ~sCButtons[c];
        }
    }
    if (sPressedPiece >= 0) {
        // The hold gesture never attacks/rolls, including its release/cancellation frame.
        input->cur.button &= ~(BTN_B | BTN_A);
        input->press.button &= ~(BTN_B | BTN_A);
    }
}

static void DrawWheel(PlayState* play) {
    if (!sHold.open || sChoiceCount == 0 || play == NULL) {
        return;
    }
    Z64Wheel_DimScreen(play, 110);
    uint32_t rings = (sChoiceCount + 11) / 12;
    for (uint32_t i = 0; i < sChoiceCount; ++i) {
        uint32_t ring = i / 12;
        uint32_t first = ring * 12;
        uint32_t count = MIN(12u, sChoiceCount - first);
        float angle = EASY_TAU * (i - first) / count;
        float radius = rings == 1 ? 56.0f : 32.0f + 64.0f * ring / (rings - 1);
        bool isSelected = i == sHighlight;
        int16_t size = isSelected ? 32 : 24;
        const EasyPiece* piece = &sPieces[sChoices[i]];
        Z64Wheel_DrawIcon(play, piece->icon, (int16_t)(SCREEN_WIDTH / 2 + sinf(angle) * radius - size / 2),
                          (int16_t)(SCREEN_HEIGHT / 2 - cosf(angle) * radius - size / 2), size,
                          IsAgeAllowed(piece) ? 255 : 96, isSelected ? 255 : 180);
    }
}

static void OnScene(int16_t sceneNum) {
    ResetGesture();
}

static void OnLoad(int32_t fileNum) {
    ResetGesture();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, WearEquipment) || !SOH_MOD_API_HAS(sApi, TakeOffEquipment) ||
        !SOH_MOD_API_HAS(sApi, UnequipCustomItem) || !SOH_MOD_API_HAS(sApi, SetCustomItemOwned) ||
        !SOH_MOD_API_HAS(sApi, GetEquippedCustomItem) || !SOH_MOD_API_HAS(sApi, ReleaseTimeControl)) {
        return;
    }
    memcpy(sPieces, sVanilla, sizeof(sVanilla));
    sPieceCount = EASY_VANILLA_COUNT;
    if (SOH_MOD_API_HAS(sApi,RegisterMenuSidebar) && SOH_MOD_API_HAS(sApi,RegisterMenuWidget)) {
        static const char* modes[]={"Hold","Instant"};
        sApi->RegisterMenuSidebar("Enhancements","Easy Equipment","gMods.Menu",1);
        SOHModMenuWidget w={0};w.structSize=sizeof(w);w.section="Enhancements";w.sidebar="Easy Equipment";
        w.type=SOH_MOD_MENU_COMBOBOX;w.label="Open equipment wheel";w.cvar="gMods.EasyEquipment.WheelMode";
        w.values=modes;w.valueCount=ARRAY_COUNT(modes);w.defaultInt=0;
        w.tooltip="Hold: tap toggles equipment, hold opens the wheel. Instant: press opens the wheel immediately. Release C to equip the highlighted piece; B cancels.";
        sApi->RegisterMenuWidget(&w);
    }
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
    SOH_REGISTER_HOOK(sApi, OnPlayerFilterInput, FilterInput);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawWheel);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, OnScene);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, OnLoad);
}
