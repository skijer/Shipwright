// Execute all six production SW97 draws, real GBI and the Water sound lifecycle.
// Engine matrix, allocation, audio and resource services are fixtures; no game runs.
#include "global.h"
#include "align_asset_macro.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expansions/sw97/actors/arrows/z_arrow_dark.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/arrows/z_arrow_ice.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/arrows/z_arrow_wind.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/arrows/z_arrow_soul.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/arrows/z_arrow_fire.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/arrows/z_arrow_light.inc.c"
#undef THIS
#undef FLAGS

#define REQUIRE(c)                                               \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            exit(1);                                             \
        }                                                        \
    } while (0)

static GraphicsContext gfx;
static PlayState play;
static EnArrow arrow;
static ArrowDark shadow;
static ArrowIce water;
static ArrowWind forest;
static ArrowSoul spirit;
static ArrowFire fire;
static ArrowLight light;
static Gfx commands[128], reference[128], scroll[1];
static Mtx matrix;
static s32 scrollArgs[10], referenceScrollArgs[10];
static int alt, assets, queries;
static int elementalImpactSounds;
static int colorChanged[2];
static Color_RGB8 colorValues[2];
typedef struct {
    Actor* actor;
    u16 id;
    int flagged;
} SoundEvent;
static SoundEvent sounds[32];
static size_t soundCount;
SaveContext gSaveContext;

typedef struct {
    Actor* actor;
    size_t size;
    ActorFunc draw;
    const char* material;
    const char* model;
    const char* textures[2];
    const char* colorKeys[2];
    Color_RGB8 defaults[2];
} ArrowCase;

