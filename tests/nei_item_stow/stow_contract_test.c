// The native dispatch guards, shared classifier/cleanup, and input latch are
// production code. Heavy per-item teardown is a boundary tested separately by
// the lantern/rod and Ball and Chain fixtures.
#include "global.h"
#include "mods/extended_player.h"
#include "mods/items/helpers/equip_helper.c"
#include "mods/items/logic/item_rod_fire.h"
#include "mods/items/logic/item_rod_ice.h"
#include "mods/items/logic/item_rod_light.h"
#include "mods/items/logic/item_whip.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

SaveContext gSaveContext;
CustomItemState gCustomItemState;
static Player player;
static PlayState play;
static Input *sControlInput;
static s32 sUpperBodyIsBusy;
static ItemEquipState sMittsEquipState;
static int cleanups[8];
static int transformed, form, lockedOn, canRoll, rollCalls, vetoStow, stowHooks;
static int claimed, intercept, instantPutaway, fdSword;
static int initCalls, modelCalls;

void Lantern_PutAway(Player *p, PlayState *s) {
  if (gCustomItemState.lanternEquipped || gCustomItemState.lanternSwinging)
    ++cleanups[0];
  gCustomItemState.lanternEquipped = gCustomItemState.lanternSwinging = 0;
}
void FireRod_PutAway(Player *p, PlayState *s) {
  if (fireRodActive || fireRodFirstPerson)
    ++cleanups[1];
  fireRodActive = fireRodFirstPerson = 0;
}
void IceRod_PutAway(Player *p, PlayState *s) {
  if (iceRodActive || iceRodFirstPerson)
    ++cleanups[2];
  iceRodActive = iceRodFirstPerson = 0;
}
void LightRod_PutAway(Player *p, PlayState *s) {
  if (lightRodActive || lightRodFirstPerson)
    ++cleanups[3];
  lightRodActive = lightRodFirstPerson = 0;
}
static void GustJar_Unequip(PlayState *s, Player *p) {
  ++cleanups[4];
  gCustomItemState.gustJarEquipped = 0;
}
static void Mitts_OnUnequip(PlayState *s, Player *p) {
  ++cleanups[5];
  gCustomItemState.mogmaMittsActive = 0;
}
static void BallChain_Stop(Player *p, PlayState *s) {
  ++cleanups[6];
  gCustomItemState.ballAndChainThrown = 0;
}
static void Whip_Stop(Player *p, PlayState *s) {
  ++cleanups[7];
  whipActive = 0;
}
#include "mods/items/custom_items_stow.c"

s8 Player_ItemToItemAction(s32 item);
s32 Player_UpdateHostileLockOn(Player *p);
s32 Player_TryRoll(Player *p, PlayState *s);
s32 Player_IsFDHoldingSword(Player *p);
void Player_UseItem(PlayState *s, Player *p, s32 item);
#include "stow_player.inc"

static void Reset(void) {
  memset(&player, 0, sizeof(player));
  memset(&play, 0, sizeof(play));
  memset(&gCustomItemState, 0, sizeof(gCustomItemState));
  memset(&gSaveContext, 0, sizeof(gSaveContext));
  memset(&sEquipCache, 0, sizeof(sEquipCache));
  memset(sStowedItemButtons, 0, sizeof(sStowedItemButtons));
  memset(sStowedItemFrames, 0, sizeof(sStowedItemFrames));
  memset(&sMittsEquipState, 0, sizeof(sMittsEquipState));
  memset(cleanups, 0, sizeof(cleanups));
  memset(gSaveContext.equips.buttonItems, ITEM_NONE,
         sizeof(gSaveContext.equips.buttonItems));
  sControlInput = &play.state.input[0];
  play.gameplayFrames = 1;
  transformed = form = lockedOn = canRoll = rollCalls = vetoStow = stowHooks =
      0;
  claimed = intercept = instantPutaway = fdSword = initCalls = modelCalls =
      sUpperBodyIsBusy = 0;
  player.heldItemAction = player.itemAction = PLAYER_IA_NONE;
}

static int CleanupCount(void) {
  int total = 0;
  for (int i = 0; i < 8; ++i)
    total += cleanups[i];
  return total;
}

