// Real initializers, equip polling and teardown. Audio, camera and effect
// allocation are fixtures.
#include "global.h"
#include "mods/items/helpers/equip_helper.h"
#include "mods/items/helpers/fx_helper.h"
#include "mods/items/logic/item_rod_fire.h"
#include "mods/items/logic/item_rod_ice.h"
#include "mods/items/logic/item_rod_light.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define fireRodFlameActive gCustomItemState.fireRodFlameActive
#define iceRodWaveActive gCustomItemState.iceRodWaveActive
#define lightRodBeamActive gCustomItemState.lightRodBeamActive

CustomItemState gCustomItemState;
static Player player;
static PlayState play;
static int cameraExits, trailKills, stopSounds, unequipSounds;
static u16 lastSound;

u8 ItemInput_CheckDamage(Player *p, s8 *previous) { return 0; }
void FirstPerson_Exit(Player *p, PlayState *context) { ++cameraExits; }
void Audio_StopSfxById(u32 sound) { ++stopSounds; }
void ItemEquip_PlayEquipSFX(PlayState *context, Player *p) {}
void ItemEquip_PlayUnequipSFX(PlayState *context, Player *p) {
  ++unequipSounds;
  lastSound = NA_SE_PL_CHANGE_ARMS;
}
s32 FX_InitSwordTrail(PlayState *context, RodColor *color) { return 7; }
void FX_KillSwordTrail(PlayState *context, s32 index) {
  assert(index == 7);
  ++trailKills;
}
s32 Collider_InitCylinder(PlayState *context, ColliderCylinder *collider) {
  return 0;
}
s32 Collider_SetCylinder(PlayState *context, ColliderCylinder *collider,
                         Actor *actor, ColliderCylinderInit *init) {
  return 0;
}
s32 Collider_DestroyCylinder(PlayState *context, ColliderCylinder *collider) {
  return 0;
}
static void FireRod_InitFlameColliders(Player *p, PlayState *context) {}
static void IceRod_InitWaveColliders(Player *p, PlayState *context) {}
static void LightRod_InitBeamColliders(Player *p, PlayState *context) {}

// Native sound boundary and item dispatch: rod teardown remains production code.
void func_808328EC(Player *p, u16 sound) { ++unequipSounds; lastSound = sound; }
s32 func_8008F2BC(Player *p, s32 action) {
  return action == PLAYER_IA_SWORD_MASTER ? 0 : -1;
}
void Player_UseItem(PlayState *context, Player *p, s32 item) {
  if (item == ITEM_NONE) {
    FireRod_PutAway(p, context);
    IceRod_PutAway(p, context);
    LightRod_PutAway(p, context);
    p->heldItemAction = PLAYER_IA_NONE;
  } else {
    p->heldItemAction = PLAYER_IA_BOW;
  }
}
static void Mitts_Deactivate(void) {}
#include "rod_stow_functions.inc"

static void CheckAnimationSoundOwnership(void) {
  const s8 actions[] = {PLAYER_IA_ROD_FIRE, PLAYER_IA_ROD_ICE, PLAYER_IA_ROD_LIGHT};
  void (*init[])(PlayState *, Player *) = {Player_InitFireRodIA, Player_InitIceRodIA, Player_InitLightRodIA};
  void (*stow[])(Player *, PlayState *) = {FireRod_PutAway, IceRod_PutAway, LightRod_PutAway};
  for (int rod = 0; rod < 3; ++rod) {
    for (int earlyCleanup = 0; earlyCleanup < 2; ++earlyCleanup) {
      for (int swap = 0; swap < 2; ++swap) {
        memset(&gCustomItemState, 0, sizeof(gCustomItemState));
        init[rod](&play, &player);
        player.heldItemAction = actions[rod];
        ItemEquip_ResetUnequipSound(&play, &player, actions[rod]);
        player.heldItemId = swap ? ITEM_BOW : ITEM_NONE;
        unequipSounds = 0;
        if (earlyCleanup) stow[rod](&player, &play);
        Player_FinishItemChange(&play, &player);
        stow[rod](&player, &play); // Late polling cleanup after a native swap.
        // One outgoing sound, plus one incoming sound only when swapping.
        assert(unequipSounds == 1 + swap);
        assert(!fireRodActive && !iceRodActive && !lightRodActive);
      }
    }
  }
  player.heldItemAction = PLAYER_IA_MOGMA_MITTS;
  player.heldItemId = ITEM_NONE;
  unequipSounds = 0;
  Mitts_OnUnequip(&play, &player);
  Player_FinishItemChange(&play, &player);
  assert(unequipSounds == 1);
  const s8 ordinary[] = {PLAYER_IA_NONE, PLAYER_IA_BOW, PLAYER_IA_SWORD_MASTER};
  for (int i = 0; i < 3; ++i) {
    player.heldItemAction = ordinary[i];
    ItemEquip_ResetUnequipSound(&play, &player, ordinary[i]);
    player.heldItemId = ITEM_NONE;
    unequipSounds = 0;
    Player_FinishItemChange(&play, &player);
    assert(unequipSounds == (i != 0));
    if (i != 0) assert(lastSound == (i == 2 ? NA_SE_IT_SWORD_PUTAWAY : NA_SE_PL_CHANGE_ARMS));
  }
}