static const ArrowCase cases[] = {
    { &shadow.actor,
      sizeof(shadow),
      ArrowDark_Draw,
      "__OTR__overlays/ovl_Arrow_Fire/sMaterialDL",
      "__OTR__overlays/ovl_Arrow_Fire/sModelDL",
      { "__OTR__custom/medallion_magic/arrows/shadow/s1Tex", "__OTR__custom/medallion_magic/arrows/shadow/s2Tex" },
      { CVAR_COSMETIC("Arrows.MedallionShadowPrimary"), CVAR_COSMETIC("Arrows.MedallionShadowSecondary") },
      { { 0, 0, 0 }, { 0, 0, 0 } } },
    { &water.actor,
      sizeof(water),
      ArrowIce_Draw,
      "__OTR__overlays/ovl_Arrow_Ice/sMaterialDL",
      "__OTR__overlays/ovl_Arrow_Ice/sModelDL",
      { "__OTR__custom/medallion_magic/arrows/water/s1Tex", "__OTR__custom/medallion_magic/arrows/water/s2Tex" },
      { CVAR_COSMETIC("Arrows.MedallionWaterPrimary"), CVAR_COSMETIC("Arrows.MedallionWaterSecondary") },
      { { 170, 255, 255 }, { 0, 0, 255 } } },
    { &forest.actor,
      sizeof(forest),
      ArrowWind_Draw,
      "__OTR__overlays/ovl_Arrow_Ice/sMaterialDL",
      "__OTR__overlays/ovl_Arrow_Ice/sModelDL",
      { "__OTR__custom/medallion_magic/arrows/forest/s1Tex", "__OTR__custom/medallion_magic/arrows/forest/s2Tex" },
      { CVAR_COSMETIC("Arrows.MedallionForestPrimary"), CVAR_COSMETIC("Arrows.MedallionForestSecondary") },
      { { 170, 255, 255 }, { 0, 255, 0 } } },
    { &spirit.actor,
      sizeof(spirit),
      ArrowSoul_Draw,
      "__OTR__overlays/ovl_Arrow_Light/sMaterialDL",
      "__OTR__overlays/ovl_Arrow_Light/sModelDL",
      { "__OTR__custom/medallion_magic/arrows/spirit/s1Tex", "__OTR__custom/medallion_magic/arrows/spirit/s2Tex" },
      { CVAR_COSMETIC("Arrows.MedallionSpiritPrimary"), CVAR_COSMETIC("Arrows.MedallionSpiritSecondary") },
      { { 255, 255, 170 }, { 255, 255, 0 } } },
    { &fire.actor,
      sizeof(fire),
      ArrowFire_Draw,
      "__OTR__overlays/ovl_Arrow_Fire/sMaterialDL",
      "__OTR__overlays/ovl_Arrow_Fire/sModelDL",
      { NULL, NULL },
      { CVAR_COSMETIC("Arrows.MedallionFirePrimary"), CVAR_COSMETIC("Arrows.MedallionFireSecondary") },
      { { 255, 200, 0 }, { 255, 0, 0 } } },
    { &light.actor,
      sizeof(light),
      ArrowLight_Draw,
      "__OTR__overlays/ovl_Arrow_Light/sMaterialDL",
      "__OTR__overlays/ovl_Arrow_Light/sModelDL",
      { NULL, NULL },
      { CVAR_COSMETIC("Arrows.MedallionLightPrimary"), CVAR_COSMETIC("Arrows.MedallionLightSecondary") },
      { { 255, 255, 255 }, { 170, 170, 170 } } },
};
static const ArrowCase* current;

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (!strcmp(name, CVAR_COSMETIC("Arrows.ElementalImpactSounds"))) {
        return elementalImpactSounds < 0 ? fallback : elementalImpactSounds;
    }
    char expected[128];
    for (int i = 0; i < 2; ++i) {
        snprintf(expected, sizeof(expected), "%s.Changed", current->colorKeys[i]);
        if (!strcmp(name, expected)) {
            return colorChanged[i];
        }
    }
    REQUIRE(!"unexpected cosmetic key (native arrows must remain independent)");
    return fallback;
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    char expected[128];
    for (int i = 0; i < 2; ++i) {
        snprintf(expected, sizeof(expected), "%s.Value", current->colorKeys[i]);
        if (!strcmp(name, expected)) {
            return colorValues[i];
        }
    }
    REQUIRE(!"unexpected color value key");
    return fallback;
}
bool ResourceMgr_IsAltAssetsEnabled(void) {
    return alt;
}
uint8_t ResourceMgr_FileAltExists(const char* path) {
    REQUIRE(alt);
    REQUIRE(current->textures[0] != NULL);
    for (int i = 0; i < 2; ++i) {
        if (!strcmp(path, current->textures[i])) {
            REQUIRE(!(queries & (1 << i)));
            queries |= 1 << i;
            return (assets & (1 << i)) != 0;
        }
    }
    REQUIRE(!"unexpected private resource name");
    return 0;
}
void Gfx_SetupDL_25Xlu(GraphicsContext* context) {
    gDPPipeSync(context->polyXlu.p++);
}
Gfx* Gfx_SetupDL_57(Gfx* p) {
    gDPPipeSync(p++);
    return p;
}
Gfx* Gfx_TwoTexScroll(GraphicsContext* context, s32 a, u32 b, u32 c, s32 d, s32 e, s32 f, u32 g, u32 h, s32 i, s32 j) {
    s32 args[] = { a, b, c, d, e, f, g, h, i, j };
    memcpy(scrollArgs, args, sizeof(args));
    return scroll;
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}
void FrameInterpolation_RecordOpenChild(const void* key, int id) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
}
void Matrix_RotateX(f32 angle, u8 mode) {
}
void Matrix_RotateY(f32 angle, u8 mode) {
}
void Matrix_RotateZ(f32 angle, u8 mode) {
}
void Matrix_RotateZYX(s16 x, s16 y, s16 z, u8 mode) {
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    return &matrix;
}
void Actor_PlaySfx_Flagged(Actor* actor, u16 id) {
    REQUIRE(soundCount < ARRAY_COUNT(sounds));
    sounds[soundCount++] = (SoundEvent){ actor, id, 1 };
    actor->flags &= ~(ACTOR_FLAG_SFX_ACTOR_POS_2 | ACTOR_AUDIO_FLAG_SFX_CENTERED_1 | ACTOR_AUDIO_FLAG_SFX_CENTERED_2 |
                      ACTOR_FLAG_SFX_TIMER);
    actor->sfx = id;
}
void Audio_PlayActorSound2(Actor* actor, u16 id) {
    REQUIRE(soundCount < ARRAY_COUNT(sounds));
    sounds[soundCount++] = (SoundEvent){ actor, id, 0 };
}
void Actor_Kill(Actor* actor) {
    actor->update = NULL;
}
void Actor_ProcessInitChain(Actor* actor, InitChainEntry* chain) {
}
void Actor_SetScale(Actor* actor, f32 scale) {
    actor->scale = (Vec3f){ scale, scale, scale };
}
void Sw97_TagBlinded(Actor* actor, s16 timer) {
    REQUIRE(!"no hit actor in the texture lifecycle fixture");
}
void Sw97_StartCuccoMode(void) {
    REQUIRE(!"no hit actor in the texture lifecycle fixture");
}
Actor* Actor_Spawn(ActorContext* context, PlayState* state, s16 id, f32 x, f32 y, f32 z, s16 rx, s16 ry, s16 rz,
                   s16 params) {
    REQUIRE(!"no hit actor in the texture lifecycle fixture");
    return NULL;
}
void Flags_UnsetSwitch(PlayState* state, s32 flag) {
    REQUIRE(!"empty world actor lists in the texture lifecycle fixture");
}
f32 Math_Vec3f_DistXYZ(Vec3f* a, Vec3f* b) {
    return sqrtf(SQ(a->x - b->x) + SQ(a->y - b->y) + SQ(a->z - b->z));
}
void BgIceShelter_MeltInstantly(Actor* actor, PlayState* context) {
    REQUIRE(!"empty background actor list must not melt any actor");
}
static void alive(Actor* actor, PlayState* context) {
}

