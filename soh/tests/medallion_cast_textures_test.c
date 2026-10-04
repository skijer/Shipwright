// Exercise production SW97 limb callbacks, native curve traversal and dust
// spawn/init/update/draw code. Only external resource, matrix, RNG and arena
// services are fixtures; these checks do not run a game or renderer.
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include "tests/test_require.h"
#include <string.h>

// Normally provided earlier by the SW97 player translation unit.
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);

#include "expansions/sw97/actors/spells/z_magic_fire.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/spells/z_magic_ice.inc.c"
#undef THIS
#undef FLAGS
#include "expansions/sw97/actors/spells/z_magic_wind.inc.c"
#undef THIS
#undef FLAGS
#include "overlays/effects/ovl_Effect_Ss_Dust/z_eff_ss_dust.c"

static GraphicsContext gfx;
static PlayState play;
static Gfx commands[128], reference[128], scrolls[4][1], allocations[4][16];
static Mtx matrix;
Mtx gMtxClear;
static int alt, resources, queries, allocCount, failAllocation, scrollCount;
static s32 scrollArgs[4][10];
static EffectSsDustInitParams spawned[26];
static EffectSs particles[26];
static int spawnCount;
static int changedColors;
static Color_RGB8 customColors[6] = {
    { 11, 22, 33 }, { 44, 55, 66 }, { 77, 88, 99 }, { 111, 122, 133 }, { 144, 155, 166 }, { 177, 188, 199 },
};
static const char* colorKeys[6][2] = {
    { "gCosmetics.Magic.MedallionFirePrimary.Changed", "gCosmetics.Magic.MedallionFirePrimary.Value" },
    { "gCosmetics.Magic.MedallionFireSecondary.Changed", "gCosmetics.Magic.MedallionFireSecondary.Value" },
    { "gCosmetics.Magic.MedallionWaterPrimary.Changed", "gCosmetics.Magic.MedallionWaterPrimary.Value" },
    { "gCosmetics.Magic.MedallionWaterSecondary.Changed", "gCosmetics.Magic.MedallionWaterSecondary.Value" },
    { "gCosmetics.Magic.MedallionForestPrimary.Changed", "gCosmetics.Magic.MedallionForestPrimary.Value" },
    { "gCosmetics.Magic.MedallionForestSecondary.Changed", "gCosmetics.Magic.MedallionForestSecondary.Value" },
};
int32_t CVarGetInteger(const char* name, int32_t fallback) {
    for (int i = 0; i < 6; ++i) {
        if (!strcmp(name, colorKeys[i][0])) {
            return (changedColors & (1 << i)) != 0;
        }
    }
    REQUIRE(!"unexpected cosmetic setting");
    return fallback;
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    for (int i = 0; i < 6; ++i) {
        if (!strcmp(name, colorKeys[i][1])) {
            REQUIRE(changedColors & (1 << i));
            return customColors[i];
        }
    }
    REQUIRE(!"unexpected cosmetic value");
    return fallback;
}
static uintptr_t rgba(Color_RGB8 rgb, u8 alpha) {
    return (uintptr_t)rgb.r << 24 | (uintptr_t)rgb.g << 16 | (uintptr_t)rgb.b << 8 | alpha;
}

static const char* privatePaths[] = {
    "__OTR__custom/medallion_magic/spells/fire/s1Tex",      "__OTR__custom/medallion_magic/spells/fire/s2Tex",
    "__OTR__custom/medallion_magic/spells/water/sTex",      "__OTR__custom/medallion_magic/spells/forest/sTex",
    "__OTR__custom/medallion_magic/spells/forest/dust1Tex", "__OTR__custom/medallion_magic/spells/forest/dust2Tex",
    "__OTR__custom/medallion_magic/spells/forest/dust3Tex", "__OTR__custom/medallion_magic/spells/forest/dust4Tex",
    "__OTR__custom/medallion_magic/spells/forest/dust5Tex", "__OTR__custom/medallion_magic/spells/forest/dust6Tex",
    "__OTR__custom/medallion_magic/spells/forest/dust7Tex", "__OTR__custom/medallion_magic/spells/forest/dust8Tex",
};