// Every custom tool sharing native and custom stow ownership. These use the
// production sound helpers and native Finish; item-specific cleanup is covered
// above for rods/mitts and by the Ball-and-Chain and stow fixtures.
static void CheckAllItemSoundOwnership(void) {
  const s8 actions[] = {
    PLAYER_IA_ROD_FIRE, PLAYER_IA_ROD_ICE, PLAYER_IA_ROD_LIGHT,
    PLAYER_IA_SHOVEL, PLAYER_IA_SWITCH_HOOK, PLAYER_IA_MOGMA_MITTS,
    PLAYER_IA_WHIP, PLAYER_IA_GUST_JAR, PLAYER_IA_DEKU_LEAF,
    PLAYER_IA_DOMINION_ROD, PLAYER_IA_BALL_AND_CHAIN, PLAYER_IA_BEETLE,
    PLAYER_IA_BOMB_ARROWS, PLAYER_IA_CANE_OF_SOMARIA
  };
  memset(&gCustomItemState, 0, sizeof(gCustomItemState));
  for (unsigned i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i) {
    for (int early = 0; early < 2; ++early) {
      for (int swap = 0; swap < 2; ++swap) {
        for (int cycle = 0; cycle < 3; ++cycle) {
          ItemEquip_PlayEquipSFXForAction(&play, &player, actions[i]);
          player.heldItemAction = actions[i];
          player.heldItemId = swap ? ITEM_BOW : ITEM_NONE;
          unequipSounds = 0;
          if (early) ItemEquip_PlayUnequipSFXForAction(&play, &player, actions[i]);
          ItemEquip_BeginItemChangeSound(&play, &player, actions[i]);
          play.gameplayFrames += 5; // Delayed animation completion.
          Player_FinishItemChange(&play, &player);
          // Cleanup after the incoming action changes must still use the old key.
          ItemEquip_PlayUnequipSFXForAction(&play, &player, actions[i]);
          ItemEquip_PlayUnequipSFXForAction(&play, &player, actions[i]);
          assert(unequipSounds == 1 + swap);
          assert(lastSound == NA_SE_PL_CHANGE_ARMS);
        }
      }
    }
    // A completion sound is a separate event from a later actual stow.
    ItemEquip_ResetUnequipSound(&play, &player, actions[i]);
    unequipSounds = 0;
    ItemEquip_PlayUnequipSFXForAction(&play, &player, actions[i]);
    play.gameplayFrames += 20;
    player.heldItemAction = actions[i];
    player.heldItemId = ITEM_NONE;
    ItemEquip_BeginItemChangeSound(&play, &player, actions[i]);
    play.gameplayFrames += 5;
    Player_FinishItemChange(&play, &player);
    assert(unequipSounds == 2);
    // Native-only/unused custom item still gets its one outgoing sound.
    ItemEquip_ResetUnequipSound(&play, &player, actions[i]);
    player.heldItemAction = actions[i];
    player.heldItemId = ITEM_NONE;
    unequipSounds = 0;
    Player_FinishItemChange(&play, &player);
    assert(unequipSounds == 1);
  }
  const s8 nativeOnly[] = {PLAYER_IA_LANTERN, PLAYER_IA_SPINNER,
    PLAYER_IA_TIME_GATE, PLAYER_IA_MINISH_CAP, PLAYER_IA_ROCS_FEATHER_SKIJER,
    PLAYER_IA_ROCS_CAPE, PLAYER_IA_HYLIAS_GRACE, PLAYER_IA_ZONAI_PERMAFROST,
    PLAYER_IA_DEMISE_DESTRUCTION};
  for (unsigned i = 0; i < sizeof(nativeOnly) / sizeof(nativeOnly[0]); ++i) {
    ItemEquip_ResetUnequipSound(&play, &player, nativeOnly[i]);
    player.heldItemAction = nativeOnly[i];
    player.heldItemId = ITEM_NONE;
    unequipSounds = 0;
    Player_FinishItemChange(&play, &player);
    assert(unequipSounds == 1 && lastSound == NA_SE_PL_CHANGE_ARMS);
  }
  // Separate simultaneous source keys cannot consume one another's sound.
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE);
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_ICE);
  unequipSounds = 0;
  ItemEquip_PlayUnequipSFXForAction(&play, &player, PLAYER_IA_ROD_FIRE);
  ItemEquip_PlayUnequipSFXForAction(&play, &player, PLAYER_IA_ROD_ICE);
  assert(unequipSounds == 2);
}

