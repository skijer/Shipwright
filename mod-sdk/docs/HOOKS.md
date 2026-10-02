# Hooks and services

Items, forms and actors cover *what* your mod adds. Hooks and services cover *when* it acts and how it shares
the game with other mods.

## Hooks

A hook is a point in the engine that calls every subscriber. Every name and signature is in
`soh/soh/Enhancements/game-interactor/GameInteractor_HookTable.h`; list the ones you use in
`ModGetRequirements`.

```c
SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickMyItem);
SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawAfterLink);
```

`SOH_REGISTER_HOOK_FOR_ID` filters by an id (usually an actor id). It only works on hooks whose engine side
dispatches by id: the actor hooks (`OnActorInit`, `OnActorUpdate`, `OnActorDraw`, `OnActorDrawEnd`,
`OnActorPlaySfx`, `OnCollisionResolveDamage`…) and the player hooks listed in [Forms](FORMS.md#the-hooks-underneath).

The ones most mods need:

| Hook | When | Typical use |
|---|---|---|
| `OnSceneInit(sceneNum)` | A new scene starts. | Forget camera ids, light nodes and actor pointers from the last scene. |
| `OnPlayerUpdate()` | End of the player update, after the action. | The last word on Link this frame; start player actions here. |
| `OnPlayerPostLimbDraw(play, player, limb)` | After each of Link's limbs is drawn. | Draw things in Link's hands with the limb's matrix. |
| `OnActorDrawEnd(actor, play)` | After an actor is drawn. | Draw over an actor (filter by `ACTOR_PLAYER` for Link). |
| `OnPlayDrawEnd()` | After the world is drawn. | Full-screen effects. |
| `OnInterfaceDrawEnd(play)` | After the HUD, in a 320×240 orthographic space. | Custom HUD, radial menus, maps. |
| `OnPlayerFilterInput(player, input)` | Before vanilla Link reads the pad. | Hide buttons from Link while your mod reads them. |
| `OnActorInit` / `OnActorUpdate` | Around any actor's own functions. | Change vanilla actors without replacing them. |
| `OnResolveSwordDamage` / `OnCollisionResolveDamage` | Damage computation. | Weapons and resistances. |
| `OnResolveCustomGetItem` | A custom pickup is about to be announced. | Decide what a variable pickup gives. |
| `OnLoadGame(fileNum)` / `OnSaveFile(fileNum, section)` | A save is loaded / written. | Reset or persist per-file state. |
| `OnGameFrameUpdate()` | Once per game frame, anywhere. | Refresh state that is not tied to gameplay. |

Hook callbacks run on the game thread, in registration order, after the game's own subscribers.

## Vanilla behaviours

A vanilla behaviour (`VB_*`) is a yes/no question the engine asks at a decision point: "should Link crawl?",
"should the pause menu open?", "can Link swim here?". Each entry in
`soh/soh/Enhancements/game-interactor/vanilla-behavior/GIVanillaBehavior.h` documents its default result and the
arguments you receive.

```c
static void AllowCrawlWhileGripping(bool* should, va_list args) {
    if (IsGripping()) {
        *should = true;
    }
}

sApi->RegisterVB(VB_CRAWL, AllowCrawlWhileGripping);
```

Mod callbacks run after every built-in handler (the randomizer's included), so a mod has the final say.

## Services

What the host arbitrates between mods that do not know each other.

### Time control

```c
sApi->RequestTimeControl("yourname.stasis", 5000, 0.0f, true);
sApi->ReleaseTimeControl("yourname.stasis");
float speed = sApi->GetWorldSpeed();
```

`worldSpeed` 0 stops every actor except Link, his projectiles and their children; between 0 and 1 slows them.
`freezeClock` stops the day clock. The highest priority claim wins; stopped actors stay hittable. Release your
claim when you are done and on scene change.

### Player input

```c
sApi->BlockPlayerInput("yourname.map", BTN_A | BTN_B, true);
sApi->ReleasePlayerInput("yourname.map");
```

Vanilla Link stops reading those buttons (and the stick) while any owner blocks them. Your mod still reads the raw
pad in `play->state.input[0]`. A screen of your own also answers `VB_OPEN_PAUSE_MENU` with `false` so Start does
not open the pause menu on top of it.

### Hazards and countdown

`RequestHazard(owner, priority, PLAYER_ENV_HAZARD_*, requiredTunic)` is the last word on what the environment
does to Link: `HOTROOM` burns unless he wears `requiredTunic` (pass `EQUIP_VALUE_TUNIC_KOKIRI` for "no tunic
helps"), `NONE` waives what the scene was doing. `StartCountdown(owner, seconds)` runs vanilla's countdown clock,
one owner at a time.

### Audio

```c
static void MixMySound(int16_t* samples, uint32_t frameCount) {
}

sApi->RegisterAudioMixInGroup(MixMySound, SOH_AUDIO_GROUP_SFX, "gMods.MyMod.Volume");
```

The callback runs **on the audio thread** with a zeroed buffer of its own (interleaved stereo `s16`). Write your
samples at full scale; the host applies the game's volume slider for the group, your own CVar (0-100, optional)
and mixes you in. Touch nothing else there: no CVars, no game state, no allocation. Hand data to it from the game
thread through atomics.

### Storage

`StorageSet(mod, key, data, size)` / `StorageGet` / `StorageSetString` / `StorageGetString` / `StorageRemove`
persist small values per mod, outside save files (settings, unlocks shared across files). Per-file state belongs
in your own save hooks or in custom randomizer flags ([Randomizer](RANDOMIZER.md#flags-of-your-own)).

### Menus

```c
SOHModMenuWidget volume = { sizeof(SOHModMenuWidget) };
volume.section = "Enhancements";
volume.sidebar = "My Mod";
volume.type = SOH_MOD_MENU_INT_SLIDER;
volume.label = "Volume";
volume.cvar = "gMods.MyMod.Volume";
volume.defaultInt = 100;
volume.maxInt = 100;
sApi->RegisterMenuWidget(&volume);
```

`RegisterMenuSidebar` adds a sidebar, `RegisterMenuWidgetAt` places a widget before or after an existing one.
`SOH_MOD_MENU_CUSTOM` calls `onChange` as a draw callback, where `ModUI_Checkbox`, `ModUI_InputInt`, `ModUI_Text`,
`ModUI_BeginSection`… draw with the game's own UI. The save editor offers `OnSaveEditorTabs`,
`OnSaveEditorInventory` and `OnSaveEditorItemPicker` for tabs and inventory pages of your own.

### Pause layouts

`RegisterLayoutPage` adds a page to the pause menu with its own grid, `PlaceLayoutItem(itemKey, pageKey, cell)`
sets default cells (also for vanilla entries, `vanilla.<kind>.<slot>`, to regroup the OoT grid into wheels), and
`RegisterLayoutVanilla` shows a vanilla slot on your page. Players rearrange everything in *Mods Items Layout*.

### Services between mods

Mods do not link against each other. A mod offers a function by name, and others look it up when they use it:

```c
typedef bool (*MmSfxPlayFunc)(uint16_t sfxId, Vec3f* pos);

sApi->PublishService("mm.sfx.play", (void*)PlaySfx);

MmSfxPlayFunc play = (MmSfxPlayFunc)sApi->FindService("mm.sfx.play");
if (play != NULL) {
    play(sfxId, &actor->projectedPos);
}
```

Look services up **when you use them**, not in `ModInit`: load order is not guaranteed. A name belongs to the first
mod that publishes it. The consumer copies the signature, since there is no shared header, and handles `NULL`.

## Engine traps worth knowing

- An action func set from anywhere inside the player's action-handler pass (an item's `init`, its upper action)
  is overwritten the same frame. Start player actions from `OnPlayerUpdate` or from a form's action handler.
- `Magic_RequestChange(play, cost, MAGIC_CONSUME_NOW)` leaves the magic bar flashing until `Magic_Reset(play)`.
  Call it once when your spell ends (and if the scene changes mid-spell), never every frame.
- A custom sub-camera makes vanilla ignore item buttons while it is active (`activeCamera != MAIN_CAM`). Read your
  buttons yourself while you own a camera, and give it back with
  `Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_ACTIVE)`.
- Rotations of Link's focus whose `unk_6AE_rotFlags` bit is not set this frame are eased back to zero: set the bit
  every frame you drive them.
- C++ mods: `OPEN_DISPS` redeclares `FrameInterpolation_RecordOpenChild`/`RecordCloseChild` inside the function.
  In an anonymous namespace that names a C++-mangled function nobody defines (`LNK2019 ...@@YAXPEBXH@Z`). Declare
  both `extern "C"` at file scope **and** inside the namespace, or draw from functions outside it.
- `R_UPDATE_RATE` is 3: timers copied from vanilla run at 20 Hz of game logic; keep them as they are.
- `Actor_SetColorFilter` with a gray filter and no `0x8000` flag plays its sound every frame.
- Anything that changes a struct shared with the game must be built with the SDK's compile definitions (the SDK
  target sets them). Built without them, `Vtx` has the wrong size (giant models) and the pad's stick reads as zero.