static void SeedSpecialActions(void) {
  gCustomItemState.dekuLeafActive = gCustomItemState.dekuLeafGliding = 1;
  gCustomItemState.beetleActive = gCustomItemState.dominionRodActive = 1;
  gCustomItemState.demiseDestructionActive = 1;
  gCustomItemState.hyliasGraceActive = gCustomItemState.zonaiPermafrostActive =
      1;
  gCustomItemState.timeGateActive = gCustomItemState.spinnerActive = 1;
  gCustomItemState.switchHookActive = gCustomItemState.minishTinyActive = 1;
}

static void TestClassifierAndCleanup(void) {
  Reset();
  const u8 items[] = {ITEM_LANTERN,        ITEM_ROD_FIRE, ITEM_ROD_ICE,
                      ITEM_ROD_LIGHT,      ITEM_GUST_JAR, ITEM_MOGMA_MITTS,
                      ITEM_BALL_AND_CHAIN, ITEM_WHIP};
  memcpy(gSaveContext.equips.buttonItems, items, sizeof(items));
  assert(!CustomItems_HasStowableHeldItem(&player));
  assert(!CustomItems_HasStowableHeldItem(NULL));
  CustomItems_PutAwayHeldItems(NULL, &play);
  CustomItems_PutAwayHeldItems(&player, NULL);
  assert(CleanupCount() == 0);
  u8 *flags[] = {&gCustomItemState.lanternEquipped,
                 &gCustomItemState.lanternSwinging,
                 &fireRodActive,
                 &fireRodFirstPerson,
                 &iceRodActive,
                 &iceRodFirstPerson,
                 &lightRodActive,
                 &lightRodFirstPerson,
                 &gCustomItemState.gustJarEquipped,
                 &gCustomItemState.mogmaMittsActive,
                 &gCustomItemState.ballAndChainThrown,
                 &whipActive};
  for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
    Reset();
    *flags[i] = 1;
    assert(CustomItems_HasStowableHeldItem(&player));
    CustomItems_PutAwayHeldItems(&player, &play);
    assert(!CustomItems_HasStowableHeldItem(&player) && CleanupCount() == 1);
    CustomItems_PutAwayHeldItems(&player, &play);
    assert(CleanupCount() == 1);
  }
  for (int state = WHIP_STATE_EQUIP; state <= WHIP_STATE_LAUNCHED; ++state) {
    Reset();
    whipActive = 1;
    whipState = state;
    int special = state == WHIP_STATE_SWINGING || state == WHIP_STATE_LAUNCHED;
    assert(CustomItems_HasStowableHeldItem(&player) == !special);
    CustomItems_PutAwayHeldItems(&player, &play);
    assert(whipActive == special && cleanups[7] == !special);
  }
  Reset();
  SeedSpecialActions();
  CustomItemState before;
  memcpy(&before, &gCustomItemState, sizeof(before));
  assert(!CustomItems_HasStowableHeldItem(&player));
  CustomItems_PutAwayHeldItems(&player, &play);
  assert(!memcmp(&before, &gCustomItemState, sizeof(before)) &&
         CleanupCount() == 0);
  gCustomItemState.lanternEquipped = 1;
  CustomItems_PutAwayHeldItems(&player, &play);
  assert(!memcmp(&before, &gCustomItemState, sizeof(before)) &&
         cleanups[0] == 1);
  sMittsEquipState.isEquipped = 1;
  CustomItems_PutAwayHeldItems(&player, &play);
  assert(!sMittsEquipState.isEquipped && cleanups[5] == 1);
}

