/**
 * mm_asset_loader.h - MM Asset Detection and Loading
 *
 * C API for detecting and loading assets from mm.o2r
 *
 * MOD OVERRIDE SYSTEM:
 * Place a mod .o2r file alongside mm.o2r to override specific assets:
 *   - mm-mod.o2r     (primary)
 *   - mm-custom.o2r  (alternative)
 *   - mm-override.o2r (alternative)
 *
 * Assets in the mod file take priority over mm.o2r.
 * Use the same OTR paths as mm.o2r to replace specific DLs, icons, or textures.
 */

#ifndef MM_ASSET_LOADER_H
#define MM_ASSET_LOADER_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize MM asset detection
 * Checks for mm.o2r and loads it if found
 */
void MmAssets_Init(void);

/**
 * Check if mm.o2r is available (detected)
 * @return 1 if available, 0 if not
 */
u8 MmAssets_IsAvailable(void);

/**
 * Check if mm.o2r is loaded into archive manager
 * @return 1 if loaded, 0 if not
 */
u8 MmAssets_IsLoaded(void);

/**
 * Check if a mod .o2r override is loaded
 * Mod archives (mm-mod.o2r, mm-custom.o2r) override assets from mm.o2r
 * @return 1 if mod loaded, 0 if not
 */
u8 MmAssets_IsModLoaded(void);

/**
 * Get path to loaded mod .o2r file
 * @return Path string, or empty if no mod loaded
 */
const char* MmAssets_GetModPath(void);

/**
 * Get required 2Ship version string
 * @return Version string (e.g., "2Ship2Harkinian Keiichi Alfa 4.0.0")
 */
const char* MmAssets_GetRequiredVersion(void);

/**
 * Get path to mm.o2r file
 * @return Path string, or empty if not found
 */
const char* MmAssets_GetPath(void);

/**
 * Load a resource from mm.o2r
 * @param path Resource path (e.g., "objects/object_link_goron/gLinkGoronSkel")
 * @return Pointer to loaded resource, or NULL if not found
 */
void* MmAssets_LoadResource(const char* path);

// MUST be MM's copy: no mod overrides, no archive priority, no fallback — returns MM's resource or
// NULL. Use it for any path that also exists in oot.o2r (object_gi_hookshot, object_gi_zoramask,
// object_gi_golonmask, object_gi_ki_tan_mask, object_gi_rabit_mask, object_gi_truth_mask, …), where
// "by path" and "MM's" are different requests. Skijer's NEI
void* MmAssets_LoadResourceStrict(const char* path);

/**
 * Load a resource from mm.o2r and get its size
 * @param path Resource path (e.g., "objects/gameplay_keep/gPlayerAnim_...")
 * @param outSize Output: size in bytes of the resource data
 * @return Pointer to loaded resource, or NULL if not found
 */
void* MmAssets_LoadResourceWithSize(const char* path, size_t* outSize);

/**
 * Check if a specific resource exists in mm.o2r
 * @param path Resource path
 * @return 1 if exists, 0 if not
 */
u8 MmAssets_ResourceExists(const char* path);

/**
 * Load an MM skeleton (2Ship OSKL resource) ARCHIVE-SCOPED from mm.o2r.
 *
 * 2Ship writes skeletons in the exact same binary format as SoH (fourcc OSKL v0,
 * identical SkeletonFactory field order), so SoH's stock factory parses them natively.
 * The load is scoped to the mm.o2r archive (bypassing the global name index) so a
 * same-named OoT skeleton can never shadow it. NOTE: the factory resolves each limb
 * by GLOBAL name lookup — only use this for skeletons whose limb paths are MM-unique
 * (e.g. gStrayFairySkel); for colliding paths (gStalchildSkel etc.) load the OoT
 * version globally instead.
 *
 * Accepts paths with or without the "__OTR__" prefix.
 *
 * @param path e.g. "objects/gameplay_keep/gStrayFairySkel"
 * @return SkeletonHeader* / FlexSkeletonHeader* (per the resource's type), or NULL.
 */