// A native animation can finish equipping after the initial C press has passed.
// The A press must still call the rod's real teardown, even though its custom
// polling never saw an equip press.
#define CHECK_NATIVE_EQUIP_AND_CANCEL(Camel, lower, equip)                     \
  do {                                                                         \
    memset(&gCustomItemState, 0, sizeof(gCustomItemState));                    \
    memset(&(equip), 0, sizeof(equip));                                        \
    Player_Init##Camel##IA(&play, &player);                                    \
    assert(lower##Active && (equip).isEquipped);                               \
    lower##FirstPerson = 1;                                                    \
    lower##Charging = 1;                                                       \
    lower##ChargeLevel = 0.6f;                                                 \
    lower##SpinActive = 1;                                                     \
    lower##SpinRadius = 125.0f;                                                \
    ItemInputState input = {.wasEquipped = 1, .otherButtonPressed = 1};        \
    ItemEquip_Update(&(equip), &input, NULL, Camel##_OnUnequip, &player,       \
                     &play);                                                   \
    assert(!lower##Active && !(equip).isEquipped && !lower##FirstPerson);      \
    assert(!lower##Charging && lower##ChargeLevel == 0.0f &&                   \
           !lower##SpinActive);                                                \
    assert(lower##SpinRadius == 0.0f && lower##BlureIdx == -1);                \
  } while (0)

#define CHECK_EXPLICIT_PUTAWAY(Camel, lower, equip)                            \
  do {                                                                         \
    Player_Init##Camel##IA(&play, &player);                                    \
    (equip).isEquipped =                                                       \
        0; /* Cleanup follows actual held state, even if polling was stale. */ \
    lower##FirstPerson = lower##Charging = 1;                                  \
    int previousSounds = unequipSounds;                                        \
    Camel##_PutAway(&player, &play);                                           \
    assert(!lower##Active && !lower##FirstPerson && !lower##Charging &&        \
           !(equip).isEquipped);                                               \
    assert(unequipSounds == previousSounds + 1);                               \
    CustomItemState expected = gCustomItemState;                               \
    int previousStops = stopSounds;                                            \
    Camel##_PutAway(&player, &play);                                           \
    assert(!memcmp(&expected, &gCustomItemState, sizeof(expected)));           \
    assert(unequipSounds == previousSounds + 1 &&                              \
           stopSounds == previousStops);                                       \
  } while (0)

int main(void) {
  CHECK_NATIVE_EQUIP_AND_CANCEL(FireRod, fireRod, sEquipState);
  CHECK_NATIVE_EQUIP_AND_CANCEL(IceRod, iceRod, sIceEquipState);
  CHECK_NATIVE_EQUIP_AND_CANCEL(LightRod, lightRod, sLightEquipState);
  assert(cameraExits == 3 && trailKills == 3 && unequipSounds == 3 &&
         stopSounds > 0);
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_FIRE);
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_ICE);
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_ROD_LIGHT);
  CHECK_EXPLICIT_PUTAWAY(FireRod, fireRod, sEquipState);
  CHECK_EXPLICIT_PUTAWAY(IceRod, iceRod, sIceEquipState);
  CHECK_EXPLICIT_PUTAWAY(LightRod, lightRod, sLightEquipState);
  CheckAnimationSoundOwnership();
  CheckAllItemSoundOwnership();
  puts("PASS: single rod stow sound across native animation and early cleanup; ordinary equipment preserved");
  puts("PASS: three native rod equips cancel through real polling; explicit "
       "teardown is idempotent");
  return 0;
}
