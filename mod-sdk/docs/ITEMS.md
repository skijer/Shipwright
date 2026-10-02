# Items

A custom item is one registration: a key, an icon, a name texture, and whichever behaviour you fill in. The
host takes care of everything around it: the pause-menu page and cursor, equipping to B, C or D-pad, saving what
the player owns and where it is equipped (per age), the get-item cutscene and textbox, drops, the console, the
save editor and the randomizer.

Start from `templates/item_template`.

## Registering

```c
#include "z64items.h"

SOHCustomItemDefinition lantern = Z64Items_Define("yourname.lantern", sIconTex, sNameTex);
Z64Items_SetButtons(&lantern, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
Z64Items_SetPlacement(&lantern, 0, 3, 0);
Z64Items_SetTextbox(&lantern, "You got the %rLantern%w!");
Z64Items_SetPauseText(&lantern, "%rLantern&%wLights up dark places.");
Z64Items_SetAction(&lantern, TakeOutLantern, UpdateLantern);
Z64Items_Register(sApi, &lantern);
```

`z64items.h` is header-only: link the `z64items` target and include it. Every helper just fills a field of
`SOHCustomItemDefinition` (`soh/soh/ModApi/CustomItemRegistry/CustomItemRegistry.h`), so you can also set fields
by hand. A zeroed definition with only `structSize`, `key`, `iconPath` and `namePath` is a valid item that can be
owned but not used. The host copies the definition and its strings once, at registration.