static void setup(int impact) {
    memset(&gfx, 0, sizeof(gfx));
    memset(&play, 0, sizeof(play));
    memset(&arrow, 0, sizeof(arrow));
    play.state.gfxCtx = &gfx;
    play.state.frames = 42;
    arrow.actor.update = alive;
    arrow.actor.world.pos = (Vec3f){ 10, 20, 30 };
    memset(colorChanged, 0, sizeof(colorChanged));
    soundCount = 0;
    elementalImpactSounds = -1;
#define INIT_ARROW(value)                       \
    do {                                        \
        memset(&(value), 0, sizeof(value));     \
        (value).actor.parent = &arrow.actor;    \
        (value).radius = 10;                    \
        (value).alpha = 130;                    \
        (value).timer = impact ? 24 : 0;        \
        (value).unk_160 = 1.0f;                 \
        (value).unk_164 = impact ? 1.0f : 0.0f; \
    } while (0)
    INIT_ARROW(shadow);
    INIT_ARROW(water);
    INIT_ARROW(forest);
    INIT_ARROW(spirit);
    INIT_ARROW(light);
#undef INIT_ARROW
    memset(&fire, 0, sizeof(fire));
    fire.actor.parent = &arrow.actor;
    fire.radius = 10;
    fire.alpha = 130;
    fire.timer = impact ? 24 : 0;
    fire.unk_158 = 1.0f;
    fire.unk_15C = impact ? 1.0f : 0.0f;
}