void* MmAssets_LoadSkeleton(const char* path);

/**
 * Load an MM animation (2Ship OANM resource, same format as SoH's) ARCHIVE-SCOPED
 * from mm.o2r. Accepts paths with or without the "__OTR__" prefix.
 *
 * @param path e.g. "objects/gameplay_keep/gStrayFairyFlyingAnim"
 * @return AnimationHeader pointer (or LinkAnimationHeader / TransformUpdateIndex per type), or NULL.
 */
void* MmAssets_LoadAnimation(const char* path);

/**
 * List files matching a pattern from mm.o2r
 * @param searchMask Pattern (e.g., "audio/fonts*")
 * @param resultSize Output: number of matching files
 * @return Array of file paths (caller must free), or NULL
 */
char** MmAssets_ListFiles(const char* searchMask, int* resultSize);

/**
 * List files matching a pattern, scoped to the mm.o2r archive ONLY.
 * Unlike MmAssets_ListFiles (which uses the global ArchiveManager and so
 * sees every archive's contents), this iterates sMmArchive directly so the
 * result is guaranteed to be MM-only paths.
 *
 * @param searchMask Glob pattern (e.g. "audio/sequences*")
 * @param resultSize Output: number of matching files
 * @return Array of file paths (caller must free each + the array), or NULL.
 */
char** MmAssets_ListMmArchiveFiles(const char* searchMask, int* resultSize);

// =============================================================================
// Asset Replacement System (OOT → MM replacements)
// =============================================================================

/**
 * Asset replacement types
 */
typedef enum {
    MM_REPLACE_ICON = 0,  // Item icon (32x32 texture)
    MM_REPLACE_TEXT = 1,  // Item name text texture
    MM_REPLACE_MODEL = 2, // 3D model/display list
} MmReplaceType;

/**
 * Check if a specific replacement is active
 * @param cvarName CVar name (e.g., "gMods.TransformMasks.DekuReplacesSkull")
 * @return 1 if mm.o2r available AND CVar enabled, 0 otherwise
 */
u8 MmAssets_IsReplacementActive(const char* cvarName);

/**
 * Get MM replacement path for an OOT asset (if replacement is active)
 * Strips __OTR__ prefix from input and output for consistency
 * @param ootPath OOT asset path (with or without __OTR__ prefix)
 * @return MM path (with __OTR__ prefix) if replacement active, or NULL
 */
const char* MmAssets_GetReplacement(const char* ootPath);

// =============================================================================
// MM Masks Inventory (3rd Page) Icon/Name Loaders
// =============================================================================

/**
 * Load icon texture for any MM mask item
 * @param itemId Item ID (ITEM_MM_MASK_POSTMAN through ITEM_MM_MASK_FIERCE_DEITY)
 * @return Pointer to 32x32 RGBA icon texture, or NULL if not found
 */
void* MmMasks_LoadIcon(uint16_t itemId);

/**
 * Load name texture for any MM mask item
 * @param itemId Item ID (ITEM_MM_MASK_POSTMAN through ITEM_MM_MASK_FIERCE_DEITY)
 * @return Pointer to name texture, or NULL if not found
 */
void* MmMasks_LoadNameTex(uint16_t itemId);

/**
 * Get OTR path string for MM mask icon (for gDPLoadTextureBlock resolution).
 * Returns __OTR__ path so the RSP can resolve actual texture dimensions from
 * resource metadata, enabling HD mod textures to render at native resolution.
 */
const char* MmMasks_GetIconPath(uint16_t itemId);

/**
 * Get OTR path string for MM mask name texture.
 */
const char* MmMasks_GetNamePath(uint16_t itemId);

/**
 * Load FD sword icon for B-button HUD override
 * @return Pointer to 32x32 RGBA icon texture, or NULL if not found
 */
