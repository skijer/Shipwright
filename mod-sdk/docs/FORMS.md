# Forms

A form changes **who Link is**: Majora's Mask style transformations (Zora, Goron, Deku, Fierce Deity), a
character skin with its own moves (Kafei, Keaton), or a full takeover (Mario, Wolf Link). Each form is its own
mod and registers itself with `RegisterForm`; a "pack" of forms is just a set of mods. No form depends on another
or on a central table.

Start from `templates/form_template`.

## The idea: Link is the base, the form declares what it replaces

Vanilla Link *is* the base form: actions, animations, body, speeds, sounds. A form does not rewrite the player;
it declares which pieces it replaces and everything else stays Link. It is Majora's Mask's model (per-form tables
over one `z_player`), with text keys instead of a compiled enum.

| Kind | What vanilla does | Examples |
|---|---|---|
| `SOH_FORM_KIND_LINK` | Still runs the player. The form replaces single pieces. | Zora, Goron, Deku, Fierce Deity, Kafei, Keaton |
| `SOH_FORM_KIND_TAKEOVER` | Neither runs the player's action nor draws Link. The form drives the whole frame. | Mario, Wolf Link |

## The mask is an item, the form points at it

A form has no inventory of its own. The mod registers **two things**: the mask, as an ordinary custom item (page,
slot, wheel, buttons, get-item — see [Items](ITEMS.md)), and the form, which names that item's key in `item`.
Pressing the mask toggles the form:

```c
static void WearMask(PlayState* play, Player* player) {
    sApi->ToggleForm("yourname.swift");
}
```

The mask is `SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE`. Without the mask in the inventory the form
cannot be entered, and losing it turns Link back. A form that takes the place of one of OoT's masks (Keaton,
Goron, Zora, Gerudo) registers its mask in `SOH_VANILLA_ITEM_REPLACE` mode, reusing the vanilla icon, name and
get-item model.

## The definition

`SOHFormDefinition` (`soh/soh/ModApi/Forms/FormRegistry.h`). Only `structSize`, `key`, `label` and `kind` are
required; an empty field means "use Link's".