static size_t draw(void) {
    union {
        ArrowDark shadow;
        ArrowIce water;
        ArrowWind forest;
        ArrowSoul spirit;
        ArrowFire fire;
        ArrowLight light;
    } before;
    EnArrow parentBefore = arrow;
    size_t soundsBefore = soundCount;
    memcpy(&before, current->actor, current->size);
    memset(commands, 0, sizeof(commands));
    memset(scrollArgs, 0, sizeof(scrollArgs));
    gfx.polyXlu.p = commands;
    queries = 0;
    current->draw(current->actor, &play);
    REQUIRE(gfx.polyXlu.p < commands + ARRAY_COUNT(commands));
    REQUIRE(!memcmp(&before, current->actor, current->size));
    REQUIRE(!memcmp(&parentBefore, &arrow, sizeof(arrow)));
    REQUIRE(soundCount == soundsBefore);
    return (size_t)(gfx.polyXlu.p - commands);
}

static unsigned opcode(const Gfx* command) {
    return (command->words.w0 >> 24) & 0xFF;
}

static size_t displayList(size_t count, const char* name) {
    for (size_t i = 0; i < count; ++i) {
        if (opcode(&commands[i]) == G_DL && commands[i].words.w1 != (uintptr_t)scroll &&
            !strcmp((const char*)commands[i].words.w1, name)) {
            return i;
        }
    }
    REQUIRE(!"native display list missing");
    return count;
}

static void checkReload(size_t first, size_t end) {
    int slot = 0, stage = 0, pipeSync = 0;
    REQUIRE(first < end);
    for (size_t i = first; i < end; ++i) {
        uintptr_t w0 = commands[i].words.w0, w1 = commands[i].words.w1;
        switch (opcode(&commands[i])) {
            case G_RDPPIPESYNC:
                REQUIRE(stage == 0 || stage == 4);
                if (stage == 4) {
                    ++slot;
                    stage = 0;
                }
                pipeSync = 1;
                break;
            case G_SETTIMG:
                REQUIRE(slot < 2 && stage == 0 && pipeSync);
                REQUIRE(((w0 >> 21) & 7) == G_IM_FMT_I);
                REQUIRE(((w0 >> 19) & 3) == G_IM_SIZ_16b);
                REQUIRE((w0 & 0xFFF) == 0); // Load-block image width is one.
                REQUIRE(w1 && !(w1 & 1));
                REQUIRE(!strcmp((const char*)w1, current->textures[slot]));
                stage = 1;
                pipeSync = 0;
                break;
            case G_SETTILE:
                REQUIRE(stage == 1 && ((w1 >> 24) & 7) == 7); // Load tile only.
                REQUIRE(((w0 >> 21) & 7) == G_IM_FMT_I && ((w0 >> 19) & 3) == G_IM_SIZ_16b);
                REQUIRE(((w0 >> 9) & 0x1FF) == 0);
                REQUIRE((w0 & 0x1FF) == (slot == 0 ? 0 : 0x100));
                stage = 2;
                break;
            case G_RDPLOADSYNC:
                REQUIRE(stage == 2);
                stage = 3;
                break;
            case G_LOADBLOCK:
                REQUIRE(stage == 3 && ((w1 >> 24) & 7) == 7);
                REQUIRE((w0 & 0xFFFFFF) == 0);
                REQUIRE(((w1 >> 12) & 0xFFF) == 1023); // 1024 * 16 bits = 32 * 64 I8.
                REQUIRE((w1 & 0xFFF) == 512);          // Native 32-byte-row DXT.
                stage = 4;
                break;
            default:
                // No render tiles, tile sizes, colors, combiner, geometry or
                // display-list replacement may be submitted by the hook.
                REQUIRE(!"non-image command in private texture reload");
        }
    }
    REQUIRE(slot == 2 && stage == 0 && pipeSync);
}