void* MmAssets_LoadFDSwordIcon(void);

/**
 * Load MM hookshot icon — used as the Clawshot mode icon (Twilight Upgrade).
 * Path: __OTR__icon_item_static_yar/gItemIconHookshotTex
 * @return Pointer to 32x32 RGBA icon texture, or NULL if not found
 */
void* MmAssets_LoadHookshotIcon(void);

/**
 * Load MM hookshot body DL (held in right hand) — used to render the
 * Clawshot's body in Link's hand when Clawshot mode is active.
 * Path: __OTR__objects/object_link_child/gLinkHumanRightHandHoldingHookshotDL
 * @return Gfx* (cast to void*), or NULL if not in mm.o2r
 */
void* MmAssets_LoadHookshotBodyDL(void);

/**
 * Load MM hookshot chain DL — used to render the chain segments when
 * the Clawshot is mid-air during a shoot.
 * Path: __OTR__objects/gameplay_keep/gHookshotChainDL
 * @return Gfx* (cast to void*), or NULL if not in mm.o2r
 */
void* MmAssets_LoadHookshotChainDL(void);

/**
 * Load MM hookshot reticle DL — used to render the first-person aim
 * reticle when Clawshot mode is active.
 * Path: __OTR__objects/gameplay_keep/gHookshotReticleDL
 * @return Gfx* (cast to void*), or NULL if not in mm.o2r
 */
void* MmAssets_LoadHookshotReticleDL(void);

/**
 * Load MM hookshot tip DL — the claw that flies through the air during
 * a shot. Distinct from the body DL (which stays in Link's hand).
 * Path: __OTR__objects/object_lbfshot/object_lbfshot_DL_000228
 * @return Gfx* (cast to void*), or NULL if not in mm.o2r
 */
void* MmAssets_LoadHookshotTipDL(void);

/**
 * MM-display-list-or-fallback selector. Returns mmDL when it is non-NULL,
 * otherwise vanillaDL. Collapses the repeated
 *   if (mm != NULL) { dl = mm; }
 * shape used in the MM-asset draw paths (e.g. the Clawshot tip/chain swap in
 * z_arms_hook.c). Pure; no loading or drawing — behavior identical to the
 * inline if-guard it replaces.
 */
Gfx* MmDL_Or(Gfx* vanillaDL, Gfx* mmDL);

/**
 * Load form-specific B-button icon (mask icon for each transformation)
 * @param form MM_PLAYER_FORM_* enum (0=FD, 1=Goron, 2=Zora, 3=Deku)
 * @return Pointer to 32x32 RGBA icon texture, or NULL if not found
 */
void* MmAssets_LoadFormBIcon(u8 form);

/**
 * Get OTR path string for Chateau Romani icon texture.
 * @return __OTR__ path string, or NULL if not available
 */
const char* MmAssets_GetChateauIconPath(void);

// =============================================================================
// Transformation Mask Asset Loaders
// =============================================================================

/**
 * Deku Mask assets (replaces Skull Mask)
 * Get Item: TWO DLs drawn with GetItem_DrawOpa0Xlu1 (Empty=Opa, Mask=Xlu)
 */
void* MmAssets_LoadDekuMaskIcon(void);
void* MmAssets_LoadDekuMaskNameText(void);
void* MmAssets_LoadDekuMaskEmptyDL(void); // First DL - empty (Opa)
void* MmAssets_LoadDekuMaskDL(void);      // Second DL - mask (Xlu)

/**
 * Stone Mask assets (replaces Spooky Mask)
 * Get Item: TWO DLs drawn with GetItem_DrawOpa0Xlu1 (Empty=Opa, Mask=Xlu)
 */
void* MmAssets_LoadStoneMaskIcon(void);
void* MmAssets_LoadStoneMaskNameText(void);
void* MmAssets_LoadStoneMaskEmptyDL(void); // First DL - empty (Opa)
void* MmAssets_LoadStoneMaskDL(void);      // Second DL - mask (Xlu)

