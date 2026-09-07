/**
 * picto_box.c - Pictograph Box image pipeline + capture (Skijer's NEI).
 *
 * Ports MM's photo conversion VERBATIM (mm/src/code/z_play.c):
 *   - Play_ConvertRgba16ToIntensityImage  -> Picto_ConvertRgba16ToIntensityImage
 *   - Play_CompressI8ToI5                 -> Picto_CompressI8ToI5
 * and feeds them real pixels via SOH's FB_WriteFramebufferSliceToCPU (the engine's
 * "used by picto box" 320x240 RGBA16 readback). The readback is DEFERRED (gDPReadFB runs when the
 * frame's GBI list is processed), so capture is a small state machine: emit during DRAW frame N,
 * read the CPU buffer in UPDATE frame N+1.
 *
 * Validation (flags) runs the instant the shutter is pressed (actor positions are accurate then),
 * independent of the image. Everything is written in MM's exact save layout (Nei_Save()->pictoFlags
 * + pictoPhotoI5) for a 2Ship bridge; OoT itself gives no reward. #included into custom_items.c.
 */
#include "snap.h"
#include "../../nei_save.h"
#include "../helpers/camera_helper.h" // FirstPerson_* for the aim mode

extern void FB_WriteFramebufferSliceToCPU(Gfx** gfxp, void* buffer, u8 byteSwap);
extern void* MmAssets_LoadResource(const char* path); // MM viewfinder textures from mm.o2r
extern void Picto_SyncWrite(void);                    // mirror the kept photo to the OoT<->MM shared sidecar
extern void Picto_SyncClear(void);                    // delete it again when the picture is thrown away
// The COLOUR half of the print. The I5 buffer both games exchange is greyscale by construction, so
// the RGBA copy travels in its own file — shared in a combo run, per-save in a solo one.
extern void Picto_SyncWriteColor(const void* rgba16, int size);
extern int Picto_SyncReadColor(void* rgba16, int size);

// MM viewfinder textures (parameter_static): corner border (IA4 16x16), crosshair (I4 32x16),
// "PICTBOX" label (I4 32x8). Loaded once; NULL falls back to a code-drawn bracket viewfinder.
static void* sVfBorder = NULL;
static void* sVfIcon = NULL;
static void* sVfText = NULL;
static u8 sVfTried = 0;
static void Picto_LoadViewfinderTextures(void) {
    if (sVfTried) {
        return;
    }
    sVfTried = 1;
    sVfBorder = MmAssets_LoadResource("__OTR__parameter_static/gPictoBoxFocusBorderTex");
    sVfIcon = MmAssets_LoadResource("__OTR__parameter_static/gPictoBoxFocusIconTex");
    sVfText = MmAssets_LoadResource("__OTR__parameter_static/gPictoBoxFocusTextTex");
}

// --- MM intensity/color macros (verbatim from MM z_play.c / color.h) ---
#define PLAY_INTENSITY_RED 2
#define PLAY_INTENSITY_GREEN 4
#define PLAY_INTENSITY_BLUE 1
#define PLAY_INTENSITY_NORM (0x1F * PLAY_INTENSITY_RED + 0x1F * PLAY_INTENSITY_GREEN + 0x1F * PLAY_INTENSITY_BLUE)
#define PLAY_INTENSITY_MIX(r, g, b, m) \
    ((((r)*PLAY_INTENSITY_RED + (g)*PLAY_INTENSITY_GREEN + (b)*PLAY_INTENSITY_BLUE) * (m)) / PLAY_INTENSITY_NORM)
#define PICTO_RGBA16_GET_R(pixel) (((pixel) >> 11) & 0x1F)
#define PICTO_RGBA16_GET_G(pixel) (((pixel) >> 6) & 0x1F)
#define PICTO_RGBA16_GET_B(pixel) (((pixel) >> 1) & 0x1F)

#define PLAY_COMPRESS_BITS 5
#define PLAY_DECOMPRESS_BITS 8

// Verbatim MM Play_ConvertRgba16ToIntensityImage (only the bit depths the picto box uses).
static void Picto_ConvertRgba16ToIntensityImage(void* destI, u16* srcRgba16, s32 rgba16Width, s32 pixelLeft,
                                                s32 pixelTop, s32 pixelRight, s32 pixelBottom, s32 bitDepth) {
    s32 i;
    s32 j;
    u32 pixel;
    u32 r;
    u32 g;
    u32 b;

    if (bitDepth == 8) {
        u8* destI8 = destI;
        for (i = pixelTop; i <= pixelBottom; i++) {
            for (j = pixelLeft; j <= pixelRight; j++) {
                pixel = srcRgba16[i * rgba16Width + j];
                r = PICTO_RGBA16_GET_R(pixel);
                g = PICTO_RGBA16_GET_G(pixel);
                b = PICTO_RGBA16_GET_B(pixel);
                *(destI8++) = PLAY_INTENSITY_MIX(r, g, b, 255);
            }
        }
    } else if (bitDepth == 16) {
        // ColorPictograph (BenUI): keep the raw RGBA16 instead of intensity.
        u16* destI16 = destI;
        for (i = pixelTop; i <= pixelBottom; i++) {
            for (j = pixelLeft; j <= pixelRight; j++) {
                *(destI16++) = srcRgba16[i * rgba16Width + j];
            }
        }
    }
}