bool ResourceMgr_IsAltAssetsEnabled(void) {
    return alt;
}
uint8_t ResourceMgr_FileAltExists(const char* path) {
    REQUIRE(alt);
    for (int i = 0; i < ARRAY_COUNT(privatePaths); ++i) {
        if (!strcmp(path, privatePaths[i])) {
            ++queries;
            return (resources & (1 << i)) != 0;
        }
    }
    REQUIRE(!"unexpected private resource name");
    return 0;
}
void* Graph_Alloc(GraphicsContext* context, size_t size) {
    REQUIRE(context == &gfx && allocCount < 4 && size <= sizeof(allocations[0]));
    return failAllocation ? NULL : allocations[allocCount++];
}
Gfx* Gfx_TwoTexScroll(GraphicsContext* context, s32 a, u32 b, u32 c, s32 d, s32 e, s32 f, u32 g, u32 h, s32 i, s32 j) {
    s32 args[] = { a, b, c, d, e, f, g, h, i, j };
    REQUIRE(context == &gfx && scrollCount < 4);
    memcpy(scrollArgs[scrollCount], args, sizeof(args));
    gSPEndDisplayList(scrolls[scrollCount]);
    return scrolls[scrollCount++];
}
void gSPSegment(void* command, int segment, uintptr_t target) {
    __gSPSegment((Gfx*)command, segment, target);
}
void gSPDisplayList(Gfx* command, Gfx* list) {
    __gSPDisplayList(command, list);
}
Gfx* Gfx_SetupDL(Gfx* command, u32 index) {
    gDPPipeSync(command++);
    return command;
}
void FrameInterpolation_RecordOpenChild(const void* key, int id) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* fmt, ...) {
}
void Matrix_Push(void) {
}
void Matrix_Pop(void) {
}
void Matrix_TranslateRotateZYX(Vec3f* pos, Vec3s* rot) {
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    return &matrix;
}
void SkinMatrix_SetTranslate(MtxF* mf, f32 x, f32 y, f32 z) {
}
void SkinMatrix_SetScale(MtxF* mf, f32 x, f32 y, f32 z) {
}
void SkinMatrix_MtxFMtxFMult(MtxF* a, MtxF* b, MtxF* out) {
}
Mtx* SkinMatrix_MtxFToNewMtx(GraphicsContext* context, MtxF* mf) {
    return &matrix;
}
void Math_Vec3f_Copy(Vec3f* dest, Vec3f* src) {
    *dest = *src;
}
f32 Rand_ZeroFloat(f32 max) {
    return 0;
}
f32 Rand_ZeroOne(void) {
    return 0;
}
void EffectSs_Spawn(PlayState* context, s32 type, s32 priority, void* params) {
    REQUIRE(context == &play && type == EFFECT_SS_DUST && priority == 128 && spawnCount < 26);
    spawned[spawnCount] = *(EffectSsDustInitParams*)params;
    u32 initialized = EffectSsDust_Init(context, spawnCount, &particles[spawnCount], params);
    REQUIRE(initialized == 1);
    ++spawnCount;
}

static unsigned opcode(const Gfx* command) {
    return (command->words.w0 >> 24) & 0xFF;
}
static void resetDraw(void) {
    memset(commands, 0, sizeof(commands));
    memset(allocations, 0, sizeof(allocations));
    memset(scrollArgs, 0, sizeof(scrollArgs));
    gfx.polyXlu.p = commands;
    allocCount = scrollCount = queries = 0;
}

typedef struct {
    SkelCurveLimbList* skeleton;
    OverrideCurveLimbDraw callback;
    int required, first, second, loadCount, dxt, colorBase;
} CastCase;