| Helper | Field(s) | Meaning |
|---|---|---|
| `Z64Items_Define(key, icon, name)` | `key`, `iconPath`, `namePath` | Identity. Icon is a 32×32 RGBA32 texture, name a 128×16 IA4 texture (see [Assets](ASSETS.md)). |
| `Z64Items_SetButtons(def, flags)` | `flags` | `SOH_CUSTOM_ITEM_B_BUTTON`, `_C_BUTTON`, `_DPAD`. Makes the item equippable. |
| `Z64Items_SetPlacement(def, page, slot, priority)` | `preferredPage`, `preferredSlot`, `priority` | Where it appears in the pause menu (see [Placement](#placement)). |
| `Z64Items_SetAge(def, age)` | `ageRequirement` | `SOH_CUSTOM_ITEM_AGE_ANY` (0), `_CHILD`, `_ADULT`. |
| `Z64Items_SetTextbox(def, text)` | `getItemText` | The textbox shown when the item is obtained. |
| `Z64Items_SetPauseText(def, text)` | `pauseText` | The description shown with C-Up on the pause screen. |
| `Z64Items_SetCanUse(def, fn)` | `canUse` | Called before every use; return `false` to refuse (error sound, nothing happens). |
| `Z64Items_SetAction(def, init, update)` | `init`, `update` | What pressing the button does. See [Behaviour patterns](#behaviour-patterns). |
| `Z64Items_SetHeldCallbacks(def, use, putAway, drawHeld)` | `use`, `putAway`, `drawHeld` | For an item that stays in Link's hand. |
| `Z64Items_SetHeldModel(def, dl, modelGroup)` | `heldModelPath`, `modelGroup` | Display list drawn in Link's left hand, and which hand pose (`PLAYER_MODELGROUP_*`). |
| `Z64Items_SetGetItemModel(def, dl)` | `getItemModelPath` | Model shown over Link's head when obtained, and on the ground as a drop. |
| `Z64Items_SetFirstPersonModel(def, dl)` | `firstPersonModelPath` | What the right hand holds while aiming. `NULL` leaves it empty instead of vanilla's bow or slingshot. |
| `Z64Items_SetMeleeWeapon(def, weapon)` | `meleeWeapon` | Swing it like a vanilla weapon: `Z64ITEMS_MELEE_KOKIRI_SWORD`, `_MASTER_SWORD`, `_BIGGORON_SWORD`, `_DEKU_STICK`, `_HAMMER`. |
| `Z64Items_SetAmmo(def, fn)` | `getAmmo` | A number drawn under the icon like vanilla ammo; return a negative value to hide it. |
| `Z64Items_SetReplaces(def, item)` + `Z64Items_SetVanillaMode(def, mode, randoGet)` | `replacesItem`, `vanillaMode`, `randoItem` | Tie the item to a vanilla one (see [Vanilla items](#vanilla-items)). |
| `Z64Items_SetLogic(def, randomizer)` | `randomizer` | Put the item in the randomizer pool (see [Randomizer](RANDOMIZER.md)). |

Other fields you set by hand:

| Field | Meaning |
|---|---|
| `flags \|= SOH_CUSTOM_ITEM_INSTANT` | The item acts on the press and never stays in Link's hand. |
| `flags \|= SOH_CUSTOM_ITEM_WEARABLE` | Usable where vanilla allows trade items and masks (masks use this). |
| `onAcquire(key)` / `onRemove(key)` | The first time the item is owned / when it stops being owned. |
| `onReceive(key)` | Every time a copy is received. A progressive item upgrades itself here. |
| `getItemEntry` | A full vanilla `GetItemEntry` when you want a vanilla get-item model or text id. Its `drawFunc` beats `getItemModelPath`; a non-zero `textId` beats `getItemText`. |
| `disabledIconSurfaces` | `SOH_ITEM_ICON_*` bits to not draw the icon on (textbox, save editor, HUD buttons…). `SOH_ITEM_NAME_TEXTURE` hides the name texture. |
| `presentationFlags` | `SOH_ITEM_HIDE_FROM_SAVE_EDITOR`, `SOH_ITEM_HIDE_FROM_KALEIDO`. |

Texts use the game's codes: `%r %g %b %y %c %w` colours, `&` new line, `^` new box, and the button glyphs
`\x9F` A, `\xA0` B, `\xA1` C, `\xA2` L, `\xA3` R, `\xA4` Z. Both texts are built when their textbox opens, so an
item that changes (a progressive item, a mode wheel) rewrites them with
`Z64Items_UpdateText(sApi, key, SOH_ITEM_TEXT_GET_ITEM or SOH_ITEM_TEXT_PAUSE, text)`.

## Behaviour patterns

Every custom item shares one item action, `PLAYER_IA_CUSTOM`; the registry dispatches to the item that is
equipped on the pressed button. Pick the pattern that matches your item.

### Instant: do something on the press

`flags |= SOH_CUSTOM_ITEM_INSTANT` and put the effect in `init`. The Heart Charm template, a spell, a mask that
toggles a form. `canUse` decides whether the press counts.

### Held: take it out, press again to use it

Leave `INSTANT` off. `init` runs when the item is taken out, `update` is its *upper action*, running every frame
it is in hand, `use` runs when its button is pressed again, and `putAway` when it leaves the hand for any reason
(another item, damage, a cutscene, a door).

The value `update` returns is "the upper body is busy". Return `1` only while the item really uses the arms: while
it returns `1`, Link cannot roll, attack or switch items. Returning `1` all the time makes Link feel stuck.

### An action of its own: the item takes the whole body

Digging, casting, riding a spinner. Install a player action exactly as vanilla does, and give Link back the same
way vanilla items do:

```c
static void DigAction(Player* player, PlayState* play) {
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        func_80839FFC(player, play);
    }
}

Player_SetupAction(play, player, DigAction, 0);
```

"Am I digging?" is `player->actionFunc == DigAction`: when damage or a cutscene takes the player, it replaces your
action and your state is gone with it. Never hold Link with `PLAYER_STATE1_INPUT_DISABLED`; nothing clears it if
something else takes the player, and the game softlocks.

Where you call `Player_SetupAction` matters:

- **Not from `init` or the upper action.** Those run inside the player's action-handler pass, and the rest of the
  update picks an action of its own the same frame (idle, fall), so yours dies at birth. It only seems to work
  when Link stands still.
- **From `OnPlayerUpdate`.** It runs at the end of the player update, after everything else. Mark a flag in
  `init`/`use`, and start the action there (the shovel does this).
- A multi-frame move that must keep Link walking can instead live in the upper action: return `1`, and set every
  frame what you need (speed, gravity, position, animation). Vanilla only chooses an animation when its state
  changes, so reset it yourself when you finish: `playSpeed = 1` and
  `Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player))`.

While your own action owns the player, the upper action stops running and the host would read that as "the item
is no longer in hand" after four frames. Call `Z64Items_KeepHeld(sApi)` from your action every frame.

Taking an item out plays a change animation that overwrites the body. Start a long action only once
`player->upperActionFunc` is no longer `Player_UpperAction_ChangeHeldItem`.

### A weapon

`Z64Items_SetMeleeWeapon(&def, Z64ITEMS_MELEE_KOKIRI_SWORD)` and a matching `modelGroup`: pressing the button
again swings it through vanilla's own attack code, trail, spin and weapon length. Draw your model in the hand
yourself (see [Drawing in Link's hand](#drawing-in-links-hand)). Change what the hit counts as with the
`OnResolveSwordDamage` hook.

### Aiming in first person

`include/z64aiming/z64aiming.h`: `Z64Aiming_Request`, `_Update` every frame, `_GetDirection`, `_DrawReticle`,
`_Release`, and `_Drop` on scene change. A positive pitch means looking down. Vanilla stops reading item buttons
while aiming with a sub-camera active, so read your button from `play->state.input[0]` yourself.

### Several modes in one slot

`include/z64wheel/z64wheel.h` draws a row picker over the HUD: hold L with the item in hand, pick with the stick. The
wheel stores its unlocks and selection in the mod's storage, and swaps the item's icon to the selected mode.

### Reacting in the air or in water

Vanilla does not read item buttons while Link is airborne or swimming. An item that must (a double jump, a
glider) reads its equipped button from the raw input itself, and skips the frame its normal press was already
handled.

### What the item hands over is decided before it is received

The get-item textbox opens before the item is received. A pickup that is not always the same thing (a random
rune, the next level of a progressive item) decides what it is in `OnResolveCustomGetItem`, which repeats every
frame Link is in range, and grants it in `onReceive`.

## Drawing in Link's hand

`heldModelPath` covers the simple case. For anything else, draw from `OnPlayerPostLimbDraw` using the limb's own
matrix, which is what makes a model look held rather than floating next to Link:

```c
static void DrawInRightHand(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_R_HAND) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(1.0f / player->actor.scale.x, 1.0f / player->actor.scale.y, 1.0f / player->actor.scale.z,
                 MTXMODE_APPLY);
    Matrix_Translate(0.0f, 6.0f, -3.0f, MTXMODE_APPLY);
    Matrix_Scale(0.4f, 0.4f, 0.4f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sModelDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
```

Dividing by the actor scale puts the offsets in world units. Measure offsets and rotations in game; do not guess.

Every function that emits display-list commands opens its own `OPEN_DISPS`/`CLOSE_DISPS`: a helper that writes to
`POLY_OPA_DISP` relying on its caller's block compiles, but fails to link (`__gfxCtx` unresolved). And those two
macros are a brace pair: never `return` between them.

## Placement

`Z64Items_SetPlacement(&def, page, slot, priority)`:

| `page` | Where |
|---|---|
| `0`, `1`, `2`… | Extra pause pages, cycled with L (Z with the GameCube switcher) after the vanilla grid. A page only shows once the player owns something on it. |
| `SOH_ITEM_PAGE_VANILLA` (0xFF) | A cell of OoT's own item grid, on top of the vanilla item there. |
| `SOH_ITEM_PAGE_QUEST_VANILLA` (0xE0) | A point of the quest screen (medallions 0-5, songs 6-17, stones 18-20…), to hang an item off a medallion. |
| `0xE1` and up | Extra quest pages. |

`slot` is 0-23 on item pages. Several items in the same slot form a **wheel**, ordered by `priority` (highest
first): A on the cell spins it, the stick picks, and buttons holding the previous item move to the new one.
Players can rearrange everything in *Save Editor → Item Layout*; your placement is only the default. Check the
placements of the other mods you expect to be installed with yours; two items in the same cell wheel together.

## Vanilla items

`Z64Items_SetReplaces(&def, ITEM_HAMMER)` ties the item to a vanilla one; `Z64Items_SetVanillaMode` says how:

| Mode | Effect |
|---|---|
| `SOH_VANILLA_ITEM_UPGRADE` (default) | The vanilla item is still obtained normally and owning it *is* owning yours: your icon, name, text and action replace its own, but it keeps its vanilla slot and buttons. |
| `SOH_VANILLA_ITEM_REPLACE` | Everything that would give the vanilla item gives yours instead, and the vanilla item never enters the inventory. Yours is a full item with its own page, wheel and buttons. |
| `SOH_VANILLA_ITEM_BLOCK` | The vanilla item is removed from the game. Use `sApi->BlockVanillaItem(item, randoGet)` to do only this. |

Pass the item's `RandomizerGet` as `randoItem` so the randomizer takes the vanilla item out of the pool and
old seeds still grant yours. Existing save files are swept on load: a blocked item already in the inventory is
replaced.

To reuse a vanilla item's look, point `iconPath`, `namePath` and `getItemEntry` at the vanilla ones
(`ItemTable_RetrieveEntry(MOD_NONE, GI_*)` with `textId = 0` so your own textbox wins).

## Equipment

Swords, shields, tunics, boots and passive upgrades go through their own registry, the equipment screen:
`sApi->RegisterCustomEquipment(&SOHCustomEquipDefinition)`.

| Field | Meaning |
|---|---|
| `slot` | `SOH_EQUIP_SLOT_SWORD`, `_SHIELD`, `_TUNIC`, `_BOOTS`, or `_UPGRADE` for a passive toggled on and off. |
| `page`, `row`, `column` | `SOH_EQUIP_PAGE_VANILLA` takes over a cell of OoT's equipment grid; `0`, `1`… are extra equipment pages. On extra pages column 0 holds passives. |
| `vanillaBase` | The `EQUIP_VALUE_*` worn underneath while yours is worn (so vanilla code sees a valid sword or tunic); `0` leaves the slot bare. |
| `onEquip`, `onUnequip` | Put on / taken off, for any reason (age change, vanilla item equipped over it, lost). |
| `getItemText`, `getItemModelPath`, `getItemEntry` | Make the piece obtainable: the registry creates a hidden companion item with the same key that marks it owned. |

Query it with `IsEquipmentWorn(key)`, `GetWornEquipment(slot)`, `IsEquipmentToggleOn(key)`. Console:
`give custom_equipment "<key>"`, `wear "<key>"`.

## Console

```
give custom_item "<key>"                       the full get-item sequence
drop custom_item "<key>"                       a pickup in front of Link
equip custom_item "<key>" <b|cleft|cdown|cright|dup|ddown|dleft|dright>
give 36 / drop 36                              a vanilla ItemID, through whatever replaces it today
```

A mod can handle its own `give <type> "<key>"` families with the `OnConsoleGive` hook.
