// Midna in place of Navi. Port of marsh6487's poc/midna-navi-soh-poc2 (f889527c..822f46b1), the visual half.
// The model is not part of this mod: it lives in an asset pack under objects/midna_navi/ (the same paths the
// original reads). Without it every function here backs out and the vanilla Navi draws as usual.
//
//   poc1/MidnaFloatDL                 static model (required)
//   poc2/BodyDL, MarkingsDL,          animated model: 64 pose vertex banks, bound to segment 8
//        ShimmerDL, ShimmerVertices,
//        Pose00..Pose63, DiffuseNeutral, MarkingsMask
//   poc2/BlinkHalfDL, BlinkClosedDL,  optional blink, with DiffuseHalf / DiffuseClosed
//
// Voice (optional, and also supplied by the pack): sample resources custom/samples/midna_<event>, 32 kHz mono, for
// dash, emerge, vanish, target_npc, target_enemy and target_other. They replace the matching Navi calls that go
// through her actor (the "Hey!", "Listen!" and fairy dash sounds). Navi's calls from the HUD and her talk laugh are
// not routed through her actor, so they stay vanilla. Without a clip the vanilla sound plays.
//
// Navi's own actor keeps running; only EnElf_Draw is replaced, and only for params == FAIRY_NAVI, so the other
// fairies that share the gameplay_keep skeleton are never touched.

#include "soh/ModApi/ModApi.h"

#include <stdio.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "sequence.h"

#define MIDNA_CVAR "gMods.MidnaNavi.Enabled"
#define MIDNA_STATIC_DL "objects/midna_navi/poc1/MidnaFloatDL"
#define MIDNA_BASE_SCALE 0.6f
#define MIDNA_MOTES 6

static const char* const sRequiredHooks[] = { "OnActorDraw", "OnActorPlaySfx" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

typedef struct {
    Gfx* body;
    Gfx* markings;
    Gfx* shimmer;
    Vtx* pose;
} MidnaModel;

// Blink: first after 1.2 s, then every ~5 s, six update frames long. Returns the blink display list, or NULL for
// "eyes open". The blink atlases are optional, and a partial pack keeps the open eye.
static Gfx* BlinkModel(u16 timer) {
    s32 phase = timer % 200;
    s32 frame = phase >= 116 ? phase - 116 : phase - 24;

    if (frame < 0 || frame > 5) {
        return NULL;
    }
    if (!ResourceMgr_FileExists("objects/midna_navi/poc2/BlinkHalfDL") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/BlinkClosedDL") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/DiffuseHalf") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/DiffuseClosed")) {
        return NULL;
    }
    return ResourceMgr_LoadGfxByName(frame == 2 || frame == 3 ? "objects/midna_navi/poc2/BlinkClosedDL"
                                                               : "objects/midna_navi/poc2/BlinkHalfDL");
}

static bool LoadAnimated(EnElf* this, MidnaModel* out) {
    char posePath[64];

    snprintf(posePath, sizeof(posePath), "objects/midna_navi/poc2/Pose%02u", (unsigned)(this->timer & 63));
    if (!ResourceMgr_FileExists("objects/midna_navi/poc2/BodyDL") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/MarkingsDL") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/ShimmerDL") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/ShimmerVertices") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/DiffuseNeutral") ||
        !ResourceMgr_FileExists("objects/midna_navi/poc2/MarkingsMask") || !ResourceMgr_FileExists(posePath)) {
        return false;
    }
    out->body = ResourceMgr_LoadGfxByName("objects/midna_navi/poc2/BodyDL");
    out->markings = ResourceMgr_LoadGfxByName("objects/midna_navi/poc2/MarkingsDL");
    out->shimmer = ResourceMgr_LoadGfxByName("objects/midna_navi/poc2/ShimmerDL");
    out->pose = ResourceMgr_LoadVtxByName(posePath);
    return out->body != NULL && out->markings != NULL && out->shimmer != NULL && out->pose != NULL &&
           ResourceMgr_LoadVtxByName("objects/midna_navi/poc2/ShimmerVertices") != NULL;
}