static void ReadyLantern(void) {
  gCustomItemState.lanternEquipped = 1;
  sControlInput->press.button = BTN_A;
}
static void TestPlayerEntries(void) {
  Reset();
  ReadyLantern();
  assert(!Player_ActionHandler_Roll(&player, &play));
  assert(cleanups[0] == 1 && stowHooks == 1 && rollCalls == 1);
  Reset();
  ReadyLantern();
  canRoll = 1;
  assert(Player_ActionHandler_Roll(&player, &play));
  assert(CleanupCount() == 0 && stowHooks == 0 &&
         gCustomItemState.lanternEquipped);
  for (int guard = 0; guard < 8; ++guard) {
    Reset();
    ReadyLantern();
    if (guard == 0)
      player.putAwayCooldownTimer = 1;
    if (guard == 1)
      vetoStow = 1;
    if (guard == 2)
      sUpperBodyIsBusy = 1;
    if (guard == 3)
      lockedOn = 1;
    if (guard == 4)
      player.stateFlags1 |= PLAYER_STATE1_ON_HORSE;
    if (guard == 5) {
      transformed = 1;
      form = 1;
    }
    if (guard == 6) {
      transformed = 1;
      form = 3;
    }
    if (guard == 7)
      sControlInput->press.button = 0;
    assert(!Player_ActionHandler_Roll(&player, &play));
    assert(CleanupCount() == 0 && gCustomItemState.lanternEquipped);
  }
  const int nativeRollForms[] = {0, 2, 5};
  for (int i = 0; i < 3; ++i) {
    Reset();
    ReadyLantern();
    transformed = 1;
    form = nativeRollForms[i];
    Player_ActionHandler_Roll(&player, &play);
    assert(cleanups[0] == 1);
  }
  Reset();
  gSaveContext.equips.buttonItems[1] = ITEM_LANTERN;
  sControlInput->press.button = BTN_A;
  Player_ActionHandler_Roll(&player, &play);
  assert(CleanupCount() == 0 && !Player_PutAwayHeldItem(&play, &player));
  Reset();
  ReadyLantern();
  assert(Player_PutAwayHeldItem(&play, &player));
  assert(cleanups[0] == 1 && !Player_PutAwayHeldItem(&play, &player));
  Reset();
  ReadyLantern();
  player.heldItemAction = PLAYER_IA_SWORD_MASTER;
  player.itemAction = PLAYER_IA_BOW;
  Player_UseItem(&play, &player, ITEM_NONE);
  assert(CleanupCount() == 0); // rejected by native acceptance gate
  player.itemAction = -1;
  Player_UseItem(&play, &player, ITEM_NONE);
  assert(cleanups[0] == 1);
  Reset();
  ReadyLantern();
  intercept = 1;
  Player_UseItem(&play, &player, ITEM_NONE);
  assert(CleanupCount() ==
         0); // form owns the transition until its clip completes
  Reset();
  ReadyLantern();
  player.stateFlags1 |= PLAYER_STATE1_SHIELDING | PLAYER_STATE1_IN_WATER;
  Player_UseItem(&play, &player, ITEM_NONE);
  assert(cleanups[0] ==
         1); // NONE remains accepted under native shield/water rules
  Reset();
  ReadyLantern();
  Player_UseItem(&play, &player, ITEM_SWORD_MASTER);
  assert(CleanupCount() == 0); // using another item is not explicit stow
  Reset();
  ReadyLantern();
  Player_InitItemAction(&play, &player, PLAYER_IA_NONE);
  Player_InitItemAction(&play, &player, PLAYER_IA_NONE);
  assert(cleanups[0] == 1 && initCalls == 2 && modelCalls == 2);
  Reset();
  ReadyLantern();
  Player_InitItemAction(&play, &player, PLAYER_IA_SWORD_MASTER);
  assert(CleanupCount() == 0 && gCustomItemState.lanternEquipped);
  Reset();
  player.heldItemAction = player.itemAction = PLAYER_IA_SWORD_MASTER;
  assert(Player_PutAwayHeldItem(&play, &player)); // native items still qualify
}

static void TestHudCandidateAndCooldown(void) {
  Reset();
  gSaveContext.equips.buttonItems[1] = ITEM_LANTERN;
  assert(Fixture_HudPutAway(&play, &player) == DO_ACTION_NONE);
  assert(player.putAwayCooldownTimer == 20);
  ReadyLantern();
  player.putAwayCooldownTimer = 0;
  assert(Fixture_HudPutAway(&play, &player) == DO_ACTION_PUTAWAY);
  player.putAwayCooldownTimer = 2;
  assert(Fixture_HudPutAway(&play, &player) == DO_ACTION_NONE &&
         player.putAwayCooldownTimer == 1);
  instantPutaway = 1;
  assert(Fixture_HudPutAway(&play, &player) == DO_ACTION_PUTAWAY &&
         player.putAwayCooldownTimer == 0);
  Reset();
  player.heldItemAction = PLAYER_IA_SWORD_MASTER;
  assert(Fixture_HudPutAway(&play, &player) == DO_ACTION_PUTAWAY);
  fdSword = 1;
  assert(Fixture_HudPutAway(&play, &player) == DO_ACTION_NONE);
}

