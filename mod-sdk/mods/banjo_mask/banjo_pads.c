#include "banjo_pads.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "banjo_form.h"
#include "banjo_item.h"
#include "banjo_page.h"
#include "banjo_render.h"
#include "banjo_world.h"
#include "bk_mesh.h"
#include "libbk/src/libbk_port/libbk.h"
#include "z64items.h"

#define SHUFFLE_PADS_OPTION "banjo_mask.shuffle_pads"
#define PAD_COUNT 2
#define PAD_STAND_RADIUS 30.0f
#define PAD_STAND_HEIGHT 12.0f
#define BONE_SCALE_CHANNEL 3

typedef struct BanjoPad {
    const char* key;
    LibBkPad pad;
    uint16_t modelId;
    uint8_t cell;
    const char* iconTexture;
    const char* nameTexture;
    const char* textbox;
    const char* pauseText;
} BanjoPad;

typedef struct PlacedPad {
    bool isPlaced;
    int16_t sceneId;
    Vec3f position;
    BkModel model;
    BkMesh mesh;
} PlacedPad;

static const BanjoPad kPads[PAD_COUNT] = {
    { BANJO_FLIGHT_PAD_KEY, LIBBK_PAD_FLIGHT, 0x48A, 1, "__OTR__textures/icon_item_custom/gItemIconBanjoFlightPadTex",
      "__OTR__textures/item_name_custom/gBanjoFlightPadNameTex",
      "You got a %rFlight Pad%w!^Set it down with %y\xA1%w, stand on it&and press %y\x9F%w to take off.",
      "%rFlight Pad&%wPress %y\xA1%w to set it at Banjo's feet.&Press %y\x9F%w on it to fly." },
    { BANJO_SHOCK_SPRING_PAD_KEY, LIBBK_PAD_SHOCK_SPRING, 0x489, 2,
      "__OTR__textures/icon_item_custom/gItemIconBanjoShockPadTex",
      "__OTR__textures/item_name_custom/gBanjoShockPadNameTex",
      "You got a %rShock Spring Pad%w!^Set it down with %y\xA1%w, stand on it&and press %y\x9F%w to spring up.",
      "%rShock Spring Pad&%wPress %y\xA1%w to set it at Banjo's feet.&Press %y\x9F%w on it to leap." },
};

static const SOHModApi* sApi;
static SOHCustomItemRandomizer sPadLogic[PAD_COUNT];
static PlacedPad sPlaced[PAD_COUNT];

static bool IsPadShuffle(void) {
    return SOH_MOD_API_HAS(sApi, GetRandoOption) && sApi->GetRandoOption(SHUFFLE_PADS_OPTION) != 0;
}

static bool CanPlacePad(Player* player, PlayState* play) {
    return sApi->IsFormActive(BANJO_FORM_KEY) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
}

// Pads sit still, so one mesh in their rest pose serves for the whole stay.
static bool BuildPadMesh(PlacedPad* placed, uint16_t modelId) {
    static BkBoneChannels sRestPose[BK_BONE_MAX];
    static const int16_t kNoSelectors[BK_SELECTOR_COUNT];

    if (placed->mesh.triangleCount > 0) {
        return true;
    }
    if (placed->model.data == NULL && !BkModel_Load(&placed->model, modelId)) {
        return false;
    }

    BkMatrix* bones = calloc(placed->model.boneCount + 1, sizeof(BkMatrix));

    if (bones == NULL) {
        return false;
    }
    for (int32_t bone = 0; bone < BK_BONE_MAX; bone++) {
        for (int32_t axis = 0; axis < 3; axis++) {
            sRestPose[bone].values[BONE_SCALE_CHANNEL + axis] = 1.0f;
        }
    }
    BkModel_ComputeBoneMatrices(&placed->model, sRestPose, bones);
    BkMesh_Build(&placed->mesh, &placed->model, bones, kNoSelectors);
    free(bones);
    return placed->mesh.triangleCount > 0;
}

static void RegisterPad(const BanjoPad* pad, SOHCustomItemRandomizer* logic) {
    SOHCustomItemDefinition item = Z64Items_Define(pad->key, pad->iconTexture, pad->nameTexture);

    logic->structSize = sizeof(*logic);
    logic->type = SOH_CUSTOM_ITEM_TYPE_ITEM;
    logic->poolCount = 1;
    logic->logicKey = pad->key;
    logic->hintKey = pad->key;
    logic->progressionGroup = "";
    logic->poolModeOption = SHUFFLE_PADS_OPTION;
    logic->poolModeValue = 1;
    Z64Items_SetButtons(&item, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    item.flags |= SOH_CUSTOM_ITEM_INSTANT;
    Z64Items_SetTextbox(&item, pad->textbox);
    Z64Items_SetPauseText(&item, pad->pauseText);
    Z64Items_SetCanUse(&item, CanPlacePad);
    Z64Items_SetLogic(&item, logic);
    Z64Items_Register(sApi, &item);
    BanjoPage_Place(sApi, pad->key, pad->cell);
}

void BanjoPads_Register(const SOHModApi* api) {
    sApi = api;
    for (int32_t index = 0; index < PAD_COUNT; index++) {
        RegisterPad(&kPads[index], &sPadLogic[index]);
    }
}

void BanjoPads_GrantWithMask(void) {
    if (IsPadShuffle() || !sApi->IsCustomItemOwned(BANJO_MASK_KEY)) {
        return;
    }
    for (int32_t index = 0; index < PAD_COUNT; index++) {
        if (!sApi->IsCustomItemOwned(kPads[index].key)) {
            sApi->SetCustomItemOwned(kPads[index].key, true);
        }
    }
}

bool BanjoPads_Place(PlayState* play, Player* player, const char* itemKey) {
    for (int32_t index = 0; index < PAD_COUNT; index++) {
        if (strcmp(itemKey, kPads[index].key) != 0) {
            continue;
        }
        if (!CanPlacePad(player, play) || !BuildPadMesh(&sPlaced[index], kPads[index].modelId)) {
            return false;
        }
        sPlaced[index].isPlaced = true;
        sPlaced[index].sceneId = BANJO_SCENE_ID(play);
        sPlaced[index].position = player->actor.world.pos;
        return true;
    }
    return false;
}

static bool IsStandingOn(const PlacedPad* placed, PlayState* play, Player* player) {
    const float dx = player->actor.world.pos.x - placed->position.x;
    const float dz = player->actor.world.pos.z - placed->position.z;
    const float dy = player->actor.world.pos.y - placed->position.y;

    return placed->isPlaced && placed->sceneId == BANJO_SCENE_ID(play) &&
           dx * dx + dz * dz < PAD_STAND_RADIUS * PAD_STAND_RADIUS && fabsf(dy) < PAD_STAND_HEIGHT;
}

void BanjoPads_MarkStanding(PlayState* play, Player* player) {
    for (int32_t index = 0; index < PAD_COUNT; index++) {
        if (IsStandingOn(&sPlaced[index], play, player)) {
            LibBk_SetStandingOnPad(kPads[index].pad);
        }
    }
}

void BanjoPads_Draw(PlayState* play) {
    for (int32_t index = 0; index < PAD_COUNT; index++) {
        const PlacedPad* placed = &sPlaced[index];

        if (!placed->isPlaced || placed->sceneId != BANJO_SCENE_ID(play)) {
            continue;
        }

        const BanjoPlacement placement = { placed->position, { 0.0f, 0.0f, 0.0f }, BANJO_GAME_UNITS_PER_BK_UNIT };

        BanjoRender_DrawMesh(play, &placed->mesh, &placed->model, &placement);
    }
}