// Verbatim MM Play_CompressI8ToI5: packs five 5-bit pixels into eight bits.
static void Picto_CompressI8ToI5(void* srcI8, void* destI5, size_t size) {
    u32 i;
    u8* src = srcI8;
    s8* dest = destI5;
    s32 bitsLeft = PLAY_DECOMPRESS_BITS;
    u32 destPixel = 0;
    s32 shift;
    u32 srcPixel;

    for (i = 0; i < size; i++) {
        srcPixel = *src++;
        srcPixel = (srcPixel * 0x1F + 0x80) / 0xFF;
        shift = bitsLeft - PLAY_COMPRESS_BITS;
        if (shift > 0) {
            destPixel |= srcPixel << shift;
        } else {
            destPixel |= srcPixel >> -shift;
            *dest++ = destPixel;
            shift += PLAY_DECOMPRESS_BITS;
            destPixel = srcPixel << shift;
        }
        bitsLeft = shift;
    }

    if (bitsLeft < PLAY_DECOMPRESS_BITS) {
        *dest = destPixel;
    }
}

// Verbatim MM Play_DecompressI5ToI8: unpacks the stored photo back to 8bpp for display. Needed for
// MM's "you already have a picture" flow — the button shows the SAVED photo, which only exists as I5.
static void Picto_DecompressI5ToI8(void* srcI5, void* destI8, size_t size) {
    u32 i;
    u8* src = srcI5;
    s8* dest = destI8;
    s32 bitsLeft = PLAY_DECOMPRESS_BITS;
    u32 destPixel;
    s32 shift;
    u32 srcPixel = *src++;

    for (i = 0; i < size; i++) {
        shift = bitsLeft - PLAY_COMPRESS_BITS;
        if (shift > 0) {
            destPixel = 0;
            destPixel |= srcPixel >> shift;
        } else {
            destPixel = 0;
            destPixel |= srcPixel << -shift;
            srcPixel = *src++;
            shift += PLAY_DECOMPRESS_BITS;
            destPixel |= srcPixel >> shift;
        }
        destPixel = (destPixel & 0x1F) * 0xFF / 0x1F;
        *dest++ = destPixel;
        bitsLeft = shift;
    }
}

// --- Pictograph state (MM PICTO_BOX_STATE: LENS aiming -> shutter -> SETUP capture -> PHOTO + keep) ---
// Deferred framebuffer readback (gDPReadFB runs when the GBI list is processed): emit during DRAW
// frame N, read the CPU buffer in UPDATE frame N+1.
typedef enum { PICTO_CAP_IDLE, PICTO_CAP_EMIT, PICTO_CAP_PROCESS } PictoCaptureState;
static PictoCaptureState sPictoCapState = PICTO_CAP_IDLE;

static u8 sPictoAimActive = 0;   // first-person viewfinder (MM PICTO_BOX_STATE_LENS)
static u8 sPictoPhotoActive = 0; // captured photo + keep/discard prompt shown (MM PICTO_BOX_STATE_PHOTO)
static u8 sPictoOnLens = 0;      // "pictobox mode" on the Lens slot (kaleido wheel)
// The press that opens the lens must never also fire the shutter. A one-frame guard was not enough:
// the button can still be HELD when the state machine first ticks (and the entry runs from the player
// update, one step behind this tick), so the shutter is armed by RELEASE — no shutter until a frame
// where neither A nor the pictograph's own button is down — plus a couple of settling frames so
// first-person is fully engaged before anything can be captured. Skijer's NEI
static u8 sPictoArmWait = 0;    // waiting for the entry button to come back up
static u8 sPictoLensFrames = 0; // frames the lens has been up (shutter needs >= 2)
// MM's sPictoPhotoBeingTaken: 1 = the photo on screen was just shot (keeping it writes the save),
// 0 = it is the photo already in the save, put back on screen by pressing the pictograph button.
static u8 sPictoPhotoBeingTaken = 0;
// The RGBA16 copy only exists for a freshly captured photo — the save holds I5 only, so a stored
// photo can only be shown in MM's sepia. Gates the ColorPictograph display.
static u8 sPictoColorValid = 0;

