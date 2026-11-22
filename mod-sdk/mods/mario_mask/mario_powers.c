#include "mario_powers.h"

#include <string.h>

#include "libsm64/libsm64.h"
#include "mario_body.h"
#include "mario_form.h"
#include "mario_item.h"
#include "mario_page.h"
#include "z64items.h"

#define PUNCH_KEY "mario_mask.punch"
#define CROUCH_KEY "mario_mask.crouch"
#define CAP_MAGIC_COST 12
#define ITEM_TEXTURE(folder, name) "__OTR__textures/" folder "/" name

typedef struct MarioPower {
    const char* key;
    const char* shuffleOption;
    uint32_t capFlag;
    // SM64's own cap durations, in its 30 Hz ticks.
    uint16_t capTicks;
    uint8_t cell;
    const char* iconTexture;
    const char* nameTexture;
    const char* textbox;
    const char* pauseText;
} MarioPower;

static const MarioPower kPowers[] = {
    { "mario_mask.wing_cap", "mario_mask.shuffle_items", SM64_MARIO_WING_CAP, 1800, 1,
      ITEM_TEXTURE("icon_item_custom", "gItemIconMarioWingCapTex"),
      ITEM_TEXTURE("item_name_custom", "gMarioWingCapNameTex"),
      "You got the %rWing Cap%w!^As Mario, press it on %y\xA1%w and then&triple jump to soar through the sky.",
      "%rWing Cap&%wPress %y\xA1%w as Mario to wear it for&some magic. Triple jump to fly." },
    { "mario_mask.metal_cap", "mario_mask.shuffle_items", SM64_MARIO_METAL_CAP, 600, 2,
      ITEM_TEXTURE("icon_item_custom", "gItemIconMarioMetalCapTex"),
      ITEM_TEXTURE("item_name_custom", "gMarioMetalCapNameTex"),
      "You got the %rMetal Cap%w!^As Mario, press it on %y\xA1%w to turn&to heavy, unbreakable metal.",
      "%rMetal Cap&%wPress %y\xA1%w as Mario to turn to&metal for some magic. He sinks." },
    { "mario_mask.vanish_cap", "mario_mask.shuffle_items", SM64_MARIO_VANISH_CAP, 600, 3,
      ITEM_TEXTURE("icon_item_custom", "gItemIconMarioVanishCapTex"),
      ITEM_TEXTURE("item_name_custom", "gMarioVanishCapNameTex"),
      "You got the %rVanish Cap%w!^As Mario, press it on %y\xA1%w to fade&and walk through walls.",
      "%rVanish Cap&%wPress %y\xA1%w as Mario to vanish for&some magic, through walls." },
    { PUNCH_KEY, "mario_mask.shuffle_punch", 0, 0, 6, ITEM_TEXTURE("icon_item_custom", "gItemIconMarioPunchTex"),
      ITEM_TEXTURE("item_name_custom", "gMarioPunchNameTex"),
      "You learned to %rPunch%w!^As Mario, press %y\xA0%w to punch, kick&and dive at your foes.",
      "%rPunch&%wMario punches and kicks with %y\xA0%w." },
    { CROUCH_KEY, "mario_mask.shuffle_crouch", 0, 0, 7, ITEM_TEXTURE("icon_item_custom", "gItemIconMarioCrouchTex"),
      ITEM_TEXTURE("item_name_custom", "gMarioCrouchNameTex"),
      "You learned to %rCrouch%w!^As Mario, hold %y\xA4%w to crouch, long&jump and ground pound.",
      "%rCrouch&%wHold %y\xA4%w as Mario to crouch, long jump&and ground pound." },
};

static const SOHModApi* sApi;
static SOHCustomItemRandomizer sPowerLogic[ARRAY_COUNT(kPowers)];

static bool IsCap(const MarioPower* power) {
    return power->capFlag != 0;
}

static bool CanWearCap(Player* player, PlayState* play) {
    return sApi->IsFormActive(MARIO_FORM_KEY);
}

static void RegisterPower(const MarioPower* power, SOHCustomItemRandomizer* logic) {
    SOHCustomItemDefinition item = Z64Items_Define(power->key, power->iconTexture, power->nameTexture);

    logic->structSize = sizeof(*logic);
    logic->type = SOH_CUSTOM_ITEM_TYPE_ITEM;
    logic->poolCount = 1;
    logic->logicKey = power->key;
    logic->hintKey = power->key;
    logic->progressionGroup = "";
    logic->poolModeOption = power->shuffleOption;
    logic->poolModeValue = 1;
    if (IsCap(power)) {
        Z64Items_SetButtons(&item, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
        item.flags |= SOH_CUSTOM_ITEM_INSTANT;
        Z64Items_SetCanUse(&item, CanWearCap);
    }
    Z64Items_SetTextbox(&item, power->textbox);
    Z64Items_SetPauseText(&item, power->pauseText);
    Z64Items_SetLogic(&item, logic);
    Z64Items_Register(sApi, &item);
    MarioPage_Place(sApi, power->key, power->cell);
}

void MarioPowers_Register(const SOHModApi* api) {
    sApi = api;
    for (int32_t index = 0; index < ARRAY_COUNT(kPowers); index++) {
        RegisterPower(&kPowers[index], &sPowerLogic[index]);
    }
}

static bool IsShuffled(const MarioPower* power) {
    return SOH_MOD_API_HAS(sApi, GetRandoOption) && sApi->GetRandoOption(power->shuffleOption) != 0;
}

void MarioPowers_GrantWithMask(void) {
    if (!sApi->IsCustomItemOwned(MARIO_MASK_KEY)) {
        return;
    }
    for (int32_t index = 0; index < ARRAY_COUNT(kPowers); index++) {
        const MarioPower* power = &kPowers[index];

        if (!IsShuffled(power) && !sApi->IsCustomItemOwned(power->key)) {
            sApi->SetCustomItemOwned(power->key, true);
        }
    }
}

bool MarioPowers_CanPunch(void) {
    return sApi->IsCustomItemOwned(PUNCH_KEY);
}

bool MarioPowers_CanCrouch(void) {
    return sApi->IsCustomItemOwned(CROUCH_KEY);
}

bool MarioPowers_UseCap(PlayState* play, const char* itemKey) {
    for (int32_t index = 0; index < ARRAY_COUNT(kPowers); index++) {
        const MarioPower* power = &kPowers[index];

        if (!IsCap(power) || strcmp(itemKey, power->key) != 0) {
            continue;
        }
        if (!MARIO_CONSUME_MAGIC(play, CAP_MAGIC_COST)) {
            return false;
        }
        MarioBody_PutOnCap(power->capFlag, power->capTicks);
        return true;
    }
    return false;
}