static CastCase cases[] = {
    { &sLimbList, MagicFire_OverrideLimbDraw, 0x003, 0, 1, 1023, 512, 0 },
    { &sIceLimbList, MagicIce_OverrideLimbDraw, 0x004, 2, 2, 2047, 256, 2 },
    { &sWindLimbList, MagicWind_OverrideLimbDraw, 0x008, 3, 3, 2047, 256, 4 },
};

// Wrong load size/TMEM slot or any extra render-state change must fail here.
static void checkWrapper(Gfx* wrapped, int limb, const CastCase* c, int textures) {
    static const unsigned ops[] = { G_RDPPIPESYNC, G_SETTIMG, G_SETTILE,     G_RDPLOADSYNC, G_LOADBLOCK,  G_RDPPIPESYNC,
                                    G_SETTIMG,     G_SETTILE, G_RDPLOADSYNC, G_LOADBLOCK,   G_RDPPIPESYNC };
    REQUIRE(wrapped == allocations[limb]);
    for (int i = 0; i < (textures ? ARRAY_COUNT(ops) : 1); ++i) {
        REQUIRE(opcode(&wrapped[i]) == ops[i]);
    }
    for (int slot = 0; slot < (textures ? 2 : 0); ++slot) {
        Gfx* image = &wrapped[slot * 5 + 1];
        Gfx* tile = image + 1;
        Gfx* load = image + 3;
        REQUIRE(((image->words.w0 >> 21) & 7) == G_IM_FMT_I);
        REQUIRE(((image->words.w0 >> 19) & 3) == G_IM_SIZ_16b);
        REQUIRE((image->words.w0 & 0xFFF) == 0);
        REQUIRE(image->words.w1 && !(image->words.w1 & 1));
        REQUIRE(!strcmp((char*)image->words.w1, privatePaths[slot ? c->second : c->first]));
        REQUIRE(((tile->words.w0 >> 21) & 7) == G_IM_FMT_I);
        REQUIRE(((tile->words.w0 >> 19) & 3) == G_IM_SIZ_16b);
        REQUIRE(((tile->words.w0 >> 9) & 0x1FF) == 0);
        REQUIRE((tile->words.w0 & 0x1FF) == (slot ? 0x100 : 0));
        REQUIRE(((tile->words.w1 >> 24) & 7) == 7);
        REQUIRE(((load->words.w1 >> 24) & 7) == 7);
        REQUIRE((load->words.w0 & 0xFFFFFF) == 0);
        REQUIRE(((load->words.w1 >> 12) & 0xFFF) == c->loadCount);
        REQUIRE((load->words.w1 & 0xFFF) == c->dxt);
    }
    int index = textures ? 11 : 1;
    if (c->colorBase != 0 && (changedColors & (1 << c->colorBase))) {
        REQUIRE(opcode(&wrapped[index]) == G_SETPRIMCOLOR);
        REQUIRE((wrapped[index].words.w0 & 0xFFFF) == 0x80);
        REQUIRE(wrapped[index++].words.w1 == rgba(customColors[c->colorBase], 255));
    }
    if (c->colorBase != 0 && (changedColors & (1 << (c->colorBase + 1)))) {
        REQUIRE(opcode(&wrapped[index]) == G_SETENVCOLOR);
        REQUIRE(wrapped[index++].words.w1 == rgba(customColors[c->colorBase + 1], 0));
    }
    REQUIRE(opcode(&wrapped[index]) == G_DL && wrapped[index++].words.w1 == (uintptr_t)scrolls[limb]);
    REQUIRE(opcode(&wrapped[index]) == G_ENDDL);
}

