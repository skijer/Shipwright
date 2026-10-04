// Production carry/stow/input functions; input, swing and passive-engine
// boundaries are fixtures.
#include "global.h"
#include "mods/items/custom_items.h"
#include "mods/items/helpers/equip_helper.h"
#include "mods/items/logic/item_lantern.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

CustomItemState gCustomItemState;
static Player player;
static PlayState play;
static ItemInputState input;
static u8 blocked;
static int swings, lights, heals;

static u8 IsItemEquipped(u8 item) {
  assert(item == ITEM_LANTERN);
  return input.wasEquipped;
}
void ItemInput_Update(ItemInputState *output, u8 item, Player *p,
                      PlayState *context) {
  assert(item == ITEM_LANTERN && p == &player && context == &play);
  *output = input;
}
u8 ItemInput_IsBlocked(Player *p, PlayState *context) { return blocked; }
static void Lantern_UpdateSwing(Player *p, PlayState *context) {}
static void Lantern_UpdateLight(Player *p, PlayState *context) {
  assert(p == &player && context == &play);
  ++lights;
}
static void Lantern_UpdateGreenHeal(Player *p, PlayState *context) {
  assert(p == &player && context == &play);
  ++heals;
}
void Player_StartLanternSwing(Player *p, PlayState *context) {
  assert(!gCustomItemState.lanternStowed);
  ++swings;
  gCustomItemState.lanternEquipped = 1;
}

#include "lantern_stow_functions.inc"

static void Reset(u8 fire) {
  memset(&gCustomItemState, 0, sizeof(gCustomItemState));
  memset(&player, 0, sizeof(player));
  memset(&play, 0, sizeof(play));
  play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
  player.heldItemAction = PLAYER_IA_NONE;
  gCustomItemState.lanternFireType = fire;
  gCustomItemState.lanternHealTimer = 23;
  gCustomItemState.lanternCatchState = 2;
  gCustomItemState.lanternCatchWindow = 1;
  input = (ItemInputState){.wasEquipped = 1};
  blocked = 0;
  swings = lights = heals = 0;
}

int main(void) {
  for (u8 fire = LANTERN_FIRE_NONE; fire < LANTERN_FIRE_MAX; ++fire) {
    Reset(fire);
    Handle_Lantern(&player, &play);
    assert(gCustomItemState.lanternEquipped == (fire != LANTERN_FIRE_NONE));
    // An unlit lantern is still stowable after its ordinary swing.
    if (fire == LANTERN_FIRE_NONE) {
      input.isPressed = 1;
      Handle_Lantern(&player, &play);
      input.isPressed = 0;
    }
    assert(Lantern_IsInHand());

    CustomItemState expected = gCustomItemState;
    expected.lanternEquipped = expected.lanternSwinging = 0;
    expected.lanternStowed = 1;
    Lantern_PutAway(&player, &play);
    assert(!memcmp(&expected, &gCustomItemState, sizeof(expected)));
    Lantern_PutAway(&player, &play); // Idempotent: no fire/catch/healing reset.
    assert(!memcmp(&expected, &gCustomItemState, sizeof(expected)));
    for (int frame = 0; frame < 30; ++frame) {
      Handle_Lantern(&player, &play);
      Lantern_UpdatePassive(&play);
      assert(!gCustomItemState.lanternEquipped && !Lantern_IsInHand());
      assert(!memcmp(&expected, &gCustomItemState, sizeof(expected)));
    }
    assert(lights == 30 && heals == 30);

    // Holding a button or trying to use it while blocked does not cancel
    // explicit stow.
    input.isHeld = 1;
    Handle_Lantern(&player, &play);
    assert(!gCustomItemState.lanternEquipped);
    input.isPressed = blocked = 1;
    Handle_Lantern(&player, &play);
    assert(gCustomItemState.lanternStowed);
    blocked = 0;
    int previousSwings = swings;
    Handle_Lantern(&player, &play);
    assert(swings == previousSwings + 1 && Lantern_IsInHand());
    assert(!gCustomItemState.lanternStowed &&
           gCustomItemState.lanternFireType == fire);
    Lantern_PutAway(&player, &play);
    Player_InitLanternIA(&play, &player);
    assert(!gCustomItemState.lanternStowed);
  }

  // Another item only borrows the hand; stowing it does not suppress an
  // already-pocketed lantern.
  Reset(LANTERN_FIRE_GREEN);
  Handle_Lantern(&player, &play);
  player.heldItemAction = PLAYER_IA_SWORD_MASTER;
  Handle_Lantern(&player, &play);
  assert(!gCustomItemState.lanternEquipped);
  Lantern_PutAway(&player, &play);
  assert(!gCustomItemState.lanternStowed);
  player.heldItemAction = PLAYER_IA_NONE;
  Handle_Lantern(&player, &play);
  assert(Lantern_IsInHand());
  input.wasEquipped = 0;
  assert(!Lantern_IsInHand());

  puts("PASS: all lantern fires stow persistently, preserve passive "
       "dispatch/state, and resume on fresh use");
  return 0;
}