static void waterSoundLifecycle(void) {
    static const s16 projectileTypes[] = { ARROW_SW97_ICE, ARROW_SEED_ICE };
    Actor holder = { 0 };
    current = &cases[1];
    for (size_t weapon = 0; weapon < ARRAY_COUNT(projectileTypes); ++weapon) {
        for (int altEnabled = 0; altEnabled < 2; ++altEnabled) {
            setup(0);
            alt = altEnabled;
            elementalImpactSounds = altEnabled;
            assets = 0;
            arrow.actor.params = projectileTypes[weapon];
            arrow.actor.parent = &holder;
            arrow.timer = 50;
            water.actor.update = ArrowIce_Update;
            water.actionFunc = ArrowIce_Charge;
            water.radius = 0;

            for (int frame = 0; frame < 3; ++frame) {
                ArrowIce_Update(&water.actor, &play);
                REQUIRE(soundCount == (size_t)frame + 1);
                REQUIRE(sounds[frame].actor == &water.actor && sounds[frame].flagged);
                REQUIRE(sounds[frame].id == NA_SE_EV_WATER_WALL - SFX_FLAG);
                REQUIRE(water.actionFunc == ArrowIce_Charge && water.radius == frame + 1);
            }
            // Redrawing, including a paused redraw, cannot replay a sound.
            draw();
            play.pauseCtx.state = 6;
            draw();
            REQUIRE(soundCount == 3);
            play.pauseCtx.state = 0;

            arrow.actor.parent = NULL;
            ArrowIce_Update(&water.actor, &play);
            REQUIRE(soundCount == 4 && sounds[3].id == NA_SE_EV_WATER_WALL - SFX_FLAG);
            REQUIRE(sounds[3].flagged && sounds[3].actor == &water.actor);
            REQUIRE(water.actionFunc == ArrowIce_Fly && water.radius == 10 && water.alpha == 255);
            ArrowIce_Update(&water.actor, &play);
            ArrowIce_Update(&water.actor, &play);
            REQUIRE(soundCount == 4); // No added firing cue or flight loop.

            arrow.hitFlags = 1;
            ArrowIce_Update(&water.actor, &play);
            REQUIRE(soundCount == 5 && sounds[4].actor == &water.actor && !sounds[4].flagged);
            REQUIRE(sounds[4].id == NA_SE_EV_DIVE_INTO_WATER);
            REQUIRE(water.actionFunc == ArrowIce_Hit && water.timer == 32 && water.alpha == 255);
            for (int frame = 0; frame < 32; ++frame) {
                ArrowIce_Update(&water.actor, &play);
            }
            REQUIRE(soundCount == 5 && water.actor.update == NULL && water.timer == 255 && water.alpha == 0);
        }
    }
    for (int phase = 0; phase < 2; ++phase) {
        setup(0);
        water.actor.update = ArrowIce_Update;
        water.actionFunc = phase ? ArrowIce_Fly : ArrowIce_Charge;
        water.actor.parent = NULL;
        ArrowIce_Update(&water.actor, &play);
        REQUIRE(soundCount == 0 && water.actor.update == NULL);
    }
}

static uintptr_t rgba(Color_RGB8 color, u8 alpha) {
    return ((uintptr_t)color.r << 24) | ((uintptr_t)color.g << 16) | ((uintptr_t)color.b << 8) | alpha;
}

static void checkColors(size_t count, u8 alpha) {
    size_t primary = count, secondary = count;
    for (size_t i = 0; i < count; ++i) {
        if (opcode(&commands[i]) == G_SETPRIMCOLOR) {
            primary = i;
        } else if (opcode(&commands[i]) == G_SETENVCOLOR) {
            secondary = i;
        }
    }
    REQUIRE(primary < count && secondary < count);
    REQUIRE((commands[primary].words.w0 & 0xFFFF) == 0x8080);
    REQUIRE(commands[primary].words.w1 == rgba(colorChanged[0] ? colorValues[0] : current->defaults[0], alpha));
    REQUIRE(commands[secondary].words.w1 == rgba(colorChanged[1] ? colorValues[1] : current->defaults[1], 128));
    for (size_t i = 0; i < count; ++i) {
        REQUIRE(commands[i].words.w0 == reference[i].words.w0);
        if (i != primary && i != secondary) {
            REQUIRE(commands[i].words.w1 == reference[i].words.w1);
        }
    }
}