static size_t drawCast(const CastCase* c, int enabled) {
    Actor actor = { 0 };
    LimbTransform transforms[5] = { 0 };
    SkelAnimeCurve curve = { .limbCount = c->skeleton->limbCount,
                             .limbList = c->skeleton->limbs,
                             .transforms = transforms };
    const Actor before = actor;
    resetDraw();
    SkelCurve_Draw(&actor, &play, &curve, c->callback, NULL, 1, &actor);
    REQUIRE(!memcmp(&actor, &before, sizeof(actor)));
    const size_t count = gfx.polyXlu.p - commands;
    const int limbCount = c->skeleton->limbCount - 1;
    const int wrapped = enabled || (c->colorBase && (changedColors & (3 << c->colorBase)));
    REQUIRE(count == limbCount * 3 && scrollCount == limbCount);
    REQUIRE(allocCount == (wrapped ? limbCount : 0));
    for (int i = 0; i < limbCount; ++i) {
        Gfx* segment = &commands[i * 3];
        REQUIRE(opcode(segment) == G_MOVEWORD);
        REQUIRE(((segment->words.w0 >> 16) & 0xFF) == G_MW_SEGMENT);
        REQUIRE((segment->words.w0 & 0xFFFF) == (8 + i) * 4);
        if (wrapped) {
            checkWrapper((Gfx*)segment->words.w1, i, c, enabled);
        } else {
            REQUIRE(segment->words.w1 == (uintptr_t)scrolls[i]);
        }
        REQUIRE(opcode(&commands[i * 3 + 1]) == G_MTX);
        REQUIRE(opcode(&commands[i * 3 + 2]) == G_DL);
        Gfx* model = c->skeleton->limbs[i + 1]->dList[1];
        REQUIRE(commands[i * 3 + 2].words.w1 == (uintptr_t)model);
        int loads = 0, calls = 0;
        // Native image loads must precede our dynamic segment; vertices follow.
        for (int j = 0; opcode(&model[j]) != G_VTX; ++j) {
            REQUIRE(j < 40);
            if (opcode(&model[j]) == G_LOADBLOCK) {
                REQUIRE(((model[j].words.w1 >> 12) & 0xFFF) == c->loadCount);
                REQUIRE((model[j].words.w1 & 0xFFF) == c->dxt);
                ++loads;
            }
            if (opcode(&model[j]) == G_SETPRIMCOLOR) {
                REQUIRE(c->colorBase != 0);
                REQUIRE((model[j].words.w0 & 0xFFFF) == 0x80);
                REQUIRE(model[j].words.w1 == (c->colorBase == 2 ? 0x96FFFFFFu : 0xFFFFAAFFu));
            }
            if (opcode(&model[j]) == G_SETENVCOLOR) {
                REQUIRE(c->colorBase != 0);
                const u32 expected =
                    c->colorBase == 2 ? (i == 1 ? 0x0096FF00u : 0x0064FF00u) : (i == 0 ? 0x96FF0000u : 0x00960000u);
                REQUIRE(model[j].words.w1 == expected);
            }
            if (opcode(&model[j]) == G_DL) {
                REQUIRE(loads == 2 && model[j].words.w1 == ((uintptr_t)(8 + i) << 24 | 1));
                ++calls;
            }
        }
        REQUIRE(loads == 2 && calls == 1);
        static const s32 standard[] = { 0, 42, 0, 32, 64, 1, 0, 138, 32, 64 };
        static const s32 forest[2][10] = { { 0, 122, 137, 64, 64, 1, 118, 19, 64, 64 },
                                           { 0, 126, 45, 64, 64, 1, 252, 91, 64, 64 } };
        REQUIRE(!memcmp(scrollArgs[i], c->first == 3 ? forest[i] : standard, sizeof(standard)));
        // Normalize only the allowed segment target before byte comparison.
        segment->words.w1 = (uintptr_t)scrolls[i];
    }
    return count;
}

static void testCastFallbacksAndReloads(void) {
    for (int c = 0; c < ARRAY_COUNT(cases); ++c) {
        alt = 0;
        resources = 0xFFF;
        const size_t count = drawCast(&cases[c], 0);
        memcpy(reference, commands, count * sizeof(Gfx));
        REQUIRE(queries == 0);
        // Includes full -> partial -> full -> off -> on on the same actor path.
        const int masks[] = { 0, 1, 2, 3, 4, 8, 0xFFF, 0, 0xFFF, 0xFFF, 0xFFF };
        for (int i = 0; i < ARRAY_COUNT(masks); ++i) {
            alt = i != 9;
            resources = masks[i];
            const int enabled = alt && (resources & cases[c].required) == cases[c].required;
            size_t current = drawCast(&cases[c], enabled);
            REQUIRE(current == count && !memcmp(reference, commands, count * sizeof(Gfx)));
        }
        failAllocation = 1;
        size_t current = drawCast(&cases[c], 0);
        REQUIRE(current == count && !memcmp(reference, commands, count * sizeof(Gfx)));
        failAllocation = 0;
    }
}