static void TestInputSuppression(void) {
  ItemInputState input;
  const u8 items[] = {ITEM_BALL_AND_CHAIN, ITEM_WHIP};
  for (int item = 0; item < 2; ++item)
    for (int slot = 0; slot < 8; ++slot) {
      Reset();
      gSaveContext.equips.buttonItems[slot] = items[item];
      sControlInput->press.button = BTN_A | sButtonMasks[slot];
      sControlInput->cur.button = sButtonMasks[slot];
      ItemInput_SuppressUntilRelease(items[item], &play);
      ItemInput_SuppressUntilRelease(items[item], &play);
      ItemInput_Update(&input, items[item], &player, &play);
      assert(input.wasEquipped && input.otherButtonPressed);
      assert(!input.isPressed && !input.isHeld && !input.isReleased);
      ++play.gameplayFrames;
      sControlInput->press.button = 0;
      ItemInput_Update(&input, items[item], &player, &play);
      assert(!input.isPressed && !input.isHeld && !input.isReleased);
      ++play.gameplayFrames;
      sControlInput->cur.button = 0;
      ItemInput_Update(&input, items[item], &player, &play);
      assert(input.isReleased);
      ++play.gameplayFrames;
      sControlInput->cur.button = sControlInput->press.button =
          sButtonMasks[slot];
      ItemInput_Update(&input, items[item], &player, &play);
      assert(input.isPressed && input.isHeld);
    }
  // Removing a slot or claiming D-pad must not hide the release edge forever.
  for (int mode = 0; mode < 2; ++mode) {
    Reset();
    int slot = mode ? 4 : 1;
    gSaveContext.equips.buttonItems[slot] = ITEM_WHIP;
    sControlInput->cur.button = sButtonMasks[slot];
    ItemInput_SuppressUntilRelease(ITEM_WHIP, &play);
    ++play.gameplayFrames;
    sControlInput->cur.button = 0;
    if (mode)
      claimed = 1;
    else
      gSaveContext.equips.buttonItems[slot] = ITEM_NONE;
    ItemInput_Update(&input, ITEM_WHIP, &player, &play);
    ++play.gameplayFrames;
    claimed = 0;
    gSaveContext.equips.buttonItems[slot] = ITEM_WHIP;
    sControlInput->cur.button = sControlInput->press.button =
        sButtonMasks[slot];
    ItemInput_Update(&input, ITEM_WHIP, &player, &play);
    assert(input.isPressed && input.isHeld);
  }
  Reset();
  gSaveContext.equips.buttonItems[1] = ITEM_WHIP;
  sControlInput->cur.button = BTN_CLEFT;
  ItemInput_SuppressUntilRelease(ITEM_WHIP, &play);
  ++play.gameplayFrames;
  gSaveContext.equips.buttonItems[1] = ITEM_BALL_AND_CHAIN;
  gSaveContext.equips.buttonItems[2] = ITEM_WHIP;
  sControlInput->cur.button = BTN_CLEFT | BTN_CDOWN;
  sControlInput->press.button = BTN_CDOWN;
  ItemInput_Update(&input, ITEM_WHIP, &player, &play);
  assert(input.isPressed &&
         input.isHeld); // latch follows the old physical button
  ItemInput_Update(&input, ITEM_BALL_AND_CHAIN, &player, &play);
  assert(input.isHeld &&
         input.otherButtonPressed); // another item is not suppressed

  // No custom handler runs during this gap: a new physical press after
  // reassignment must still work even if the release was never polled.
  Reset();
  gSaveContext.equips.buttonItems[1] = ITEM_WHIP;
  sControlInput->cur.button = BTN_CLEFT;
  ItemInput_SuppressUntilRelease(ITEM_WHIP, &play);
  ++play.gameplayFrames;
  gSaveContext.equips.buttonItems[1] = ITEM_NONE;
  sControlInput->cur.button = sControlInput->press.button = 0;
  ++play.gameplayFrames;
  gSaveContext.equips.buttonItems[1] = ITEM_WHIP;
  sControlInput->cur.button = sControlInput->press.button = BTN_CLEFT;
  ItemInput_Update(&input, ITEM_WHIP, &player, &play);
  assert(input.isPressed && input.isHeld);

  for (int item = 0; item < 2; ++item) {
    Reset();
    gSaveContext.equips.buttonItems[1] = items[item];
    if (item) {
      whipActive = 1;
      whipState = WHIP_STATE_EQUIP;
    } else
      gCustomItemState.ballAndChainThrown = 1;
    sControlInput->cur.button = BTN_CLEFT;
    CustomItems_PutAwayHeldItems(&player, &play);
    ItemInput_Update(&input, items[item], &player, &play);
    assert(cleanups[6 + item] == 1 &&
           !input.isHeld); // real helper invokes latch before stop
  }
}

