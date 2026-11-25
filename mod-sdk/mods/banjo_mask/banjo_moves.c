#include "banjo_moves.h"

#include "banjo_animation_ids.h"
#include "banjo_body.h"
#include "banjo_item.h"
#include "banjo_page.h"
#include "banjo_render.h"
#include "libbk/src/libbk_port/libbk.h"
#include "z64items.h"

#define SHUFFLE_MOVES_OPTION "banjo_mask.shuffle_moves"
#define FIRST_MOVE_CELL 6
#define MOVE_TEXTURE(prefix, name) "__OTR__textures/" prefix name

typedef struct BanjoMove {
    const char* key;
    LibBkAbility ability;
    const char* iconTexture;
    const char* nameTexture;
    const char* textbox;
    const char* pauseText;
    void (*drawGetItem)(PlayState* play, BANJO_GET_ITEM_ENTRY* entry);
} BanjoMove;

static const LibBkAbility kSpiralMountainMoves[] = {
    LIBBK_ABILITY_BEAK_BARGE,    LIBBK_ABILITY_CAMERA_CONTROL, LIBBK_ABILITY_CLAW_SWIPE,
    LIBBK_ABILITY_CLIMB,         LIBBK_ABILITY_FEATHERY_FLAP,  LIBBK_ABILITY_FLAP_FLIP,
    LIBBK_ABILITY_JUMP_HIGHER,   LIBBK_ABILITY_RAT_A_TAT_RAP,  LIBBK_ABILITY_ROLL,
    LIBBK_ABILITY_DIVE,
};

// Each move's get-item is a small Banjo playing that move, Kazooie out.
#define MOVE_FIGURE(name, animation)                                       \
    static void Draw##name(PlayState* play, BANJO_GET_ITEM_ENTRY* entry) { \
        BanjoBody_DrawFigure(play, animation, true);                           \
    }

// A move BK ties to an object shows that object instead: its pad or its shoes.
#define MOVE_PROP(name, modelId)                                           \
    static void Draw##name(PlayState* play, BANJO_GET_ITEM_ENTRY* entry) { \
        static BanjoProp sProp = { modelId };                              \
        BanjoRender_DrawPropGetItem(play, &sProp);                         \
    }

// A move that spends a collectible shows that collectible, animated as in BK.
#define MOVE_SPRITE(name, assetId)                                         \
    static void Draw##name(PlayState* play, BANJO_GET_ITEM_ENTRY* entry) { \
        static BanjoSprite sSprite = { assetId };                          \
        BanjoRender_DrawSpriteGetItem(play, &sSprite);                     \
    }

MOVE_FIGURE(TalonTrot, BANJO_ANIM_TROT_WALK)
MOVE_FIGURE(BeakBuster, BANJO_ANIM_BEAK_BUSTER)
MOVE_SPRITE(Eggs, BANJO_SPRITE_BLUE_EGG)
MOVE_PROP(ShockSpringJump, BANJO_MODEL_SHOCK_SPRING_PAD)
MOVE_PROP(Flight, BANJO_MODEL_FLIGHT_PAD)
MOVE_SPRITE(Wonderwing, BANJO_SPRITE_GOLD_FEATHER)
MOVE_PROP(StiltStride, BANJO_MODEL_WADING_BOOTS)
MOVE_PROP(TurboTalonTrot, BANJO_MODEL_TURBO_TRAINERS)
MOVE_SPRITE(BeakBomb, BANJO_SPRITE_RED_FEATHER)