/**
 * Fierce Deity Mask assets (replaces Gerudo Mask)
 * Get Item: TWO DLs drawn with GetItem_DrawOpa01 (both Opa)
 */
void* MmAssets_LoadFierceMaskIcon(void);
void* MmAssets_LoadFierceMaskNameText(void);
void* MmAssets_LoadFierceMaskFaceDL(void); // First DL - face (Opa)
void* MmAssets_LoadFierceMaskHairDL(void); // Second DL - hair/hat (Opa)

/**
 * Goron Mask assets
 * Get Item: TWO DLs drawn with GetItem_DrawOpa0Xlu1 (Empty=Opa, Mask=Xlu)
 */
void* MmAssets_LoadGoronMaskEmptyDL(void);
void* MmAssets_LoadGoronMaskDL(void);

/**
 * Zora Mask assets
 * Get Item: TWO DLs drawn with GetItem_DrawOpa01 (both Opa)
 */
void* MmAssets_LoadZoraMaskEmptyDL(void);
void* MmAssets_LoadZoraMaskDL(void);

/**
 * Worn Mask DLs (attached to Link's face when wearing)
 * DIFFERENT from Get Item DLs! These come from gameplay_keep or object_mask_*.
 * From 2Ship z_player_lib.c D_801C0B20[] array.
 */
void* MmAssets_LoadDekuMaskWornDL(void);   // gDekuMaskDL (gameplay_keep)
void* MmAssets_LoadStoneMaskWornDL(void);  // object_mask_stone_DL_000820
void* MmAssets_LoadFierceMaskWornDL(void); // gFierceDeityMaskDL (gameplay_keep)

// =============================================================================
// MM SFX Loader (Audio from mm.o2r)
// =============================================================================

/**
 * Check if MM audio is available
 * @return 1 if available, 0 if not
 */
s32 MmSfx_IsAvailable(void);

/**
 * Initialize MM SFX system
 */
void MmSfx_Init(void);

/**
 * Shutdown MM SFX system
 */
void MmSfx_Shutdown(void);

/**
 * Load a SoundFont from mm.o2r
 * @param fontId Font index (0-6)
 * @return Pointer to SoundFont, or NULL
 */
SoundFont* MmSfx_LoadFont(s32 fontId);

/**
 * Get SoundFont for a specific SFX ID
 * @param sfxId MM SFX ID
 * @return Pointer to SoundFont, or NULL
 */
SoundFont* MmSfx_GetFontForSfx(u16 sfxId);

/**
 * Play MM sound effect at position
 * @param sfxId MM SFX ID
 * @param pos World position (NULL for 2D)
 * @return 1 if played successfully, 0 if no valid sample (caller should use OOT fallback)
 */
s32 MmSfx_PlayAtPos(u16 sfxId, Vec3f* pos);

/**
 * Play MM sound effect with full control
 * @return 1 if played successfully, 0 if no valid sample
 */
s32 MmSfx_PlayEx(u16 sfxId, Vec3f* pos, u8 token, f32* freqScale, f32* vol, s8* reverbAdd);

/**
 * Stop a MM sound effect
 */
void MmSfx_Stop(u16 sfxId);

/**
 * Play Goron roll sound with MM's synced freq/vol mapping.
 * @param speed XZ speed param (matches MM's sp54/unk_B08 inputs).
 */
void MmSfx_PlayGoronRoll(Vec3f* pos, f32 speed);

/**
 * Play Goron charged roll sound with MM's synced freq/vol mapping.
 * From 2Ship line 19781: NA_SE_PL_GORON_CHG_ROLL when unk_B86[1] != 0
 */
void MmSfx_PlayGoronChgRoll(Vec3f* pos, f32 speed);