static void TestNativeSoundLifetime(void) {
  Reset();
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE);
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_ICE);
  assert(ItemEquip_ClaimUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE));
  assert(!ItemEquip_ClaimUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE));
  Player_InitItemAction(&play, &player, PLAYER_IA_ROD_ICE);
  // Initializing a new item must not reopen the outgoing item's sound.
  assert(!ItemEquip_ClaimUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE));
  assert(ItemEquip_ClaimUnequipSound(&play, &player, PLAYER_IA_ROD_ICE));
  Player_InitItemAction(&play, &player, PLAYER_IA_ROD_FIRE);
  assert(ItemEquip_ClaimUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE));
}

int main(void) {
  TestNativeSoundLifetime();
  TestClassifierAndCleanup();
  TestPlayerEntries();
  TestHudCandidateAndCooldown();
  TestInputSuppression();
  puts("Custom stow: actual classifier/cleanup, player guards, HUD cooldown, "
       "and release suppression passed");
  return 0;
}

// Native engine/item-animation boundaries used by the extracted entry points.
int32_t CVarGetInteger(const char *name, int32_t fallback) {
  if (!strcmp(name, "gEnhancements.DpadEquips"))
    return 1;
  if (!strcmp(name, "gEnhancements.InstantPutaway"))
    return instantPutaway;
  return fallback;
}
u8 Pacci_UltrahandModeActive(void) { return claimed; }
u8 MasterCycle_IsRiding(void) { return 0; }
u8 Cryonis_ModeActive(void) { return 0; }
uint8_t Sw97_IsBowItem(uint16_t item) { return 0; }
uint8_t Sw97_EffectiveElement(uint8_t sling) { return 0; }
u8 TransformMasks_IsTransformed(void) { return transformed; }
MmPlayerTransformation MmForm_GetCurrentForm(void) { return form; }
s32 Player_UpdateHostileLockOn(Player *p) { return lockedOn; }
s32 Player_TryRoll(Player *p, PlayState *s) {
  ++rollCalls;
  return canRoll;
}
s32 Player_IsFDHoldingSword(Player *p) { return fdSword; }
bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...) {
  if (flag == VB_PLAYER_PUTAWAY_HELD_ITEM) {
    ++stowHooks;
    return result && !vetoStow;
  }
  assert(flag == VB_PLAYER_TOGGLE_NAVI);
  return result;
}
s8 Player_ItemToItemAction(s32 item) {
  return item == ITEM_NONE ? PLAYER_IA_NONE : PLAYER_IA_SWORD_MASTER;
}
u8 MmForm_RitoTryAirThrow(Player *p, PlayState *s, s32 action) { return 0; }
u8 GerudoMhr_InterceptUseItem(PlayState *s, Player *p, s32 item) {
  return intercept;
}
u8 TradeAdult_IsMmTradeUseItem(s32 item) { return 0; }
void TradeAdult_PresentAndMessage(PlayState *s, Player *p, s32 item) {
  assert(0);
}
int MmBottle_FromItemId(unsigned short item) { return -1; }
int MmBottle_GetUseBehavior(int content) {
  assert(0);
  return 0;
}
s32 Player_ActionToMeleeWeapon(s32 action) {
  return action == PLAYER_IA_SWORD_MASTER;
}
static void NativeInit(PlayState *s, Player *p) { ++initCalls; }
ItemActionInitFunc ExtPlayer_GetItemActionInitFunc(int32_t action) {
  return NativeInit;
}
void Player_SetModelGroup(Player *p, s32 group) { ++modelCalls; }