static const BanjoMove kMoves[] = {
    { "banjo_mask.move.talon_trot", LIBBK_ABILITY_TALON_TROT,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoTalonTrotTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoTalonTrotNameTex"),
      "You learned the %rTalon Trot%w!^Hold %y\xA4%w and press %y\xA1%w so Kazooie&carries Banjo up steep slopes.",
      "%rTalon Trot&%wHold %y\xA4%w and press %y\xA1%w to run on&Kazooie's legs.", DrawTalonTrot },
    { "banjo_mask.move.beak_buster", LIBBK_ABILITY_BEAK_BUSTER,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoBeakBusterTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoBeakBusterNameTex"),
      "You learned the %rBeak Buster%w!^Press %y\xA4%w in the air to slam&Kazooie's beak into the ground.",
      "%rBeak Buster&%wPress %y\xA4%w in the air to drive&Kazooie's beak down.", DrawBeakBuster },
    { "banjo_mask.move.eggs", LIBBK_ABILITY_EGGS, MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoEggsTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoEggsNameTex"),
      "You learned %rEgg Firing%w!^Hold %y\xA4%w and press %y\xA1%w to shoot&eggs forwards or backwards.",
      "%rEgg Firing&%wHold %y\xA4%w and press %y\xA1%w to fire&Kazooie's eggs.", DrawEggs },
    { "banjo_mask.move.shock_spring_jump", LIBBK_ABILITY_SHOCK_SPRING_JUMP,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoShockJumpTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoShockJumpNameTex"),
      "You learned the %rShock Spring Jump%w!^Stand on a %yShock Spring Pad%w and&press %y\x9F%w to soar upwards.",
      "%rShock Spring Jump&%wPress %y\x9F%w on a Shock Spring Pad&to leap high.", DrawShockSpringJump },
    { "banjo_mask.move.flight", LIBBK_ABILITY_FLIGHT, MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoFlightTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoFlightNameTex"),
      "You learned to %rFly%w!^Stand on a %yFlight Pad%w and press %y\x9F%w&to take off with red feathers.",
      "%rFlight&%wPress %y\x9F%w on a Flight Pad to fly.&Each flap spends a red feather.", DrawFlight },
    { "banjo_mask.move.wonderwing", LIBBK_ABILITY_WONDERWING,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoWonderwingTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoWonderwingNameTex"),
      "You learned the %rWonderwing%w!^Hold %y\xA4%w and press %y\xA1%w to shine&with a gold feather's power.",
      "%rWonderwing&%wHold %y\xA4%w and press %y\xA1%w to become&invulnerable with gold feathers.", DrawWonderwing },
    { "banjo_mask.move.stilt_stride", LIBBK_ABILITY_STILT_STRIDE,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoStiltStrideTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoStiltStrideNameTex"),
      "You learned the %rStilt Stride%w!^With %yWading Boots%w Kazooie wades&through what would hurt Banjo.",
      "%rStilt Stride&%wWading Boots let Kazooie walk&over harmful ground.", DrawStiltStride },
    { "banjo_mask.move.turbo_talon_trot", LIBBK_ABILITY_TURBO_TALON_TROT,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoTurboTalonTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoTurboTalonNameTex"),
      "You learned the %rTurbo Talon Trot%w!^%yTurbo Trainers%w make Kazooie run&faster than anything.",
      "%rTurbo Talon Trot&%wTurbo Trainers send Kazooie&racing ahead.", DrawTurboTalonTrot },
    { "banjo_mask.move.beak_bomb", LIBBK_ABILITY_BEAK_BOMB,
      MOVE_TEXTURE("icon_item_custom/", "gItemIconBanjoBeakBombTex"),
      MOVE_TEXTURE("item_name_custom/", "gBanjoBeakBombNameTex"),
      "You learned the %rBeak Bomb%w!^While flying, press %y\xA0%w to dive&beak first into enemies.",
      "%rBeak Bomb&%wPress %y\xA0%w while flying to charge&beak first.", DrawBeakBomb },
};

static const SOHModApi* sApi;
static SOHCustomItemRandomizer sMoveLogic[ARRAY_COUNT(kMoves)];

static bool IsMoveShuffle(void) {
    return SOH_MOD_API_HAS(sApi, GetRandoOption) && sApi->GetRandoOption(SHUFFLE_MOVES_OPTION) != 0;
}

static void RegisterMove(const BanjoMove* move, SOHCustomItemRandomizer* logic, uint8_t cell) {
    SOHCustomItemDefinition item = Z64Items_Define(move->key, move->iconTexture, move->nameTexture);

    logic->structSize = sizeof(*logic);
    logic->type = SOH_CUSTOM_ITEM_TYPE_ITEM;
    logic->poolCount = 1;
    logic->logicKey = move->key;
    logic->hintKey = move->key;
    logic->progressionGroup = "";
    logic->poolModeOption = SHUFFLE_MOVES_OPTION;
    logic->poolModeValue = 1;
    Z64Items_SetTextbox(&item, move->textbox);
    Z64Items_SetPauseText(&item, move->pauseText);
    item.getItemEntry.drawFunc = move->drawGetItem;
    Z64Items_SetLogic(&item, logic);
    Z64Items_Register(sApi, &item);
    BanjoPage_Place(sApi, move->key, cell);
}

void BanjoMoves_Register(const SOHModApi* api) {
    sApi = api;
    for (int32_t index = 0; index < ARRAY_COUNT(kMoves); index++) {
        RegisterMove(&kMoves[index], &sMoveLogic[index], (uint8_t)(FIRST_MOVE_CELL + index));
    }
}

void BanjoMoves_GrantWithMask(void) {
    if (IsMoveShuffle() || !sApi->IsCustomItemOwned(BANJO_MASK_KEY)) {
        return;
    }
    for (int32_t index = 0; index < ARRAY_COUNT(kMoves); index++) {
        if (!sApi->IsCustomItemOwned(kMoves[index].key)) {
            sApi->SetCustomItemOwned(kMoves[index].key, true);
        }
    }
}

void BanjoMoves_ApplyLearned(void) {
    for (int32_t index = 0; index < ARRAY_COUNT(kSpiralMountainMoves); index++) {
        LibBk_SetAbilityLearned(kSpiralMountainMoves[index], true);
    }
    for (int32_t index = 0; index < ARRAY_COUNT(kMoves); index++) {
        LibBk_SetAbilityLearned(kMoves[index].ability, sApi->IsCustomItemOwned(kMoves[index].key));
    }
}