static void liveArrowColors(void) {
    for (size_t variant = 0; variant < ARRAY_COUNT(cases); ++variant) {
        current = &cases[variant];
        for (int impact = 0; impact < 2; ++impact) {
            for (int textureMode = 0; textureMode < 3; ++textureMode) {
                setup(impact);
                alt = textureMode != 0;
                assets = textureMode == 2 ? 3 : 0;
                u8 alpha = impact ? 211 : 37;
                shadow.alpha = water.alpha = forest.alpha = spirit.alpha = fire.alpha = light.alpha = alpha;
                colorValues[0] = (Color_RGB8){ 241, 73, 19 };
                colorValues[1] = (Color_RGB8){ 11, 23, 47 };
                size_t count = draw();
                memcpy(reference, commands, sizeof(reference));
                checkColors(count, alpha);
                // Each override is independent and works with or without Alt
                // textures. Only the main RGB packets may differ, never fades,
                // screen filters, LOD, texture loads, scrolling or geometry.
                for (int changed = 1; changed < 4; ++changed) {
                    colorChanged[0] = changed & 1;
                    colorChanged[1] = changed & 2;
                    REQUIRE(draw() == count);
                    checkColors(count, alpha);
                }
                // Live/rainbow values must be reread while the same actor exists.
                colorValues[0] = (Color_RGB8){ 3, 5, 7 };
                colorValues[1] = (Color_RGB8){ 233, 199, 151 };
                REQUIRE(draw() == count);
                checkColors(count, alpha);
                colorChanged[0] = colorChanged[1] = 0;
                REQUIRE(draw() == count);
                REQUIRE(!memcmp(reference, commands, sizeof(commands))); // Reset ignores stale Value entries.
            }
        }
    }
}

static void medallionImpactSounds(void) {
    static const int options[] = { -1, 1, 0 };
    for (size_t step = 0; step < ARRAY_COUNT(options); ++step) {
        setup(0);
        elementalImpactSounds = options[step];
        arrow.timer = 50;
        ArrowFire_Fly(&fire, &play);
        ArrowLight_Fly(&light, &play);
        REQUIRE(soundCount == 0);
        arrow.hitFlags = 1;
        ArrowFire_Fly(&fire, &play);
        REQUIRE(soundCount == 1 && sounds[0].actor == &fire.actor && !sounds[0].flagged);
        REQUIRE(sounds[0].id == (options[step] > 0 ? NA_SE_EV_FLAME_IGNITION : NA_SE_IT_EXPLOSION_FRAME));
        REQUIRE(fire.actionFunc == ArrowFire_Hit && fire.timer == 32 && fire.alpha == 255);
        ArrowLight_Fly(&light, &play);
        REQUIRE(soundCount == 2 && sounds[1].actor == &light.actor && !sounds[1].flagged);
        REQUIRE(sounds[1].id == (options[step] > 0 ? NA_SE_EN_LIGHT_ARROW_HIT : NA_SE_IT_EXPLOSION_LIGHT));
        REQUIRE(light.actionFunc == ArrowLight_Hit && light.timer == 32 && light.alpha == 255);
        ArrowIce_Fly(&water, &play);
        REQUIRE(soundCount == 3 && sounds[2].id == NA_SE_EV_DIVE_INTO_WATER);
        ArrowFire_Hit(&fire, &play);
        ArrowLight_Hit(&light, &play);
        ArrowIce_Hit(&water, &play);
        REQUIRE(soundCount == 3);
    }
}