// 320x240 RGBA16 readback; the 160x112 I8 scratch (grayscale save) and the contiguous RGBA16 color
// copy (the on-screen ColorPictograph display).
static u16 sPictoFrameRgba16[SCREEN_WIDTH * SCREEN_HEIGHT];
static u8 sPictoI8[PICTO_PHOTO_SIZE];
static u16 sPictoColorTex[PICTO_PHOTO_WIDTH * PICTO_PHOTO_HEIGHT];

// pictoFlags before the shutter — restored if the photo is discarded. MM records the subjects when
// the photo is KEPT, but it freezes the world at the shutter (haltAllActors), so nothing can move in
// between: recording at the shutter with the same halt in place is the same photo. Discarding rolls
// them back, which is what MM's "the picture was never kept" amounts to.
static u32 sPictoPrevFlags0 = 0;
static u32 sPictoPrevFlags1 = 0;

// HUD takeover, 1:1 with MM (z_parameter.c PICTO_BOX_STATE_LENS): while the lens is up B reads "Stop"
// and the rest of the interface fades out; the pictograph owns the screen.
static void Picto_HudLensOn(PlayState* play) {
    Interface_LoadActionLabelB(play, DO_ACTION_STOP); // also sets unk_1FA -> B draws the label
    Interface_ChangeHudVisibilityMode(10);            // only B stays lit (MM: HUD_VISIBILITY_A_B)
}

// Back to normal play: B shows the equipped item again and the whole HUD fades back in.
static void Picto_HudRestore(PlayState* play) {
    play->interfaceCtx.unk_1FA = 0;
    Interface_ChangeHudVisibilityMode(50); // HUD_VISIBILITY_ALL
}

// Shutter (MM: NA_SE_SY_CAMERA_SHUTTER + haltAllActors + SETUP_PHOTO). Freeze the world, validate the
// subjects on that frozen frame, remember the previous flags for a possible discard, and queue the
// framebuffer readback.
static void Picto_Shutter(PlayState* play) {
    extern s32 MmSfx_IsAvailable(void);
    extern s32 MmSfx_PlayAtPos(u16 sfxId, Vec3f * pos);
    // MM stops EVERY actor the instant the shutter fires and only resumes once the keep/discard
    // choice is made — the world behind the photo is dead still. OoT has the exact same switch
    // (z_play.c gates Actor_UpdateAll on it), so this is the real thing, not a player-state trick.
    // Picto_Update runs from z_play.c precisely so it keeps ticking while everything is halted.
    play->haltAllActors = true;
    sPictoPrevFlags0 = Nei_Save()->pictoFlags0;
    sPictoPrevFlags1 = Nei_Save()->pictoFlags1;
    Snap_RecordPictographedActors(play); // writes pictoFlags0/1
    Nei_Save()->pictoboxOwned = 1;
    sPictoCapState = PICTO_CAP_EMIT;
    // MM camera-shutter sfx (NA_SE_SY_CAMERA_SHUTTER = 0x4850 in mm_sources/audio/sfx/systembank_table.h)
    // — an MM-only sfx, played through the MM audio engine (not in the OOT sfx banks). No-ops if mm.o2r
    // isn't loaded.
    if (MmSfx_IsAvailable()) {
        MmSfx_PlayAtPos(0x4850, &GET_PLAYER(play)->actor.world.pos);
    }
}

// Fire the shutter from outside the viewfinder (menu/debug). The capture state machine raises the
// photo display once the readback completes.
void Picto_TakePhoto(PlayState* play) {
    Picto_Shutter(play);
}

// DRAW hook: emit the framebuffer readback into the GBI list when a capture is queued (native-endian
// for the I8 convert, matching 2Ship's picto capture).
void Picto_EmitCapture(PlayState* play, Gfx** gfxp) {
    if (sPictoCapState == PICTO_CAP_EMIT) {
        FB_WriteFramebufferSliceToCPU(gfxp, sPictoFrameRgba16, 0);
        sPictoCapState = PICTO_CAP_PROCESS;
    }
}

// Build the contiguous RGBA16 color copy of the captured region (the on-screen photo). The readback is
// native-endian; F3D wants RGBA16 textures big-endian, so byte-swap each pixel.
static void Picto_BuildColor(void) {
    s32 y;
    s32 x;
    for (y = 0; y < PICTO_PHOTO_HEIGHT; y++) {
        for (x = 0; x < PICTO_PHOTO_WIDTH; x++) {
            u16 px = sPictoFrameRgba16[(PICTO_PHOTO_TOPLEFT_Y + y) * SCREEN_WIDTH + (PICTO_PHOTO_TOPLEFT_X + x)];
            sPictoColorTex[y * PICTO_PHOTO_WIDTH + x] = (u16)((px >> 8) | (px << 8));
        }
    }
}

static void Picto_UpdateState(PlayState* play); // defined below

