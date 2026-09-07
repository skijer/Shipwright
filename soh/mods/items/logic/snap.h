/**
 * snap.h - Pictograph Box (Skijer's NEI) — Majora's Mask photo validation, ported to OoT.
 *
 * The pictograph VALIDATION + FLAG layout are MM's, verbatim (mm/src/code/z_snap.c +
 * include/z64snap.h). The only port change: MM keys validation off a per-actor PictoActor.
 * validationFunc; OoT actors have no such field, so the record loop keys off actor->id via a
 * central subject table (snap.c). Same checks, same flags, OoT-compatible.
 *
 * The pictograph has NO in-OoT use — flags/photo are written in MM's exact save layout
 * (Nei_Save()->pictoFlags0/1 + pictoPhotoI5) so a 2Ship bridge can consume them.
 *
 * #included into a host TU (custom_items.c); the CMake mods glob is *.cpp/*.h only.
 */
#ifndef NEI_SNAP_H
#define NEI_SNAP_H

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

// === Picto validation flags (verbatim from MM include/z64snap.h) ===
typedef enum {
    // Set/read for actors (persisted in pictoFlags0/1)
    /* 0x00 */ PICTO_VALID_0,
    /* 0x01 */ PICTO_VALID_IN_SWAMP,
    /* 0x02 */ PICTO_VALID_MONKEY,
    /* 0x03 */ PICTO_VALID_BIG_OCTO,
    /* 0x04 */ PICTO_VALID_LULU_HEAD,
    /* 0x05 */ PICTO_VALID_LULU_RIGHT_ARM,
    /* 0x06 */ PICTO_VALID_LULU_LEFT_ARM, // all three needed to qualify
    /* 0x07 */ PICTO_VALID_SCARECROW,
    /* 0x08 */ PICTO_VALID_TINGLE,
    /* 0x09 */ PICTO_VALID_PIRATE_GOOD,
    /* 0x0A */ PICTO_VALID_DEKU_KING,
    /* 0x0B */ PICTO_VALID_PIRATE_TOO_FAR, // overlaps PIRATE_GOOD; checked second

    // Internal failure modes (transient)
    /* 0x3B */ PICTO_VALID_BEHIND_COLLISION = 0x3B,
    /* 0x3C */ PICTO_VALID_BEHIND_BG,
    /* 0x3D */ PICTO_VALID_NOT_IN_VIEW,
    /* 0x3E */ PICTO_VALID_BAD_ANGLE,
    /* 0x3F */ PICTO_VALID_BAD_DISTANCE
} PictoValidFlag;

// On-screen subregion that counts an actor as "in the photo" (verbatim from MM z64snap.h).
#define PICTO_VALID_WIDTH 150
#define PICTO_VALID_HEIGHT 105
#define PICTO_VALID_TOPLEFT_X ((SCREEN_WIDTH - PICTO_VALID_WIDTH) / 2)
#define PICTO_VALID_TOPLEFT_Y ((SCREEN_HEIGHT - PICTO_VALID_HEIGHT) / 2)

// Custom message id for the "Keep this picture?" 2-choice prompt (MM's 0xF8). Registered by the
// OnOpenText hook in picto_message.cpp; opened from picto_box.c via Message_StartTextbox.
#define PICTO_KEEP_TEXTID 0x6F08
// (0x6F09 was a SOH-only "you already have a pictograph, replace it?" warn. MM overwrites the stored
// photo without asking, so the prompt is gone and the id stays free.)
// Gag message shown when an MM trade-quest item is USED (trade_items.c present flow). Registered in
// picto_message.cpp. "Oak's words echoed... There's a time and place for everything, but not now."
#define MM_TRADE_USE_TEXTID 0x6F0A

// Photo image dimensions / storage (verbatim from MM include/z64save.h).
#define PICTO_PHOTO_WIDTH 160
#define PICTO_PHOTO_HEIGHT 112
#define PICTO_PHOTO_TOPLEFT_X ((SCREEN_WIDTH - PICTO_PHOTO_WIDTH) / 2)   // 80
#define PICTO_PHOTO_TOPLEFT_Y ((SCREEN_HEIGHT - PICTO_PHOTO_HEIGHT) / 2) // 64
#define PICTO_PHOTO_SIZE (PICTO_PHOTO_WIDTH * PICTO_PHOTO_HEIGHT)        // 17920 (I8)
#define PICTO_PHOTO_COMPRESSED_SIZE (PICTO_PHOTO_SIZE * 5 / 8)           // 11200 (I5)

// === Engine (snap.c) ===

// Flag bit-ops on Nei_Save()->pictoFlags0/1 (MM Snap_SetFlag/UnsetFlag/CheckFlag, retargeted).
void Snap_SetFlag(s32 flag);
void Snap_UnsetFlag(s32 flag);
u32 Snap_CheckFlag(s32 flag);

// Verbatim MM validation: distance / angle / in-capture-region / not-behind-bg / not-occluded.
// Returns 0 (sets `flag`) on success, else an or'd combination of the failure flag indices.
s32 Snap_ValidatePictograph(PlayState* play, Actor* actor, s32 flag, Vec3f* pos, Vec3s* rot, f32 distanceMin,
                            f32 distanceMax, s16 angleRange);

// Clears pictoFlags0/1, then sweeps loaded actors and validates each mapped subject (central
// actor->id table in snap.c). Returns the count validly captured. Call at the moment of capture.
s32 Snap_RecordPictographedActors(PlayState* play);

// === Image pipeline (picto_box.c) ===
// Shutter: validate subjects now + queue the deferred framebuffer capture. Call on capture press.
void Picto_TakePhoto(PlayState* play);
// DRAW hook: emits the framebuffer readback when a capture is queued (call during player draw).
void Picto_EmitCapture(PlayState* play, Gfx** gfxp);
// DRAW hook (OVERLAY): shows the captured photo for a few seconds after the shutter (color preview).
void Picto_DrawPhoto(PlayState* play, Gfx** gfxp);
// UPDATE hook: one frame later, converts+compresses the readback into Nei_Save()->pictoPhotoI5.
// Called from z_play.c, NOT from the player actor: the shutter sets play->haltAllActors like MM does,
// which stops every actor (Link included), and this state machine has to keep running behind the photo.
void Picto_Update(PlayState* play);

// Item ownership (granted via menu / save editor) + a menu-driven debug shutter.
u8 Picto_IsOwned(void);
void Picto_SetOwned(u8 on);
void Picto_TakePhotoNow(void);
// Throw the stored picture away (menu). With a picture stored, the pictograph button shows THAT
// picture + the keep/discard prompt instead of opening the lens — MM's own behaviour.
void Picto_ClearPhoto(void);

// "Pictobox mode" on the Lens-of-Truth slot, toggled by the kaleido wheel (z_kaleido_item.c).
u8 Picto_IsOnLensActive(void);
void Picto_SetOnLensActive(u8 on);

// Enter the photo viewfinder (called from Player_UseItem when the Lens slot's pictobox mode fires).
void Picto_EnterAimMode(PlayState* play);

#ifdef __cplusplus
}
#endif

#endif // NEI_SNAP_H