static void checkPhaseBindings(u8 alpha) {
    size_t count = draw();
    size_t material = displayList(count, current->material);
    size_t model = displayList(count, current->model);
    REQUIRE(model > material && commands[model - 1].words.w1 == (uintptr_t)scroll);
    int textureCount = 0, listCount = 0;
    uintptr_t primary = 0, secondary = 0;
    for (size_t i = 0; i < count; ++i) {
        switch (opcode(&commands[i])) {
            case G_SETTIMG:
                ++textureCount;
                break;
            case G_DL:
                ++listCount;
                break;
            case G_SETPRIMCOLOR:
                primary = commands[i].words.w1;
                break;
            case G_SETENVCOLOR:
                secondary = commands[i].words.w1;
                break;
        }
    }
    REQUIRE(listCount == 3); // The same material, scroll and model in every phase.
    if (current->textures[0] != NULL && alt && assets == 3) {
        REQUIRE(textureCount == 2 && queries == 3);
        checkReload(material + 1, model - 1);
    } else {
        REQUIRE(textureCount == 0 && model == material + 2);
    }
    REQUIRE(primary == rgba(colorChanged[0] ? colorValues[0] : current->defaults[0], alpha));
    REQUIRE(secondary == rgba(colorChanged[1] ? colorValues[1] : current->defaults[1], 128));
}

static void allMedallionLifecycles(void) {
    static const struct {
        ActorFunc init;
        ActorFunc update;
        u8* alpha;
        u16* timer;
        s16 bowParam;
        s16 seedParam;
        u8 chargeAlpha;
    } lifecycle[] = {
        { ArrowDark_Init, ArrowDark_Update, &shadow.alpha, &shadow.timer, ARROW_SW97_0C, ARROW_SEED_0C, 200 },
        { ArrowIce_Init, ArrowIce_Update, &water.alpha, &water.timer, ARROW_SW97_ICE, ARROW_SEED_ICE, 100 },
        { ArrowWind_Init, ArrowWind_Update, &forest.alpha, &forest.timer, ARROW_SW97_0E, ARROW_SEED_0E, 120 },
        { ArrowSoul_Init, ArrowSoul_Update, &spirit.alpha, &spirit.timer, ARROW_SW97_0D, ARROW_SEED_0D, 130 },
        { ArrowFire_Init, ArrowFire_Update, &fire.alpha, &fire.timer, ARROW_SW97_FIRE, ARROW_SEED_FIRE, 160 },
        { ArrowLight_Init, ArrowLight_Update, &light.alpha, &light.timer, ARROW_SW97_LIGHT, ARROW_SEED_LIGHT, 130 },
    };
    Player holder = { 0 };
    REQUIRE(ARRAY_COUNT(lifecycle) == ARRAY_COUNT(cases));
    for (size_t variant = 0; variant < ARRAY_COUNT(cases); ++variant) {
        current = &cases[variant];
        for (int seed = 0; seed < 2; ++seed) {
            for (int textureMode = 0; textureMode < 3; ++textureMode) {
                setup(0);
                alt = textureMode != 0;
                assets = textureMode == 2 ? 3 : 0;
                arrow.actor.params = seed ? lifecycle[variant].seedParam : lifecycle[variant].bowParam;
                arrow.actor.parent = &holder.actor;
                arrow.timer = 50;
                play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &holder.actor;
                lifecycle[variant].init(current->actor, &play);
                current->actor->update = lifecycle[variant].update;
                colorChanged[0] = colorChanged[1] = 1;
                colorValues[0] = (Color_RGB8){ 241, 73, 19 };
                colorValues[1] = (Color_RGB8){ 11, 23, 47 };

                for (int frame = 0; frame < 10; ++frame) {
                    lifecycle[variant].update(current->actor, &play);
                    REQUIRE(*lifecycle[variant].alpha == lifecycle[variant].chargeAlpha);
                    checkPhaseBindings(lifecycle[variant].chargeAlpha);
                }

                // The release update changes the real action function to Fly.
                // Check its first draw before another simulation update runs.
                arrow.actor.parent = NULL;
                lifecycle[variant].update(current->actor, &play);
                REQUIRE(*lifecycle[variant].alpha == 255 && *lifecycle[variant].timer == 0);
                checkPhaseBindings(255);
                for (int frame = 0; frame < 3; ++frame) {
                    ++play.state.frames;
                    arrow.actor.world.pos.x += 40;
                    colorValues[0].g += 1;
                    colorValues[1].b += 1;
                    lifecycle[variant].update(current->actor, &play);
                    REQUIRE(current->actor->world.pos.x == arrow.actor.world.pos.x);
                    REQUIRE(*lifecycle[variant].alpha == 255 && *lifecycle[variant].timer == 0);
                    checkPhaseBindings(255);
                }
                arrow.timer = 33;
                lifecycle[variant].update(current->actor, &play);
                REQUIRE(*lifecycle[variant].alpha == 230);
                checkPhaseBindings(230);

                arrow.hitFlags = 1;
                lifecycle[variant].update(current->actor, &play);
                REQUIRE(*lifecycle[variant].alpha == 255 && *lifecycle[variant].timer == 32);
                checkPhaseBindings(255);
                for (int frame = 0; frame < 17; ++frame) {
                    lifecycle[variant].update(current->actor, &play);
                }
                REQUIRE(*lifecycle[variant].timer == 15 && *lifecycle[variant].alpha == 245);
                checkPhaseBindings(245);
            }
        }
    }
}