// Color-only wrappers must apply after the native limb material, including
// native Water G=150 limb 2 and Forest's distinct inner/outer greens on reset.
static void testCastColors(void) {
    for (int c = 1; c < 3; ++c) {
        changedColors = 0;
        alt = 0;
        resources = 0;
        size_t count = drawCast(&cases[c], 0);
        memcpy(reference, commands, count * sizeof(Gfx));
        for (int mask = 0; mask < 4; ++mask) {
            changedColors = mask << cases[c].colorBase;
            for (int mode = 0; mode < 3; ++mode) {
                alt = mode != 0;
                resources = mode == 2 ? 0xFFF : 0;
                size_t current = drawCast(&cases[c], mode == 2);
                REQUIRE(current == count && !memcmp(reference, commands, count * sizeof(Gfx)));
                customColors[cases[c].colorBase].r++;
                customColors[cases[c].colorBase + 1].g++;
            }
        }
        // Reset removes wrappers immediately, restoring each native material.
        changedColors = 0;
        alt = 0;
        size_t current = drawCast(&cases[c], 0);
        REQUIRE(current == count && !memcmp(reference, commands, count * sizeof(Gfx)));
    }
}

static void testFireColors(void) {
    MagicFire fire = { 0 };
    // Real Draw emits colors even without curve transforms. Test fading alpha.
    for (int alphaCase = 0; alphaCase < 3; ++alphaCase) {
        fire.alphaMultiplier = alphaCase * 0.5f;
        for (int mask = 0; mask < 4; ++mask) {
            changedColors = mask;
            alt = mask & 1;
            resources = 0;
            resetDraw();
            MagicFire before = fire;
            MagicFire_Draw(&fire.actor, &play);
            REQUIRE(!memcmp(&fire, &before, sizeof(fire)));
            REQUIRE(gfx.polyXlu.p - commands == 3);
            REQUIRE(opcode(&commands[1]) == G_SETPRIMCOLOR && opcode(&commands[2]) == G_SETENVCOLOR);
            REQUIRE((commands[1].words.w0 & 0xFFFF) == 0x80);
            REQUIRE(commands[1].words.w1 ==
                    rgba(mask & 1 ? customColors[0] : (Color_RGB8){ 255, 200, 0 }, (u8)(fire.alphaMultiplier * 255)));
            REQUIRE(commands[2].words.w1 ==
                    rgba(mask & 2 ? customColors[1] : (Color_RGB8){ 255, 0, 0 }, (u8)(fire.alphaMultiplier * 255)));
            ++customColors[0].b;
            ++customColors[1].r;
        }
    }
    changedColors = 0;
}

static uintptr_t drawDust(EffectSs* effect, size_t* count) {
    const EffectSs before = *effect;
    resetDraw();
    EffectSsDust_Draw(&play, 0, effect);
    REQUIRE(!memcmp(effect, &before, sizeof(before)));
    *count = gfx.polyXlu.p - commands;
    for (size_t i = 0; i < *count; ++i) {
        if (opcode(&commands[i]) == G_MOVEWORD) {
            REQUIRE((commands[i].words.w0 & 0xFFFF) == 8 * 4);
            uintptr_t texture = commands[i].words.w1;
            commands[i].words.w1 = 0;
            return texture;
        }
    }
    REQUIRE(!"dust texture segment missing");
    return 0;
}

