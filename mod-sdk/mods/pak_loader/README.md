# PAK Loader for Unbound

Experimental appearance mod. Open **Mods > PAK Loader** after restarting Unbound.

Three independent selections change the appearance of ordinary Child Link, ordinary Adult Link,
and the `nei.fierce_deity` form. Other forms keep their own models. No form is activated or replaced:
the Fierce Deity mod retains its actions, damage, beams, equipment restrictions, movement and actor scale.

With `mm.o2r` loaded, **Apply MM Child / MM FD / OoT Adult** selects:

1. Child Link: `objects/object_link_child/gLinkHumanSkel`.
2. Adult Link: `objects/object_link_boy/gLinkFierceDeitySkel`.
3. Fierce Deity: `objects/object_link_boy/gLinkAdultSkel`.

Selections and root adjustments are saved in configuration. Default restores each slot independently.
Root height is estimated from bind-pose proportions and can be adjusted per slot; it does not alter gameplay
height or the Fierce Deity actor's 1.5 scale.

## Menu

The two-column page uses native `SOH_MOD_MENU_CHECKBOX`, `SOH_MOD_MENU_COMBOBOX` and
`SOH_MOD_MENU_FLOAT_SLIDER` registrations. The host renders them through `mSohMenu->AddWidget`,
with its native options, spacing, reset controls and tooltips. Model selectors are no longer drawn in a
custom panel. The left column contains enable, three model selectors, preset and restore; the right
column contains library installation, root adjustments and status. Labels for built-in models omit paths.

The current ModApi has no native button type or operation for updating a registered combobox. Individual
action rows therefore use the host's themed `ModUI_Button`; importing/installing models populates the
native lists on the next launch. A restart action appears after a successful save. Stable model keys are
mapped to the native integer combo values each launch, preserving selections when catalog order changes.

## Model sources

- Mounted O2R/OTR archives: the catalog lists OoT Adult/Child and MM Human/Fierce Deity skeleton paths,
  including models under custom directories using those skeleton names. Models must retain the 21-limb
  human Link hierarchy and the original flex matrix slots. Meshes, joints, matching equipment variants,
  and face textures are resolved from the selected model directory.
- **Install O2R / OTR** copies the archive into the game's mods folder through the host file picker
  and save confirmation. Restart to load it and populate the native model lists. The host mounts archives
  globally: a pack replacing original resource paths also replaces base-game resources for other consumers.
  Use unique resource directories for independent packs.
- **Import PAK / ZOBJ** reads ModLoader64 UNCO and zlib DEFL entries in memory. It accepts human Link
  bodies with a MODLOADER64 manifest and segment-06 skeleton. Native vertices are expanded to Unbound's
  s32 format, fixed-point matrices become floats, and display lists/embedded face textures remain alive
  for the session. Old adult zzplayas and universal equipment aliases have partial support.
- Importing offers the host's save dialog for an asset-only `pakloader_import_<hash>.o2r` containing
  the original file. Saved imports are restored through the archive API next launch. Restart updates the native model selectors. Declining the save
  leaves the lists unchanged; import again and allow saving to install it. Standalone ZOBJ source age is inferred from its filename:
  name child/MM Human files accordingly. Every imported body can be selected in every destination slot.

## Boundaries

This is not full parity with NEI's engine-integrated loader. It does not import voices, equipment-only
packs, per-item equipment mixes, arbitrary rigs, external display lists, or Z64Online environment segments.
An unsupported body is rejected with a status message. Hands and sheaths fall back to the correct OoT
equipment when the chosen pack lacks a matching variant. Nested weapon/boot display lists are not globally
intercepted. The mod changes the local player; it does not distribute appearances over multiplayer.

Fierce Deity lazily asks these optional services, regardless of mod load order:

- `nei.pak_loader.fd_body.v1`: `bool(Player*, int32_t, Gfx**, Gfx*, Vec3f*)`.
- `nei.pak_loader.fd_face.v1`: `bool(const char**, const char**)`.
- `nei.pak_loader.fd_active.v1`: `bool(void)`; suppresses FD's additional original sword drawing.

Without the provider or a valid selection, the existing Fierce Deity rendering remains active.

## Validation

Built against Unbound's headers/import library with `build-all.local.ps1 -Only Mods -Reconfigure`.
Native checks use actual `oot.o2r` and `mm.o2r` skeleton data: all four supported base skeletons share the
21-limb topology and flex matrix slots. Archive checks cover stored/fixed/dynamic DEFL streams, corrupt
input, native s32 vertex conversion and recursive display-list rejection. Visual validation in gameplay,
including individual third-party PAKs, remains required.