| Field | Replaces |
|---|---|
| `item` | Key of the custom item (the mask) that grants the form. |
| `body` | A whole `PlayerAgeProperties`: ceiling height, shadow, wall radius, ledge reach, voice bank (`unk_92`) and footstep offset (`unk_94`). |
| `actions` / `actionCount` | Link's action handlers, see [Actions](#actions). |
| `anims` / `animCount` | Animations per `PLAYER_ANIMGROUP_*`; `animType` picks one model type or `SOH_FORM_ANIM_TYPE_ALL`. |
| `motionScale` | Multiplier on the stick's target speed (walk, run, swim, air). |
| `blockedButtons` | Buttons vanilla stops reading while the form is active. The mod still reads the raw pad. |
| `onEnter` / `onExit` | Entering and leaving the form. |
| `update` | Every frame while active. In `TAKEOVER` it replaces the player's action. |
| `draw` | In `TAKEOVER` it replaces Link's drawing; in `LINK` it draws on top. |
| `modelPath` | Root of the body's resources, mirroring the player object (see [The body](#the-body)). |
| `rootScaleAdult` / `rootScaleChild` | Height correction for a rig with other proportions than Link's. |
| `rootDrop` | How far the root drops after scaling, in limb units. |
| `height` | Body height for the camera, targeting and get-item (MM: Fierce Deity 124, Goron 80, Zora 68, Deku 36). `0` keeps Link's. A height that changes by itself (a rolled-up Goron) subscribes to `OnPlayerResolveHeight`. Never 0 or negative: the camera divides by it. |
| `resolveEquipment` | Which sword, shield, tunic and boots the form allows (see [Loadout](#loadout)). |
| `allowsButtonItem` | Which C and D-pad items the form allows. |
| `transformAnim` | Link's pose during the transformation cutscene. |
| `transformOffAnim` | The form's pose taking the mask off; `NULL` reuses `transformAnim`. |
| `transformMask` / `transformMaskClimax` | Display list of the mask on Link's face, and the squashed version swapped in at the climax. |
| `transformVoiceSfx` | The form's cry at the climax. |
| `transformGlowOffset` / `transformGlowScale` | Where the blue face glow sits in head-limb space; all zero = Link's face, scale 0 = 1. |

## Actions

The player tries its 14 action handlers in order every frame, and the first one that starts an action wins. A
form looks at a handler and answers:

| Result | Meaning |
|---|---|
| `SOH_FORM_ACTION_VANILLA` | Not mine: Link handles it as always. |
| `SOH_FORM_ACTION_BLOCKED` | Mine, but nothing started: the player keeps trying the next handlers. |
| `SOH_FORM_ACTION_STARTED` | Mine, and I started an action: the player stops trying. |

That is why a form can take B for its combo without losing the roll, putting items away or talking, which live in
the same handler. Inside an action handler you **may** call `Player_SetupAction`: it is where vanilla installs
its own. (From an item's `init` you may not; see [Items](ITEMS.md#an-action-of-its-own-the-item-takes-the-whole-body).)

| `SOHPlayerAction` | Link's action |
|---|---|
| `SOH_PLAYER_ACTION_FIRST_PERSON` | C-Up: first person and Navi |
| `SOH_PLAYER_ACTION_DOOR` | Opening doors |
| `SOH_PLAYER_ACTION_RECEIVE_ITEM` | Picking up or receiving an item |
| `SOH_PLAYER_ACTION_MOUNT_HORSE` | Mounting a horse |
| `SOH_PLAYER_ACTION_TALK` | Talking |
| `SOH_PLAYER_ACTION_WALL` | Walls: climb, crawl, push |
| `SOH_PLAYER_ACTION_ROLL` | A: roll, put item away, Navi |
| `SOH_PLAYER_ACTION_MELEE` | B: attack |
| `SOH_PLAYER_ACTION_SPIN_CHARGE` | Holding B: charging the spin |
| `SOH_PLAYER_ACTION_THROW` | Throwing or dropping what is carried |
| `SOH_PLAYER_ACTION_ZTARGET_A` | A while Z-targeting: sidehop, backflip, jumpslash |
| `SOH_PLAYER_ACTION_SHIELD` | R: shield |
| `SOH_PLAYER_ACTION_LEDGE` | Climbing a ledge |
| `SOH_PLAYER_ACTION_ITEM_ON_ACTOR` | Using an item on an actor: ocarina, spells, showing |

The template's form leaps instead of rolling while running:

```c
static int32_t LeapInsteadOfRolling(PlayState* play, Player* player) {
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) || !IsRunning(player)) {
        return SOH_FORM_ACTION_VANILLA;
    }
    func_80838940(player, (LinkAnimationHeader*)sLeapAnim, SWIFT_LEAP_VELOCITY, play, NA_SE_VO_LI_AUTO_JUMP);
    return SOH_FORM_ACTION_STARTED;
}

static const SOHFormActionOverride sActions[] = {
    { SOH_PLAYER_ACTION_ROLL, LeapInsteadOfRolling },
};
```

A move that must keep hitting like a sword calls `func_80842DF4` from its own action, the same way vanilla's
sword actions do: bouncing off shields, sparks on walls and damage reactions come for free.

## Loadout

With neither callback, Link keeps everything. What the form does not allow is **really unequipped in the save**
(the B button empties, the HUD and the equipment screen show it) and re-equipped when the form ends. The stash
travels with the save file, so saving while transformed loses nothing.

```c
static uint16_t NoSwordOrShield(int32_t equipType, uint16_t value) {
    if (equipType == EQUIP_TYPE_SWORD) {
        return EQUIP_VALUE_SWORD_NONE;
    }
    if (equipType == EQUIP_TYPE_SHIELD) {
        return EQUIP_VALUE_SHIELD_NONE;
    }
    return value;
}

static bool AllowsItem(uint16_t item, const char* customKey) {
    if (customKey != NULL) {
        return strcmp(customKey, "yourname.deku_leaf") == 0;
    }
    return item == ITEM_OCARINA_FAIRY || item == ITEM_OCARINA_TIME;
}
```

- `resolveEquipment` is called per `EQUIP_TYPE_*`: return the same value to keep it, `EQUIP_VALUE_*_NONE` to
  remove it, anything else to impose it.
- `allowsButtonItem` receives each C and D-pad item; a custom item arrives as `ITEM_CUSTOM` with its key.
- The form's own mask is never removed, whatever the callback says.
- Both are enforced every frame: the pause menu, a chest or the save editor can equip something again.

## The body

A form's model **mirrors the player object**: under `<modelPath>/object_link_boy/` (and `object_link_child/`) it
ships the vanilla resources it wants to replace, **with their vanilla names**. The skeleton keeps Link's 21
limbs, so every Link animation and all of Link's code keep working.

```
assets/objects/forms/swift/object_link_boy/gLinkAdultSkel
assets/objects/forms/swift/object_link_boy/gLinkAdultTorsoNearDL
assets/objects/forms/swift/object_link_boy/gLinkAdultEyesOpenTex
...
```

While drawing, the host swaps the skeleton and, limb by limb, uses the form's version of whatever vanilla chose.
Hands and sheath arrive as vanilla names, so a hand holding the sword or shield comes out right if the form ships
it. Eyes and mouth follow the same rule. What the form does not ship keeps drawing as vanilla — the sword in hand,
the sheath on the back — so no item needs to know forms exist. Link's tunic tint is not applied to a form's body.

The one measurement to take is the root scale: the root's position comes from the *animation*, not the skeleton,
so a rig with other proportions floats or sinks. The value is *how far the body hangs below the root* divided by
*the height the animation carries the root to*. As a child, the engine already scales the root by 0.64 before
the form does. Measure it; never copy another form's value.

## The transformation cutscene

Changing form plays Majora's Mask's cutscene: the sub-camera orbit, screen distortion, fog and light ramp, the
white flash. The form is applied at the flash peak. The world is frozen through the [time control
service](HOOKS.md#time-control), and Link is in an action of the host's own, so anything that takes the player
(damage, another cutscene, water) cuts the transformation cleanly and leaves the form applied.

The form supplies only `transformAnim`, `transformMask` and friends; without them the rest of the cutscene
still plays. A, B or C skips it (always when taking the mask off; when putting it on, from the second time in a
session). The `gEnhancements.Forms.InstantTransform` CVar disables it.

## What other mods see

The active form lives in the registry, not in `Player`. An NPC or item asks `GetActiveForm()` (key or `NULL`) or
`IsFormActive("yourname.swift")`. The active form is saved by key; if its mod is not installed on load, Link is
Link. Console: `form "<key>"`, `form link`.

## The hooks underneath

The registry applies a form through generic hooks any mod can use, filtered by id with
`SOH_REGISTER_HOOK_FOR_ID`:

| Hook | Id filter | Allows |
|---|---|---|
| `OnPlayerActionHandler` | action | Replacing any of Link's actions. |
| `OnPlayerResolveAnim` | group | Replacing any animation from an animation group. |
| `OnPlayerResolveAnimSite` | site | The animations outside the groups: `HOP` (`direction * 3 + phase`), `DAMAGE`, `FIDGET` (`FidgetType * 2 + column`), `FALL`. |
| `OnPlayerResolveAgeProperties` | — | The player's body. |
| `OnPlayerResolveMotionScale` | kind | Speeds: `STICK_SPEED` is a multiplier, `SPEED_CAP` the cap itself in world units. |
| `OnPlayerResolveHeight` | — | The body's height. |
| `OnPlayerFilterInput` | — | The copy of the pad vanilla Link reads. |
| `OnPlayerResolveLimbDraw` / `OnPlayerResolveFaceTextures` | — | Link's meshes and face. |
| `OnCollisionResolveDamage` | victim actor id | The final damage of a hit. |
| `OnInterfaceResolveButtonIcon` | — | A button's icon; an empty B asks too, for forms that fight bare-handed. |
| `OnActorPlaySfx` | actor id | Voice, footsteps, jumps and every other actor sound. |
| `OnOcarinaNote` / `OnOcarinaPlaybackNote` | — | Each played and replayed ocarina note, to give the form its own instrument. |

## The ocarina of a form

A form gives the ocarina its own voice from `OnOcarinaNote` (it runs on the audio thread: no CVars or game state
there) with `Audio_OcaSetInstrument`. Two rules of vanilla's ocarina action shape the pose:

- It opens the ocarina only when the intro clip **ends**, and waits for the outro clip on close. A form with a
  "take out the instrument" clip puts it there, played once, and vanilla's own wait handles it.
- While playing, **every time a clip reports it finished, the ocarina reopens** and the song so far is erased.
  The playing clip loops and stops one frame before its end; never a once-clip parked on its last frame.

## Complete forms to read

The [`unbound-mod-nei` branch](https://github.com/skijer/Shipwright/tree/unbound-mod-nei/mod-sdk/mods) has full
forms: Keaton (combo, charged fireball,
three-tail rig), Kafei (sprint, landmines, whistle), Goron, Zora, Deku, Fierce Deity, Rito, Wolf Link and Mario.