static void DrawShimmer(EnElf* this, PlayState* play, Gfx* shimmer, u8 alpha) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    // Vertex alpha shapes each mote; the fog blender would read it as fog distance.
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)this->outerColor.r, (u8)this->outerColor.g, (u8)this->outerColor.b, 255);
    for (s32 i = 0; i < MIDNA_MOTES; i++) {
        s16 orbit = this->timer * 0x100 + i * 0x2AAA;
        f32 pulse = 0.5f + 0.5f * Math_SinS(this->timer * 0x500 + i * 0x2AAA);

        Matrix_Push();
        Matrix_Translate(Math_CosS(orbit) * 1050.0f, -1650.0f + i * 255.0f + pulse * 70.0f, Math_SinS(orbit) * 650.0f,
                         MTXMODE_APPLY);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, (u8)(alpha * 0.35f * pulse));
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, shimmer);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool DrawMidna(EnElf* this, PlayState* play) {
    MidnaModel animated = { 0 };
    Gfx* model;
    f32 alphaScale;
    f32 scale;
    u8 alpha;

    if (!ResourceMgr_FileExists(MIDNA_STATIC_DL)) {
        return false;
    }
    model = ResourceMgr_LoadGfxByName(MIDNA_STATIC_DL);
    if (model == NULL) {
        return false;
    }
    alphaScale = this->disappearTimer < 0 ? this->disappearTimer * (7.0f / 6000.0f) + 1.0f : 1.0f;
    alpha = (u8)(this->innerColor.a * CLAMP(alphaScale, 0.0f, 1.0f));
    if (alpha == 0) {
        return true;
    }
    if (LoadAnimated(this, &animated)) {
        model = animated.body;
    } else {
        animated.pose = NULL;
    }
    scale = MIDNA_BASE_SCALE * CVarGetFloat("gCosmetics.Fairies.Size", 1.0f);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateZ(Math_SinS(this->timer * 0x300) * 0.035f, MTXMODE_APPLY);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, alpha);
    // A half-faded mesh must not write depth over later translucent draws.
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, alpha == 255 ? G_RM_AA_ZB_OPA_SURF2 : G_RM_AA_ZB_XLU_SURF2);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (animated.pose != NULL) {
        Gfx* blink = BlinkModel(this->timer);

        gSPSegment(POLY_XLU_DISP++, 0x08, animated.pose);
        gSPDisplayList(POLY_XLU_DISP++, blink != NULL ? blink : model);
        // Identical animated vertices and decal depth keep the markings from detaching or z-fighting.
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)this->innerColor.r, (u8)this->innerColor.g, (u8)this->innerColor.b,
                        255);
        gSPDisplayList(POLY_XLU_DISP++, animated.markings);
        DrawShimmer(this, play, animated.shimmer, alpha);
    } else {
        gSPDisplayList(POLY_XLU_DISP++, model);
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

static void ReplaceNavi(Actor* actor, PlayState* play, bool* drawVanilla) {
    EnElf* this = (EnElf*)actor;

    if (actor->params != FAIRY_NAVI || !CVarGetInteger(MIDNA_CVAR, 1)) {
        return;
    }
    if (DrawMidna(this, play)) {
        *drawVanilla = false;
    }
}

// -- voice ----------------------------------------------------------------------------------------------------------

#define VOICE_SLOTS 2
#define VOICE_COOLDOWN_FRAMES (8 * 20) // frames of game logic: 20 Hz

typedef enum {
    VOICE_DASH,
    VOICE_EMERGE,
    VOICE_VANISH,
    VOICE_TARGET_NPC,
    VOICE_TARGET_ENEMY,
    VOICE_TARGET_OTHER,
    VOICE_COUNT,
} VoiceEvent;

static const char* const sVoicePaths[VOICE_COUNT] = {
    "custom/samples/midna_dash",         "custom/samples/midna_emerge",       "custom/samples/midna_vanish",
    "custom/samples/midna_target_npc",   "custom/samples/midna_target_enemy", "custom/samples/midna_target_other",
};

typedef struct {
    const s16* samples;
    s32 count;
    bool tried;
} VoiceClip;

typedef struct {
    s32 clip; // index into sClips, or -1
    s32 position;
} VoiceSlot;

static VoiceClip sClips[VOICE_COUNT];
static VoiceSlot sSlots[VOICE_SLOTS] = { { -1, 0 }, { -1, 0 } };
static volatile s32 sRequested = -1;
static s32 sDashCooldown;

// Audio thread: reads only the request and the slots, never the game state.
static void MixMidna(s16* samples, u32 frameCount) {
    s32 request = sRequested;

    if (request >= 0) {
        sRequested = -1;
        // A new cue takes the oldest slot; two voices at most, so a fresh cue replaces a stale one.
        sSlots[0] = sSlots[1];
        sSlots[1].clip = request;
        sSlots[1].position = 0;
    }
    for (s32 slot = 0; slot < VOICE_SLOTS; slot++) {
        VoiceSlot* voice = &sSlots[slot];
        const VoiceClip* clip = voice->clip >= 0 ? &sClips[voice->clip] : NULL;

        for (u32 frame = 0; clip != NULL && frame < frameCount; frame++) {
            s32 value;

            if (clip->samples == NULL || voice->position >= clip->count) {
                voice->clip = -1;
                break;
            }
            value = clip->samples[voice->position++];
            samples[frame * 2 + 0] = (s16)CLAMP(samples[frame * 2 + 0] + value, -32768, 32767);
            samples[frame * 2 + 1] = (s16)CLAMP(samples[frame * 2 + 1] + value, -32768, 32767);
        }
    }
}

static const VoiceClip* LoadClip(VoiceEvent event) {
    VoiceClip* clip = &sClips[event];

    if (!clip->tried) {
        SoundFontSample* sample;

        clip->tried = true;
        sample = sApi->HasResource(sVoicePaths[event]) ? ResourceMgr_LoadAudioSample(sVoicePaths[event]) : NULL;
        if (sample != NULL) {
            clip->samples = (const s16*)sample->sampleAddr;
            clip->count = (s32)(sample->size / sizeof(s16));
        }
    }
    return clip->samples != NULL ? clip : NULL;
}

static s32 EventFor(uint16_t sfxId) {
    switch (sfxId) {
        case NA_SE_EV_FAIRY_DASH:
            return VOICE_DASH;
        case NA_SE_EV_NAVY_VANISH:
            return VOICE_VANISH;
        case NA_SE_VO_NAVY_HELLO:
            return VOICE_TARGET_NPC;
        case NA_SE_VO_NAVY_ENEMY:
            return VOICE_TARGET_ENEMY;
        case NA_SE_VO_NAVY_HEAR:
            return VOICE_TARGET_OTHER;
        default:
            return -1;
    }
}

static void SpeakAsMidna(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    s32 event;

    if (actor->params != FAIRY_NAVI || !CVarGetInteger(MIDNA_CVAR, 1) || !ResourceMgr_FileExists(MIDNA_STATIC_DL)) {
        return;
    }
    event = EventFor(*sfxId);
    if (event < 0 || LoadClip((VoiceEvent)event) == NULL) {
        return;
    }
    *handled = true;
    // Her darts and calls repeat constantly: movement cues keep a cooldown, speech cues do not.
    if (event == VOICE_DASH || event == VOICE_EMERGE || event == VOICE_VANISH) {
        if (gPlayState != NULL && (s32)gPlayState->gameplayFrames < sDashCooldown) {
            return;
        }
        sDashCooldown = gPlayState != NULL ? (s32)gPlayState->gameplayFrames + VOICE_COOLDOWN_FRAMES : 0;
    }
    sRequested = event;
}

static void RegisterToggle(void) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = "Midna as Navi";
    widget.cvar = MIDNA_CVAR;
    widget.tooltip = "Draw Midna instead of Navi. Needs the Midna asset pack; without it Navi is unchanged.";
    widget.defaultInt = 1;
    sApi->RegisterMenuWidget(&widget);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_ELF, ReplaceNavi);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorPlaySfx, ACTOR_EN_ELF, SpeakAsMidna);
    if (SOH_MOD_API_HAS(sApi, RegisterAudioMixInGroup)) {
        sApi->RegisterAudioMixInGroup(MixMidna, SOH_AUDIO_GROUP_SFX, NULL);
    }
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterToggle();
    }
}