int main(void) {
    allMedallionLifecycles();
    medallionImpactSounds();
    waterSoundLifecycle();
    liveArrowColors();
    for (size_t variant = 0; variant < ARRAY_COUNT(cases); ++variant) {
        current = &cases[variant];
        if (current->textures[0] == NULL) {
            continue;
        }
        for (int impact = 0; impact < 2; ++impact) {
            setup(impact);
            alt = 0;
            assets = 3;
            size_t nativeCount = draw();
            REQUIRE(queries == 0);
            size_t material = displayList(nativeCount, current->material);
            size_t model = displayList(nativeCount, current->model);
            REQUIRE(model == material + 2);
            REQUIRE(commands[material + 1].words.w1 == (uintptr_t)scroll);
            memcpy(reference, commands, sizeof(reference));
            memcpy(referenceScrollArgs, scrollArgs, sizeof(scrollArgs));

            // Repeat the toggle cycle without rebuilding the actor. Cached
            // eligibility, either partial pair, or only one image must fail.
            for (int cycle = 0; cycle < 2; ++cycle) {
                alt = 1;
                assets = 3;
                size_t count = draw();
                REQUIRE(count > nativeCount);
                REQUIRE(queries == 3);
                size_t extra = count - nativeCount;
                checkReload(material + 1, material + 1 + extra);
                REQUIRE(!memcmp(reference, commands, (material + 1) * sizeof(Gfx)));
                REQUIRE(!memcmp(reference + material + 1, commands + material + 1 + extra,
                                (nativeCount - material - 1) * sizeof(Gfx)));
                REQUIRE(!memcmp(referenceScrollArgs, scrollArgs, sizeof(scrollArgs)));

                for (int pair = 0; pair < 3; ++pair) {
                    assets = pair;
                    REQUIRE(draw() == nativeCount);
                    REQUIRE(queries == 3);
                    REQUIRE(!memcmp(reference, commands, sizeof(commands)));
                }
                alt = 0;
                assets = 3;
                REQUIRE(draw() == nativeCount && queries == 0);
                REQUIRE(!memcmp(reference, commands, sizeof(commands)));
            }
        }
    }
    puts("PASS: all four medallion draws, live Alt toggles, complete/absent/partial pairs, named loads, "
         "32x64 I8 bytes, TMEM slots, load-tile-only commands, all six live/reset independent arrow colors, "
         "all six bow/seed Charge-release-Fly-Hit binding continuity, native material/scroll/model/fade preservation, "
         "Water sound lifecycle, optional Fire/Light impact sounds");
    return 0;
}