/**
 * Variants that honor the player's current floorSfxOffset — when the offset
 * matches MM's ice floor (0xF), the ICE-variant SFX is played
 * (NA_SE_PL_GORON_ROLL_ICE / NA_SE_PL_GORON_CHG_ROLL_ICE). Mirrors MM's
 * Player_GetFloorSfx(this, NA_SE_PL_GORON_ROLL) dispatch.
 */
void MmSfx_PlayGoronRollWithFloor(Vec3f* pos, f32 speed, u16 floorSfxOffset);
void MmSfx_PlayGoronChgRollWithFloor(Vec3f* pos, f32 speed, u16 floorSfxOffset);

/**
 * Play Goron charge sound with charge level pitch
 */
void MmSfx_PlayGoronCharge(Vec3f* pos, f32 chargeLevel);

/**
 * Play transformation mask flash sound
 */
void MmSfx_PlayTransformFlash(void);

/**
 * Get cache statistics
 */
void MmSfx_GetCacheStats(s32* outLoadedFonts, s32* outTotalBytes);

/**
 * Flush SFX cache
 */
void MmSfx_FlushCache(void);

// =============================================================================
// MM Direct Audio (bypass OOT SFX pipeline - decode ADPCM, mix into output)
// =============================================================================

/**
 * Mix MM direct audio sounds into the audio output buffer
 * Called from AudioMgr_CreateNextAudioBuffer after OOT synthesis
 * @param outBuf Interleaved stereo s16 buffer [L,R,L,R,...]
 * @param numSamples Number of stereo sample pairs
 */
void MmDirectAudio_MixInto(s16* outBuf, u32 numSamples);

/**
 * Stop all playing MM direct audio sounds
 */
void MmDirectAudio_StopAll(void);

/**
 * Play an instrument note for gakki (MM form-specific instruments)
 * Uses real MM soundfonts: Goron Drums (SF38), Zora Guitar (SF29), Deku Pipes (SF34)
 * @param form MM_PLAYER_FORM_GORON(1), ZORA(2), DEKU(3)
 * @param buttonIndex 0=A(D4), 1=CDown(F4), 2=CRight(A4), 3=CLeft(B4), 4=CUp(D5)
 * @param pos World position for spatial audio (NULL for 2D)
 */
void MmGakki_PlayNote(s32 form, u8 buttonIndex, Vec3f* pos);

/**
 * How a form's instrument is voiced — the per-form equivalent of MM's
 * sPlayerFormOcarinaInstruments (z_message.c:4560).
 *
 *   GAKKI_VOICE_NONE:    the plain ocarina. Nothing to suppress, nothing to synthesize
 *                        (MM's OCARINA_INSTRUMENT_DEFAULT fallback — Human/Fierce Deity).
 *   GAKKI_VOICE_NATIVE:  an instrument OoT's seq 0 already ships on its ocarina channel.
 *                        Selected via AudioOcarina_SetInstrument (MM's own mechanism);
 *                        the engine voices the notes, so NA_SE_OC_OCARINA must stay ON.
 *   GAKKI_VOICE_MM_FONT: an MM-only instrument synthesized from mm.o2r soundfonts.
 *                        NA_SE_OC_OCARINA is silenced and notes are driven off the
 *                        OnOcarinaNote hook (exact pitch incl. sharps/flats + bend).
 */
typedef enum {
    GAKKI_VOICE_NONE = 0,
    GAKKI_VOICE_NATIVE,
    GAKKI_VOICE_MM_FONT,
} MmGakkiVoiceType;

/** @return the form's MmGakkiVoiceType (GAKKI_VOICE_NONE when out of table range). */
s32 MmGakki_GetVoiceType(s32 form);

/**
 * Resource path of the form's instrument animations, or NULL when the form has none.
 *
 * ONE system for every form: MM's own forms load their gakki clips out of mm.o2r through
 * the MmAnim ids, while custom forms point at PlayerAnimation resources retargeted onto
 * Link's skeleton (baked by tools/bake_oot_npc_link_anims.py). Both end up in the same
 * gFormState.gakkiStartAnim/gakkiPlayAnim fields and are driven by the same code, so
 * adding a form is a table row — not another branch in the per-form loader.
 *
 * @param form MM_PLAYER_FORM_* value
 * @return "__OTR__…" path, or NULL to fall back to the MM clips / no animation
 */