static void testForestDustIsolation(void) {
    const char* native[] = { gDust1Tex, gDust2Tex, gDust3Tex, gDust4Tex, gDust5Tex, gDust6Tex, gDust7Tex, gDust8Tex };
    // Preserve each existing low-bit draw mode, even when the new bit is set.
    for (int flags = 0; flags < 8; ++flags) {
        EffectSsDustInitParams init = { .pos = { 1, 2, 3 },
                                        .primColor = { 220, 250, 255, 220 },
                                        .envColor = { 150, 200, 230, 140 },
                                        .scale = 200,
                                        .scaleStep = 30,
                                        .life = 16,
                                        .drawFlags = flags,
                                        .updateMode = 0 };
        EffectSs dust = { 0 };
        EffectSsDust_Init(&play, 0, &dust, &init);
        for (int frame = 0; frame < 8; ++frame) {
            dust.rTexIdx = frame;
            dust.rDrawFlags = flags;
            alt = 1;
            resources = 0xFFF;
            size_t count, current;
            uintptr_t chosen = drawDust(&dust, &count);
            REQUIRE(chosen == (uintptr_t)native[frame] && queries == 0);
            memcpy(reference, commands, count * sizeof(Gfx));
            dust.rDrawFlags |= 0x100;
            // Removing any single animation frame falls back as one set.
            for (int missing = -1; missing < 8; ++missing) {
                resources = missing < 0 ? 0xFF0 : (0xFF0 & ~(1 << (missing + 4)));
                chosen = drawDust(&dust, &current);
                REQUIRE(!strcmp((char*)chosen, missing < 0 ? privatePaths[frame + 4] : native[frame]));
                REQUIRE(current == count && !memcmp(reference, commands, count * sizeof(Gfx)));
            }
            alt = 0;
            resources = 0xFFF;
            chosen = drawDust(&dust, &current);
            REQUIRE(chosen == (uintptr_t)native[frame] && queries == 0);
            REQUIRE(current == count && !memcmp(reference, commands, count * sizeof(Gfx)));
            alt = 1;
            chosen = drawDust(&dust, &current);
            REQUIRE(!strcmp((char*)chosen, privatePaths[frame + 4]));
        }
    }
}

static void testForestDustColors(void) {
    EffectSsDustInitParams init = { .primColor = { 220, 250, 255, 220 },
                                    .envColor = { 150, 200, 230, 140 },
                                    .scale = 200,
                                    .life = 16,
                                    .drawFlags = 0x100 };
    EffectSs dust = { 0 };
    EffectSsDust_Init(&play, 0, &dust, &init);
    for (int tagged = 0; tagged < 2; ++tagged) {
        dust.rDrawFlags = tagged ? 0x100 : 0;
        changedColors = 0;
        alt = 0;
        resources = 0;
        size_t count;
        drawDust(&dust, &count);
        memcpy(reference, commands, count * sizeof(Gfx));
        for (int mask = 0; mask < 4; ++mask) {
            changedColors = mask << 4;
            for (int mode = 0; mode < 3; ++mode) {
                alt = mode != 0;
                resources = mode == 2 ? 0xFFF : 0;
                size_t current;
                drawDust(&dust, &current);
                REQUIRE(current == count);
                int colors = 0;
                for (size_t i = 0; i < count; ++i) {
                    if (opcode(&commands[i]) == G_SETPRIMCOLOR) {
                        REQUIRE(commands[i].words.w1 ==
                                rgba(tagged && (mask & 1) ? customColors[4] : (Color_RGB8){ 220, 250, 255 }, 255));
                        commands[i].words.w1 = reference[i].words.w1;
                        ++colors;
                    } else if (opcode(&commands[i]) == G_SETENVCOLOR) {
                        REQUIRE(commands[i].words.w1 ==
                                rgba(tagged && (mask & 2) ? customColors[5] : (Color_RGB8){ 150, 200, 230 }, 140));
                        commands[i].words.w1 = reference[i].words.w1;
                        ++colors;
                    }
                }
                REQUIRE(colors == 2 && !memcmp(reference, commands, count * sizeof(Gfx)));
                ++customColors[4].g;
                ++customColors[5].b;
            }
        }
    }
    changedColors = 0;
}