// UPDATE hook, called from z_play.c (NOT from the player actor — the shutter halts every actor, so a
// player-driven tick would stop with the world and nothing could answer the prompt). Drives the input
// state machine; one frame after the shutter emit, converts the readback to I8 (grayscale, for the
// save) + color (display) and raises the photo + prompt. The compressed I5 is committed only when the
// player KEEPS the photo (Picto_UpdateState), like MM.
void Picto_Update(PlayState* play) {
    // Runs every gameplay frame now, including frames where the player actor isn't loaded yet.
    if ((play == NULL) || (GET_PLAYER(play) == NULL)) {
        return;
    }

    Picto_UpdateState(play);

    if (sPictoCapState == PICTO_CAP_PROCESS) {
        Picto_ConvertRgba16ToIntensityImage(sPictoI8, sPictoFrameRgba16, SCREEN_WIDTH, PICTO_PHOTO_TOPLEFT_X,
                                            PICTO_PHOTO_TOPLEFT_Y, (PICTO_PHOTO_TOPLEFT_X + PICTO_PHOTO_WIDTH) - 1,
                                            (PICTO_PHOTO_TOPLEFT_Y + PICTO_PHOTO_HEIGHT) - 1, 8);
        Picto_BuildColor();
        sPictoCapState = PICTO_CAP_IDLE;
        sPictoColorValid = 1;      // fresh capture: the RGBA copy matches this photo
        sPictoPhotoBeingTaken = 1; // MM: keeping this one writes the save + records the subjects
        // Capture done: leave first-person so the photo + textbox show as an overlay over the normal
        // view (MM-style — MM never holds the lens view during PICTO_BOX_STATE_PHOTO), and the player
        // returns to a clean state so the next photo can be taken.
        Player* player = GET_PLAYER(play);
        FirstPerson_Exit(player, play);
        sPictoAimActive = 0;
        sPictoPhotoActive = 1;
        // The world is already frozen by haltAllActors (set at the shutter) — Link included, since the
        // player is just another actor. MM also blanks the interface for the photo, so only the print
        // and the prompt are on screen.
        Interface_ChangeHudVisibilityMode(1);                // MM: HUD_VISIBILITY_NONE
        Message_StartTextbox(play, PICTO_KEEP_TEXTID, NULL); // MM 0xF8 "Keep this picture?" (keep/discard)
    }
}