const char* MmGakki_GetStartAnimPath(s32 form);
const char* MmGakki_GetPlayAnimPath(s32 form);

/**
 * Display list of the form's instrument, drawn in place of the hand limb while the
 * instrument is out, and the limb it attaches to.
 *
 * MM's forms bake their instrument into the form model, so they return NULL. Custom forms
 * name a DL from oot.o2r: the Gerudo uses Skull Kid's gSkullKidLeftHandAndFluteDL, which
 * holds hand AND flute in one list — the same hand the retargeted flute animation drives.
 *
 * @return "__OTR__…" DL path, or NULL when the form has no separate instrument model
 */
const char* MmGakki_GetInstrumentDL(s32 form);

/**
 * Sentinel returned by MmGakki_GetInstrumentDL for forms that play with NO instrument
 * model. The limb is redrawn with Link's EMPTY-hand DL, which drops the ocarina
 * and keeps the hand: OoT bakes the ocarina into the hand DL, so blanking the
 * limb outright would delete both. Point it at the hand that actually holds the
 * instrument (PLAYER_LIMB_R_HAND for the ocarina).
 * Distinct from NULL, which means "don't touch the rendering".
 */
#define GAKKI_DL_HIDE ((const char*)-1)

/** @return PLAYER_LIMB_* the instrument DL replaces (0 when the form has none). */
s32 MmGakki_GetInstrumentLimb(s32 form);

/** @return OCARINA_INSTRUMENT_* for GAKKI_VOICE_NATIVE forms, 0 otherwise. */
s32 MmGakki_GetNativeInstrument(s32 form);

/** @return the Soundfont_0 instrument the form's SONG is sung with, 0 when it has none. */
s32 MmGakki_GetSongInstrument(s32 form);

/** @return 1 when the form's voice type is not GAKKI_VOICE_NONE. */
s32 MmGakki_FormHasOwnInstrument(s32 form);

/**
 * Pitch-accurate gakki note (MM_FONT forms), driven from the OnOcarinaNote hook.
 * @param pitch OoT OcarinaPitch: semitones from C4 (C4=0 → MIDI 60+pitch), already
 *              including the Z/R sharp/flat modifiers.
 * @param bendFreq sCurOcarinaBendFreq (control-stick bend multiplier; pass 1.0f for none).
 * @param pos world position for spatial audio (NULL for 2D).
 */
void MmGakki_PlayPitch(s32 form, u8 pitch, f32 bendFreq, Vec3f* pos);

/** Keep the held gakki note alive (call once per frame while the pitch is held). */
void MmGakki_RefreshNote(void);

/** Note-off: release the current gakki note. */
void MmGakki_StopNote(void);

// Per-note settings a sequence carries and the plain ocarina does not.
typedef struct {
    f32 volumeScale; // the note's velocity, 0..1
    f32 reverb;      // the channel's reverb send, 0..1
    u8 pan;          // the channel's pan, 0..127 (64 = centre)
    u8 sustain;      // hold a loopless sample instead of letting it run out mid-note
} MmGakkiNoteShape;

/** Apply a sequence's note settings to the gakki note just started. */
void MmGakki_ShapeActiveNote(const MmGakkiNoteShape* shape);

/** Note-off through the instrument's own release, instead of cutting the sound dead. */
void MmGakki_ReleaseNote(void);

/** Sing a note with a named Soundfont_0 instrument rather than the form's gakki row. */
void MmGakki_PlayInstrumentPitch(u8 instIdx, u8 pitch, f32 bendFreq, Vec3f* pos);

#ifdef __cplusplus
}
#endif

#endif // MM_ASSET_LOADER_H