static void testTornadoSpawnAndLifetime(void) {
    Vec3f center = { 10, 20, 30 };
    spawnCount = 0;
    alt = 0;
    MagicWind_SpawnTornadoVFX(&play, &center, 200);
    REQUIRE(spawnCount == 26);
    for (int i = 0; i < 26; ++i) {
        const int ring = i < 12, streak = i >= 20;
        const Vec3f pos = { ring ? 160 : streak ? 110 : 10, 20, 30 };
        const Vec3f velocity = { streak ? 1.5f : 0, ring ? 5 : streak ? 22 : 14, ring ? 12 : streak ? 0 : 6 };
        const Vec3f accel = { 0, streak ? 0 : 0.3f, 0 };
        const Color_RGBA8 prim = streak ? (Color_RGBA8){ 255, 255, 255, 240 } : (Color_RGBA8){ 220, 250, 255, 220 };
        const Color_RGBA8 env = streak ? (Color_RGBA8){ 180, 220, 240, 160 } : (Color_RGBA8){ 150, 200, 230, 140 };
        REQUIRE(spawned[i].pos.x == pos.x && spawned[i].pos.y == pos.y && spawned[i].pos.z == pos.z);
        REQUIRE(spawned[i].velocity.x == velocity.x && spawned[i].velocity.y == velocity.y &&
                spawned[i].velocity.z == velocity.z);
        REQUIRE(!memcmp(&spawned[i].accel, &accel, sizeof(accel)));
        REQUIRE(!memcmp(&spawned[i].primColor, &prim, sizeof(prim)));
        REQUIRE(!memcmp(&spawned[i].envColor, &env, sizeof(env)));
        REQUIRE(spawned[i].scale == (streak ? 220 : 200));
        REQUIRE(spawned[i].scaleStep == (streak ? 60 : 30));
        REQUIRE(spawned[i].life == (ring ? 16 : streak ? 22 : 18));
        REQUIRE(spawned[i].drawFlags == 0x100 && spawned[i].updateMode == 0);
        EffectSsDustInitParams controlInit = spawned[i];
        EffectSs control = { 0 };
        controlInit.drawFlags = 0;
        EffectSsDust_Init(&play, i, &control, &controlInit);
        // The new bit is the only permitted difference after init and updates.
        while (control.life > 0) {
            particles[i].update(&play, i, &particles[i]);
            control.update(&play, i, &control);
            EffectSs normalized = particles[i];
            normalized.rDrawFlags &= ~0x100;
            REQUIRE(!memcmp(&normalized, &control, sizeof(control)));
            alt = control.life & 1;
            resources = 0xFF0;
            size_t count;
            uintptr_t chosen = drawDust(&particles[i], &count);
            REQUIRE(!strcmp((char*)chosen, alt ? privatePaths[control.rTexIdx + 4]
                                               : (const char*[]){ gDust1Tex, gDust2Tex, gDust3Tex, gDust4Tex, gDust5Tex,
                                                                  gDust6Tex, gDust7Tex, gDust8Tex }[control.rTexIdx]));
            --particles[i].life;
            --control.life;
        }
    }
}

int main(int argc, char** argv) {
    play.state.gfxCtx = &gfx;
    play.state.frames = play.gameplayFrames = 42;
    const struct {
        const char* name;
        void (*run)(void);
    } tests[] = {
        { "textures", testCastFallbacksAndReloads },
        { "colors", testCastColors },
        { "fire", testFireColors },
        { "dust", testForestDustIsolation },
        { "dust-colors", testForestDustColors },
        { "spawn", testTornadoSpawnAndLifetime },
    };
    int ran = 0;
    for (int i = 0; i < ARRAY_COUNT(tests); ++i) {
        if (argc == 1 || !strcmp(argv[1], tests[i].name)) {
            tests[i].run();
            ++ran;
        }
    }
    REQUIRE(ran > 0);
    puts("PASS: SW97 cast texture loads, fallback, scrolling, cosmetics and isolated forest dust");
    return 0;
}