// DRAW hook (OVERLAY): show the captured photo for a few seconds, scaled 2x and centered, with a
// 1px black frame. No-op when no preview is armed.
void Picto_DrawPhoto(PlayState* play, Gfx** gfxp) {
    Gfx* g = *gfxp;

    // Aim mode: MM viewfinder. Authentic textures from mm.o2r (parameter_static) when available —
    // 4 mirrored corner borders + center crosshair + "PICTBOX" label — exactly like MM's
    // Interface_Draw PICTO_BOX_STATE_LENS block. Falls back to code-drawn brackets if absent.
    if (sPictoAimActive && !sPictoPhotoActive) {
        // MM's exact viewfinder layout, from the R_PICTO_FOCUS_* register defaults
        // (mm/src/code/title_setup.c:37-48): the four 16x16 corner borders are NOT the photo rect —
        // they sit at (80,60)/(220,60)/(80,160)/(220,160) — the crosshair is at (142,108) and the
        // "PICTBOX" label sits at the BOTTOM RIGHT (204,177), not centered under the frame.
        s32 lx = 80;                  // R_PICTO_FOCUS_BORDER_TOPLEFT_X / BOTTOMLEFT_X
        s32 ty = 60;                  // R_PICTO_FOCUS_BORDER_TOPLEFT_Y / TOPRIGHT_Y
        s32 rx = 220;                 // R_PICTO_FOCUS_BORDER_TOPRIGHT_X / BOTTOMRIGHT_X
        s32 by = 160;                 // R_PICTO_FOCUS_BORDER_BOTTOMLEFT_Y / BOTTOMRIGHT_Y
        s32 iconX = 142, iconY = 108; // R_PICTO_FOCUS_ICON_X / _Y
        s32 textX = 204, textY = 177; // R_PICTO_FOCUS_TEXT_X / _Y

        Picto_LoadViewfinderTextures();

        if (sVfBorder != NULL && sVfIcon != NULL && sVfText != NULL) {
            gDPPipeSync(g++);
            gDPSetCycleType(g++, G_CYC_1CYCLE);
            gDPSetAlphaCompare(g++, G_AC_THRESHOLD);
            gDPSetRenderMode(g++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(g++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetTextureFilter(g++, G_TF_BILERP);
            gDPSetPrimColor(g++, 0, 0, 255, 255, 155, 255);

            // 4 corner borders (IA4 16x16, mirrored: s/t = 512 = one 16px mirror period).
            // Pass the OTR PATH (not the resolved sVfBorder pointer) so an MM HD texture pack applies
            // — sVf* stay only as the mm.o2r availability gate above.
            gDPLoadTextureBlock_4b(g++, "__OTR__parameter_static/gPictoBoxFocusBorderTex", G_IM_FMT_IA, 16, 16, 0,
                                   G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
            // Each corner is drawn 16x16 from its own register position (MM draws all four the same
            // way; the mirror s/t offsets flip the same texture into each corner).
            gSPTextureRectangle(g++, lx << 2, ty << 2, (lx + 16) << 2, (ty + 16) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                                1 << 10);
            gSPTextureRectangle(g++, rx << 2, ty << 2, (rx + 16) << 2, (ty + 16) << 2, G_TX_RENDERTILE, 512, 0, 1 << 10,
                                1 << 10);
            gSPTextureRectangle(g++, lx << 2, by << 2, (lx + 16) << 2, (by + 16) << 2, G_TX_RENDERTILE, 0, 512, 1 << 10,
                                1 << 10);
            gSPTextureRectangle(g++, rx << 2, by << 2, (rx + 16) << 2, (by + 16) << 2, G_TX_RENDERTILE, 512, 512,
                                1 << 10, 1 << 10);

            // Crosshair (I4 32x16) at R_PICTO_FOCUS_ICON_X/Y
            gDPLoadTextureBlock_4b(g++, "__OTR__parameter_static/gPictoBoxFocusIconTex", G_IM_FMT_I, 32, 16, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(g++, iconX << 2, iconY << 2, (iconX + 32) << 2, (iconY + 16) << 2, G_TX_RENDERTILE, 0,
                                0, 1 << 10, 1 << 10);

            // "PICTBOX" label (I4 32x8) at R_PICTO_FOCUS_TEXT_X/Y — bottom right, like MM
            gDPLoadTextureBlock_4b(g++, "__OTR__parameter_static/gPictoBoxFocusTextTex", G_IM_FMT_I, 32, 8, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(g++, textX << 2, textY << 2, (textX + 32) << 2, (textY + 8) << 2, G_TX_RENDERTILE, 0, 0,
                                1 << 10, 1 << 10);
            gDPPipeSync(g++);
        } else {
            // Fallback (no mm.o2r): code-drawn brackets on MM's four corner positions + a crosshair
            // where MM's icon goes. The frame spans lx..rx+16 horizontally and ty..by+16 vertically.
            s32 b = 16;
            s32 fx = rx + 16; // frame right edge
            s32 fy = by + 16; // frame bottom edge
            s32 cx = iconX + 16;
            s32 cy = iconY + 8;
            gDPPipeSync(g++);
            gDPSetCycleType(g++, G_CYC_FILL);
            gDPSetRenderMode(g++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetFillColor(g++, 0xFFFFFFFF);
            gDPFillRectangle(g++, lx, ty, lx + b, ty + 2);
            gDPFillRectangle(g++, lx, ty, lx + 2, ty + b);
            gDPFillRectangle(g++, fx - b, ty, fx, ty + 2);
            gDPFillRectangle(g++, fx - 2, ty, fx, ty + b);
            gDPFillRectangle(g++, lx, fy - 2, lx + b, fy);
            gDPFillRectangle(g++, lx, fy - b, lx + 2, fy);
            gDPFillRectangle(g++, fx - b, fy - 2, fx, fy);
            gDPFillRectangle(g++, fx - 2, fy - b, fx, fy);
            gDPFillRectangle(g++, cx - 5, cy, cx + 6, cy + 1);
            gDPFillRectangle(g++, cx, cy - 5, cx + 1, cy + 6);
            gDPPipeSync(g++);
        }
    }

    // Captured photo display — MM PICTO_BOX_STATE_PHOTO (z_parameter.c:9917-9971), ported 1:1: a gray
    // border panel, then the photo at its native 160x112 size offset UP 33px to leave room for the
    // prompt box along the bottom. NOT a full-screen modal. Photo drawn in 8-row strips.
    // A = keep, B = discard (handled in Picto_UpdateState).
    //
    // Sepia is the DEFAULT, exactly like MM: the I8 image through G_CC_MODULATEI_PRIM with prim
    // (250,160,160,255). The RGBA16 color image is the ColorPictograph enhancement — same CVar name and
    // same default (OFF) as 2Ship's 2s2h/Enhancements/Items/ColorPictograph.cpp. Skijer's NEI
    if (sPictoPhotoActive) {
        s32 top;
        s32 left;
        s32 sy;
        // Only a freshly captured photo has an RGBA copy; the save stores I5, so the picture you get
        // back from it is MM's sepia no matter how the enhancement is set.
        s32 colorPicto = sPictoColorValid && CVarGetInteger("gEnhancements.Items.ColorPictograph", 0);

        // Gray border/background panel (MM: prim 200,200,200,250 XLU, gDPFillRectangle(70,22,251,151)).
        gDPPipeSync(g++);
        gDPSetCycleType(g++, G_CYC_1CYCLE);
        gDPSetRenderMode(g++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(g++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(g++, 0, 0, 200, 200, 200, 250);
        gDPFillRectangle(g++, 70, 22, 251, 151);

        // The photo at native size, offset up 33px (MM "to give room for the message box at the bottom").
        gDPPipeSync(g++);
        gDPSetRenderMode(g++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gDPSetTextureFilter(g++, G_TF_POINT);
        if (colorPicto) {
            gDPSetCombineMode(g++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        } else {
            gDPSetCombineMode(g++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
            gDPSetPrimColor(g++, 0, 0, 250, 160, 160, 255); // MM's sepia tint
        }
        top = PICTO_PHOTO_TOPLEFT_Y - 33; // 31
        for (sy = 0; sy < PICTO_PHOTO_HEIGHT; sy += 8, top += 8) {
            left = PICTO_PHOTO_TOPLEFT_X; // 80
            // Both buffers live at a fixed address, so Fast3D's texture cache would keep showing the
            // PREVIOUS photo even after a new capture overwrites them. Invalidate each strip so the
            // new image is re-uploaded (same fix 2Ship applies in z_parameter.c/ColorPictograph).
            if (colorPicto) {
                gSPInvalidateTexCache(g++, &sPictoColorTex[sy * PICTO_PHOTO_WIDTH]);
                gDPLoadTextureBlock(g++, &sPictoColorTex[sy * PICTO_PHOTO_WIDTH], G_IM_FMT_RGBA, G_IM_SIZ_16b,
                                    PICTO_PHOTO_WIDTH, 8, 0, G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
            } else {
                gSPInvalidateTexCache(g++, &sPictoI8[sy * PICTO_PHOTO_WIDTH]);
                gDPLoadTextureBlock(g++, &sPictoI8[sy * PICTO_PHOTO_WIDTH], G_IM_FMT_I, G_IM_SIZ_8b, PICTO_PHOTO_WIDTH,
                                    8, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                    G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            }
            gSPTextureRectangle(g++, left << 2, top << 2, (left + PICTO_PHOTO_WIDTH) << 2, (top + 8) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }

        // The "Keep this picture?" textbox itself is drawn by the message system (Message_StartTextbox
        // PICTO_KEEP_TEXTID) at the bottom — the photo is offset up 33px to leave room for it, MM-style.
        gDPPipeSync(g++);
    }

    *gfxp = g;
}

// --- First-person viewfinder + photo state machine (MM PICTO_BOX_STATE) ---
u8 Picto_IsAiming(void) {
    return sPictoAimActive;
}

// Show the photo that is already in the save (MM z_parameter.c:4002-4008, the QUEST_PICTOGRAPH branch
// of the pictograph button): decompress the stored I5 back to I8, freeze the world and raise the same
// keep/discard prompt. Answering "No" throws the picture away and drops you into the lens to shoot a
// new one — that is how MM lets you retake a photo, and why the box never asks "replace it?".
static void Picto_ShowStoredPhoto(PlayState* play) {
    Picto_DecompressI5ToI8(Nei_Save()->pictoPhotoI5, sPictoI8, PICTO_PHOTO_SIZE);
    // The stored print gets its colour back from its own file (shared with MM in a combo run). With
    // no colour file it stays MM's sepia, which is the right fallback and never a black frame.
    sPictoColorValid = Picto_SyncReadColor(sPictoColorTex, (int)sizeof(sPictoColorTex)) ? 1 : 0;
    sPictoPhotoBeingTaken = 0;  // keeping it must NOT recompress or re-record the subjects
    play->haltAllActors = true; // MM halts here too
    sPictoAimActive = 0;
    sPictoPhotoActive = 1;
    Interface_ChangeHudVisibilityMode(1); // MM: HUD_VISIBILITY_NONE
    Message_StartTextbox(play, PICTO_KEEP_TEXTID, NULL);
}

// Press the pictograph button (MM z_parameter.c:3996-4008). With no picture stored you go into the
// lens (PICTO_BOX_STATE_LENS); with one stored you are shown THAT picture first. Called from
// Player_UseItem in z_player.c when the Lens slot is in pictobox mode. Guards against unsafe player
// states. Skijer's NEI
void Picto_EnterAimMode(PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (sPictoAimActive || sPictoPhotoActive || !Nei_Save()->pictoboxOwned) {
        return;
    }
    if (player->meleeWeaponState != 0 ||
        (player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_CUTSCENE |
                                PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING))) {
        return;
    }
    if (Nei_Save()->pictoHasPhoto) {
        Picto_ShowStoredPhoto(play);
        return;
    }
    FirstPerson_Init(player, play);
    Picto_HudLensOn(play); // MM: B reads "Stop", the rest of the HUD fades out
    sPictoAimActive = 1;
    sPictoArmWait = 1;    // the button that opened the lens has to come back up first
    sPictoLensFrames = 0; // and first-person needs a frame or two to engage
}

static void Picto_UpdateState(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];

    if (!Nei_Save()->pictoboxOwned) {
        if (sPictoAimActive || sPictoPhotoActive) {
            FirstPerson_Exit(player, play);
            Picto_HudRestore(play);
            play->haltAllActors = false;
        }
        sPictoAimActive = 0;
        sPictoPhotoActive = 0;
        return;
    }

    // PHOTO: the captured photo + the "Keep this picture?" 2-choice textbox (MM PICTO_BOX_STATE_PHOTO,
    // z_parameter.c:3941-3967). The world stays halted from the shutter; the message system drives the
    // choice and we read choiceIndex like MM (0 = Yes/keep, !=0 = No/discard).
    if (sPictoPhotoActive) {
        player->linearVelocity = 0.0f;
        // Only the 2-choice menu ends this state, exactly like MM: A confirms the highlighted option
        // and choiceIndex decides. (There is deliberately no B shortcut — in MM B does nothing here,
        // and a stray B press throwing the picture away is the opposite of how the item feels.)
        if (Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE && Message_ShouldAdvance(play)) {
            Message_CloseTextbox(play);
            play->haltAllActors = false; // MM releases the halt the moment the choice is made
            sPictoPhotoActive = 0;
            if (play->msgCtx.choiceIndex != 0) {
                // "No" -> MM clears QUEST_PICTOGRAPH and goes back to PICTO_BOX_STATE_LENS: the
                // picture is gone (the one you just shot AND the one that was stored) and you are
                // left looking through the lens, ready to take another. A fresh shot also rolls the
                // subject flags back to what they were before the shutter.
                if (sPictoPhotoBeingTaken) {
                    Nei_Save()->pictoFlags0 = sPictoPrevFlags0;
                    Nei_Save()->pictoFlags1 = sPictoPrevFlags1;
                }
                Picto_SyncClear(); // MM: REMOVE_QUEST_ITEM(QUEST_PICTOGRAPH) — clears pictoHasPhoto
                                   // and deletes the cross-game sidecars, or the next load brings
                                   // the picture back and you land on this prompt again
                FirstPerson_Init(player, play);
                Picto_HudLensOn(play);
                sPictoAimActive = 1;
                sPictoArmWait = 1; // the A that answered the prompt must not roll into a shutter
                sPictoLensFrames = 0;
                Audio_PlaySoundGeneral(NA_SE_SY_CANCEL, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            } else {
                // "Yes" -> MM sets QUEST_PICTOGRAPH and returns to PICTO_BOX_STATE_OFF, and ONLY
                // compresses + records the subjects when the photo was actually just taken
                // (sPictoPhotoBeingTaken). Saying yes to the stored photo just puts it away.
                if (sPictoPhotoBeingTaken) {
                    Picto_CompressI8ToI5(sPictoI8, Nei_Save()->pictoPhotoI5, PICTO_PHOTO_SIZE);
                    Picto_SyncWrite(); // the greyscale print — MM's own format, what MM reads
                    // ...and the colour print alongside it, or the picture reaches the other game
                    // (and survives a reload here) in sepia no matter what was on screen.
                    Picto_SyncWriteColor(sPictoColorTex, (int)sizeof(sPictoColorTex));
                }
                Nei_Save()->pictoHasPhoto = 1; // MM: SET_QUEST_ITEM(QUEST_PICTOGRAPH)
                Picto_HudRestore(play);
                Audio_PlaySoundGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
            sPictoPhotoBeingTaken = 0;
        }
        return;
    }

    // OFF: the pictobox is entered ONLY via its equipped C-button (the Lens slot in pictobox mode,
    // handled in Player_UseItem). No D-pad fallback — it spuriously opened the viewfinder when a D-pad
    // button was used for another item (e.g. the Iron Knuckle's Axe / hammer on D-Up).
    if (!sPictoAimActive) {
        // This tick runs BEFORE the player actor, so the press that is about to open the lens
        // (Player_UseItem -> Picto_EnterAimMode, later this same frame) is still in the input right
        // now. Pre-arm the release guard with it, so no matter how the entry is reached the shutter
        // stays locked until the button comes back up. Skijer's NEI
        u16 openBtn = ItemInput_GetEquippedButton(ITEM_LENS, play);
        if (CHECK_BTN_ALL(input->press.button, BTN_A) ||
            (openBtn != 0 && CHECK_BTN_ALL(input->press.button, openBtn))) {
            sPictoArmWait = 1;
            sPictoLensFrames = 0;
        }
        return;
    }

    // LENS: aiming. Keep the player put, run the first-person camera.
    player->linearVelocity = 0.0f;
    FirstPerson_Update(player, play);

    // The pictograph OWNS A and B while the lens is up (A = shutter, B = put the box away). Take a
    // copy of the presses for ourselves, then strip both buttons from the input BEFORE the player
    // actor updates this frame: otherwise Link's own A handling runs first, kicks him out of
    // first-person, and the frame we capture is the third-person one. MM gets this for free — its
    // player sits in PLAYER_UNKAA5_2 picto mode, where A does nothing at all. Skijer's NEI
    u16 pictoPress = input->press.button;
    u16 pictoHeld = input->cur.button;
    input->press.button &= ~(BTN_A | BTN_B);
    input->cur.button &= ~(BTN_A | BTN_B);

    // MM re-asserts the lens HUD every frame of PICTO_BOX_STATE_LENS, so anything else that touches
    // the interface can't leave the pictograph with a half-restored HUD. The call no-ops when the mode
    // is already set.
    Interface_ChangeHudVisibilityMode(10);

    if (sPictoLensFrames < 255) {
        sPictoLensFrames++;
    }

    {
        u16 shutterBtn = ItemInput_GetEquippedButton(ITEM_LENS, play);
        u16 shutterMask = (u16)(BTN_A | shutterBtn);

        // ARM: the lens has just come up. Nothing fires until every shutter button is physically back
        // up — the press that opened the box (or the A that answered the prompt) can be held across
        // several frames, and the entry runs from the player update, a step behind this tick, so a
        // "skip one frame" guard let it leak through and shoot instantly. Two settling frames on top,
        // so first-person is fully engaged and the picture is the aimed view.
        if (sPictoArmWait) {
            if (!(pictoHeld & shutterMask) && (sPictoLensFrames >= 2)) {
                sPictoArmWait = 0;
            }
            return;
        }

        if (sPictoCapState != PICTO_CAP_IDLE) {
            return; // mid-capture; ignore input until the readback completes
        }

        // Shutter = A, exactly like MM (z_parameter.c PICTO_BOX_STATE_LENS: BTN_A, or the "cheese"
        // voice command on the N64DD mic — no OoT equivalent for that one). B closes the lens, which
        // is why B reads "Stop" while aiming. The equipped C-button fires too: it is the button the
        // pictograph is assigned to, and pressing it again is the natural reflex.
        // Capture IMMEDIATELY, in first-person (the aimed view) — never pop a pre-capture textbox,
        // that would drop the player out of first-person and the shot would come out in 3rd person.
        u8 shutter = CHECK_BTN_ALL(pictoPress, BTN_A) || (shutterBtn != 0 && CHECK_BTN_ALL(pictoPress, shutterBtn));
        if (shutter) {
            Picto_Shutter(play);
        } else if (CHECK_BTN_ALL(pictoPress, BTN_B) ||
                   (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED))) {
            FirstPerson_Exit(player, play);
            Picto_HudRestore(play);
            sPictoAimActive = 0;
        }
    }
}

// --- "Pictobox mode" on the Lens of Truth slot (toggled by the kaleido wheel) ---
// When active, the Lens slot represents the Pictograph Box; the in-game trigger lets the player aim.
// (sPictoOnLens is declared up top so the aim trigger sees it.)
u8 Picto_IsOnLensActive(void) {
    // Without the real Lens of Truth, the shared slot is ALWAYS the pictobox — you can't be in "Lens
    // mode" for an item you don't own (so the equipped C-button routes to the camera, not the lens).
    // With the Lens owned, the kaleido wheel's sPictoOnLens decides.
    if (Nei_Save()->pictoboxOwned && gSaveContext.inventory.items[SLOT_LENS] != ITEM_LENS) {
        return 1;
    }
    return sPictoOnLens;
}

void Picto_SetOnLensActive(u8 on) {
    sPictoOnLens = on ? 1 : 0;
}

// --- Item ownership + debug shutter (menu/save-editor accessors) ---
u8 Picto_IsOwned(void) {
    return Nei_Save()->pictoboxOwned;
}

// Photo counter shown on the shared Lens slot: 1 once a picture is kept, 0 otherwise. Skijer's NEI
u8 Picto_HasPhoto(void) {
    return Nei_Save()->pictoHasPhoto;
}

void Picto_SetOwned(u8 on) {
    Nei_Save()->pictoboxOwned = on ? 1 : 0;
}

// Throw the stored picture away from outside the prompt (menu). Same thing answering "No" does: with
// no picture stored, the pictograph button goes straight to the lens instead of showing you the old
// photo — which is MM's behaviour, and the quickest way to tell the two apart while testing.
void Picto_ClearPhoto(void) {
    Picto_SyncClear();
}

// Fire the shutter from the menu without the kaleido wheel (for testing capture + validation).
void Picto_TakePhotoNow(void) {
    if (gPlayState != NULL) {
        Picto_TakePhoto(gPlayState);
    }
}
