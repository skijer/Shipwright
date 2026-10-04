// Real production C and GBI, with engine resource/allocation boundary fixtures.
#include "global.h"
#include "din_fire_sword.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GraphicsContext gfx;
static PlayState play;
static Player player;
static Gfx opa[512], xlu[512], core[1], flame[1];
static u8 pixels[4];
static Mtx matrix;
static MtxF currentMatrix, matrixStack[8], submittedMatrices[8];
static int enabled, alt, assets, loadFailure, transformed, invisible, customColors;
static int depth, matrices, fireDamage, otherOwner, pakActive, bossOwner;
static const char* missing;
static const char* lastCorePath;
SaveContext gSaveContext;

#define REQUIRE(c)                                               \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            exit(1);                                             \
        }                                                        \
    } while (0)

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (!strcmp(name, "gEnhancements.DinFireSword"))
        return enabled;
    if (!strcmp(name, "gEnhancements.DinFireSwordDamage"))
        return fireDamage;
    if (!strcmp(name, "gSettings.AltAssets"))
        return alt;
    return fallback;
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    if (!customColors)
        return fallback;
    if (!strcmp(name, "gCosmetics.Custom.DinFireSwordCore.Value"))
        return (Color_RGB8){ 41, 241, 199 };
    if (!strcmp(name, "gCosmetics.Custom.DinFireSwordOuter.Value"))
        return (Color_RGB8){ 12, 33, 177 };
    return fallback;
}
uint8_t ResourceMgr_FileExists(const char* path) {
    return assets && !(missing && strstr(path, missing));
}
Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    if (loadFailure || (missing && strstr(path, missing)))
        return NULL;
    if (strstr(path, "CoreDL")) {
        lastCorePath = path;
        return core;
    }
    return flame;
}
void* ResourceGetDataByName(const char* path) {
    return loadFailure || (missing && strstr(path, missing)) ? NULL : pixels;
}
u8 PakLoader_HasActiveModel(void) {
    return pakActive;
}
u8 ExtEquip_ShouldHideSwordDL(void) {
    return otherOwner;
}
s32 BossRemains_IsOdolwaWorn(void) {
    return bossOwner == 1;
}
s32 BossRemains_IsGohtWorn(void) {
    return bossOwner == 2;
}
u8 WeaponUpgrade_KokiriLevel(void) {
    return otherOwner;
}
u8 WeaponUpgrade_HasGreatFairy(void) {
    return otherOwner;
}
u8 TransformMasks_IsTransformedAny(void) {
    return transformed;
}
uint8_t GameInteractor_InvisibleLinkActive(void) {
    return invisible;
}
void FrameInterpolation_RecordOpenChild(const void* key, int id) {
    (void)key;
    (void)id;
}
void FrameInterpolation_RecordCloseChild(void) {
}
void Gfx_SetupDL_25Xlu(GraphicsContext* context) {
    (void)context;
}
void Gfx_SetupDL_25Opa(GraphicsContext* context) {
    (void)context;
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}
void Matrix_Push(void) {
    REQUIRE(depth < ARRAY_COUNT(matrixStack));
    matrixStack[depth] = currentMatrix;
    ++depth;
}
void Matrix_Pop(void) {
    REQUIRE(depth > 0);
    --depth;
    currentMatrix = matrixStack[depth];
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    (void)context;
    (void)file;
    (void)line;
    REQUIRE(matrices < ARRAY_COUNT(submittedMatrices));
    submittedMatrices[matrices++] = currentMatrix;
    return &matrix;
}

static void setup(void) {
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&gfx, 0, sizeof(gfx));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(opa, 0, sizeof(opa));
    memset(xlu, 0, sizeof(xlu));
    memset(&currentMatrix, 0, sizeof(currentMatrix));
    play.state.gfxCtx = &gfx;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    player.actor.scale.y = .01f;
    player.leftHandType = PLAYER_MODELTYPE_LH_SWORD;
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_MASTER;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    enabled = alt = assets = 1;
    loadFailure = transformed = invisible = customColors = depth = matrices = 0;
    missing = lastCorePath = NULL;
    fireDamage = otherOwner = pakActive = bossOwner = 0;
    DinFireSword_Reset();
}
static void tick(void) {
    ++play.gameplayFrames;
    DinFireSword_Update(&play, &player);
}
static size_t draw(void) {
    gfx.polyOpa.p = opa;
    gfx.polyXlu.p = xlu;
    matrices = 0;
    Player before = player;
    DinFireSword_BeginPlayerDraw(&play, &player);
    DinFireSword_Draw(&play, &player);
    DinFireSword_DrawAfterPlayer(&play, &player);
    REQUIRE(depth == 0 && gfx.polyOpa.p < opa + 512 && gfx.polyXlu.p < xlu + 512);
    REQUIRE(!memcmp(&before, &player, sizeof(player)));
    return (gfx.polyOpa.p - opa) + (gfx.polyXlu.p - xlu);
}
static void requireTexture(Gfx* start, Gfx* end, const char* name) {
    int found = 0;
    for (Gfx* p = start; p < end; ++p) {
        if ((p->words.w0 >> 24) != G_SETTIMG)
            continue;
        uintptr_t address = p->words.w1;
        REQUIRE(address != 0 && !(address & 1));
        if (strcmp((char*)address, name) == 0)
            ++found;
    }
    REQUIRE(found == 1);
}
static int color(Gfx* start, Gfx* end, int opcode, unsigned rgb) {
    for (Gfx* p = start; p < end; ++p)
        if ((p->words.w0 >> 24) == opcode && ((p->words.w1 >> 8) & 0xFFFFFF) == rgb)
            return 1;
    return 0;
}

static int containsLayer(Gfx* start, Gfx* end, Gfx* layer) {
    for (Gfx* p = start; p < end; ++p)
        if ((p->words.w0 >> 24) == G_DL && p->words.w1 == (uintptr_t)layer)
            return 1;
    return 0;
}

static void requireRecordedMatrix(Gfx* start, Gfx* end) {
    int found = 0;
    for (Gfx* p = start; p < end; ++p) {
        if ((p->words.w0 >> 24) == G_MTX) {
            REQUIRE(p->words.w1 == (uintptr_t)&matrix);
            ++found;
        }
    }
    REQUIRE(found == 1);
}

static u32 lastColor(Gfx* start, Gfx* end, int opcode) {
    u32 result = 0;
    for (Gfx* p = start; p < end; ++p)
        if ((p->words.w0 >> 24) == opcode)
            result = p->words.w1;
    return result;
}

static void requireBodyColorsPreserved(void) {
    // Custom meshes may inherit either color from the preceding limb. The fire
    // must not paint subsequent torso/arm sections, regardless of sword profile.
    for (int profile = 0; profile < 4; ++profile) {
        setup();
        customColors = 1;
        if (profile == 1) {
            gSaveContext.linkAge = LINK_AGE_CHILD;
            player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_KOKIRI;
        } else if (profile >= 2) {
            player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_BIGGORON;
            player.leftHandType = PLAYER_MODELTYPE_LH_BGS;
            gSaveContext.swordHealth = profile == 2 ? 8 : 0;
        }
        tick();
        gfx.polyOpa.p = opa;
        gfx.polyXlu.p = xlu;
        DinFireSword_BeginPlayerDraw(&play, &player);
        gDPSetPrimColor(gfx.polyOpa.p++, 0, 0, 93, 47, 201, 173);
        gDPSetEnvColor(gfx.polyOpa.p++, 17, 202, 64, 0);
        currentMatrix.xx = currentMatrix.yy = currentMatrix.zz = currentMatrix.ww = 1.0f;
        currentMatrix.xw = 91.0f;
        currentMatrix.yw = -32.0f;
        currentMatrix.zw = 67.0f;
        MtxF hand = currentMatrix;
        DinFireSword_Draw(&play, &player);
        REQUIRE(gfx.polyOpa.p == opa + 2 && gfx.polyXlu.p == xlu);
        REQUIRE(lastColor(opa, gfx.polyOpa.p, G_SETPRIMCOLOR) == 0x5D2FC9AD);
        REQUIRE(lastColor(opa, gfx.polyOpa.p, G_SETENVCOLOR) == 0x11CA4000);
        // Subsequent limbs change the current pose; the deferred fire must use
        // the captured hand and restore the caller's matrix afterwards.
        currentMatrix.xw = 999.0f;
        DinFireSword_DrawAfterPlayer(&play, &player);
        REQUIRE(containsLayer(opa, gfx.polyOpa.p, core));
        REQUIRE(containsLayer(xlu, gfx.polyXlu.p, flame));
        REQUIRE(matrices == 1 && depth == 0 && currentMatrix.xw == 999.0f);
        REQUIRE(!memcmp(&submittedMatrices[0], &hand, sizeof(hand)));
        requireRecordedMatrix(opa, gfx.polyOpa.p);
        requireRecordedMatrix(xlu, gfx.polyXlu.p);
    }
    puts("PASS fire sword preserves inherited adult/child/Master/BGS/broken body colors");
}

static void requireDrawLifetime(void) {
    setup();
    tick();
    gfx.polyOpa.p = opa;
    gfx.polyXlu.p = xlu;
    DinFireSword_BeginPlayerDraw(&play, &player);
    DinFireSword_Draw(&play, &player);
    // A new draw with no selected hand must not reuse a previous queued pose.
    DinFireSword_BeginPlayerDraw(&play, &player);
    DinFireSword_DrawAfterPlayer(&play, &player);
    REQUIRE(gfx.polyOpa.p == opa && gfx.polyXlu.p == xlu);
    DinFireSword_BeginPlayerDraw(&play, &player);
    DinFireSword_Draw(&play, &player);
    enabled = 0;
    DinFireSword_DrawAfterPlayer(&play, &player);
    enabled = 1;
    DinFireSword_DrawAfterPlayer(&play, &player);
    REQUIRE(gfx.polyOpa.p == opa && gfx.polyXlu.p == xlu);
    // Reflections precede the actual player in the same frame. Suppression must
    // not reset the animation context needed by the following normal pass.
    player.actor.scale.y = -.01f;
    REQUIRE(draw() == 0);
    player.actor.scale.y = .01f;
    REQUIRE(draw() > 0);
    Gfx* opaqueEnd = gfx.polyOpa.p;
    Gfx* effectsEnd = gfx.polyXlu.p;
    DinFireSword_DrawAfterPlayer(&play, &player);
    REQUIRE(gfx.polyOpa.p == opaqueEnd && gfx.polyXlu.p == effectsEnd);
    puts("PASS deferred fire draw: no stale/duplicate pose, disable recovery and reflection-to-player rendering");
}

int main(void) {
    requireBodyColorsPreserved();
    requireDrawLifetime();
    setup();
    tick();
    REQUIRE(draw() > 0);
    // An opaque core must precede room transparency; drawing it in XLU can
    // overwrite a foreground waterfall that has already blended over Link.
    REQUIRE(containsLayer(opa, gfx.polyOpa.p, core));
    REQUIRE(matrices == 1 && gfx.polyOpa.p > opa && gfx.polyXlu.p > xlu);
    REQUIRE(strstr(lastCorePath, "/adult/") != NULL);
    requireTexture(opa, gfx.polyOpa.p, "__OTR__objects/din_fire_sword/poc1/CoreTex");
    requireTexture(xlu, gfx.polyXlu.p, "__OTR__objects/din_fire_sword/poc1/FlameTex");
    customColors = 1;
    draw();
    REQUIRE(color(opa, gfx.polyOpa.p, G_SETPRIMCOLOR, 0x29F1C7));
    REQUIRE(color(xlu, gfx.polyXlu.p, G_SETENVCOLOR, 0x0C21B1));
    setup();
    player.itemAction = -1;
    tick();
    REQUIRE(draw() > 0);
    setup();
    player.stateFlags2 = PLAYER_STATE2_IDLE_FIDGET;
    tick();
    REQUIRE(draw() > 0);
    setup();
    enabled = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    alt = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    assets = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    loadFailure = 1;
    REQUIRE(draw() == 0);
    const char* deps[] = { "DinSleekEquipment", "CoreTex",      "FlameTex",     "CoreDL",
                           "FlameDL",           "CoreVertices", "FlameVertices" };
    for (size_t i = 0; i < ARRAY_COUNT(deps); ++i) {
        setup();
        missing = deps[i];
        tick();
        REQUIRE(draw() == 0);
    }
    setup();
    gSaveContext.linkAge = LINK_AGE_CHILD;
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_KOKIRI;
    tick();
    REQUIRE(draw() > 0);
    REQUIRE(strstr(lastCorePath, "/child/") != NULL);
    setup();
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_KOKIRI;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_BIGGORON;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_BIGGORON;
    player.leftHandType = PLAYER_MODELTYPE_LH_BGS;
    gSaveContext.swordHealth = 8;
    tick();
    REQUIRE(draw() > 0);
    REQUIRE(strstr(lastCorePath, "/bgs/") != NULL);
    gSaveContext.swordHealth = 0;
    tick();
    REQUIRE(draw() > 0);
    REQUIRE(strstr(lastCorePath, "/broken/") != NULL);
    player.leftHandType = PLAYER_MODELTYPE_LH_OPEN;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_CS;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.leftHandType = PLAYER_MODELTYPE_LH_OPEN;
    tick();
    REQUIRE(draw() == 0);
    setup();
    transformed = 1;
    tick();
    REQUIRE(draw() == 0);
    setup();
    invisible = 1;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.stateFlags1 = PLAYER_STATE1_DEAD;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.stateFlags1 = PLAYER_STATE1_IN_WATER;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.stateFlags2 = PLAYER_STATE2_DISABLE_DRAW;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.csAction = 1;
    tick();
    REQUIRE(draw() == 0);
    setup();
    play.transitionTrigger = TRANS_TRIGGER_START;
    tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    player.actor.scale.y = -.01f;
    REQUIRE(draw() == 0);
    player.actor.scale.y = .01f;
    REQUIRE(draw() > 0);
    setup();
    tick();
    Player other = player;
    gfx.polyOpa.p = opa;
    gfx.polyXlu.p = xlu;
    DinFireSword_Draw(&play, &other);
    REQUIRE(gfx.polyOpa.p == opa && gfx.polyXlu.p == xlu);
    setup();
    tick();
    draw();
    Gfx a[512], b[512];
    memcpy(a, opa, sizeof(a));
    memcpy(b, xlu, sizeof(b));
    draw();
    REQUIRE(!memcmp(a, opa, sizeof(a)) && !memcmp(b, xlu, sizeof(b)));
    DinFireSword_Update(&play, &player);
    draw();
    REQUIRE(!memcmp(a, opa, sizeof(a)) && !memcmp(b, xlu, sizeof(b)));
    play.pauseCtx.state = 6;
    tick();
    draw();
    REQUIRE(!memcmp(a, opa, sizeof(a)) && !memcmp(b, xlu, sizeof(b)));
    play.pauseCtx.state = 0;
    tick();
    draw();
    REQUIRE(memcmp(b, xlu, sizeof(b)) != 0);
    ++play.sceneNum;
    REQUIRE(draw() == 0);
    tick();
    REQUIRE(draw() > 0);
    DinFireSword_Reset();
    REQUIRE(draw() == 0);
    setup();
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_MASTER) == DMG_SLASH_MASTER);
    fireDamage = 1;
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_MASTER) == (DMG_SLASH_MASTER | DMG_ARROW_FIRE));
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_JUMP_MASTER) == (DMG_JUMP_MASTER | DMG_ARROW_FIRE));
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_HAMMER_SWING) == DMG_HAMMER_SWING);
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_MASTER | DMG_FIXED_DAMAGE) ==
            (DMG_SLASH_MASTER | DMG_FIXED_DAMAGE));
    otherOwner = 1;
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_MASTER) == DMG_SLASH_MASTER);
    otherOwner = 0;
    enabled = 0;
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_MASTER) == DMG_SLASH_MASTER);
    enabled = 1;
    assets = 0;
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_MASTER) == DMG_SLASH_MASTER);
    setup();
    gSaveContext.linkAge = LINK_AGE_CHILD;
    player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_KOKIRI;
    pakActive = 1;
    tick();
    REQUIRE(draw() > 0);
    REQUIRE(strstr(lastCorePath, "/child/") != NULL);
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_KOKIRI) == DMG_SLASH_KOKIRI);
    fireDamage = 1;
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_KOKIRI) == (DMG_SLASH_KOKIRI | DMG_ARROW_FIRE));
    for (bossOwner = 1; bossOwner <= 2; ++bossOwner) {
        // A PAK sword can leave the ordinary hand type intact for these owners.
        tick();
        REQUIRE(draw() == 0);
        REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_KOKIRI) == DMG_SLASH_KOKIRI);
    }
    bossOwner = 0;
    tick();
    REQUIRE(draw() > 0);
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_KOKIRI) == (DMG_SLASH_KOKIRI | DMG_ARROW_FIRE));
    otherOwner = 1;
    REQUIRE(DinFireSword_DamageFlags(&play, &player, DMG_SLASH_KOKIRI) == DMG_SLASH_KOKIRI);
    puts("PASS child Din fire rendering and opt-in damage with PAK equipment slots");
    setup();
    for (int q = 0; q < 2; ++q)
        player.meleeWeaponQuads[q].info.toucher.dmgFlags =
            DinFireSword_SetDamageFlags(&play, &player, q, DMG_JUMP_MASTER);
    fireDamage = 1;
    DinFireSword_RefreshDamage(&play, &player);
    REQUIRE(player.meleeWeaponQuads[0].info.toucher.dmgFlags == (DMG_JUMP_MASTER | DMG_ARROW_FIRE));
    fireDamage = 0;
    DinFireSword_RefreshDamage(&play, &player);
    REQUIRE(player.meleeWeaponQuads[0].info.toucher.dmgFlags == DMG_JUMP_MASTER);
    REQUIRE(player.meleeWeaponQuads[1].info.toucher.dmgFlags == DMG_JUMP_MASTER);
    fireDamage = 1;
    DinFireSword_RefreshDamage(&play, &player);
    enabled = 0;
    DinFireSword_RefreshDamage(&play, &player);
    REQUIRE(player.meleeWeaponQuads[0].info.toucher.dmgFlags == DMG_JUMP_MASTER);
    setup();
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gfx.polyOpa.p = opa;
    gfx.polyXlu.p = xlu;
    DinFireSword_DrawPedestal(&play);
    REQUIRE(gfx.polyXlu.p > xlu && strstr(lastCorePath, "/adult/"));
    enabled = 0;
    gfx.polyOpa.p = opa;
    gfx.polyXlu.p = xlu;
    DinFireSword_DrawPedestal(&play);
    REQUIRE(gfx.polyOpa.p == opa && gfx.polyXlu.p == xlu);
    enabled = 1;
    alt = 0;
    DinFireSword_DrawPedestal(&play);
    REQUIRE(gfx.polyOpa.p == opa);
    alt = 1;
    assets = 0;
    DinFireSword_DrawPedestal(&play);
    REQUIRE(gfx.polyOpa.p == opa);
    puts("PASS Din fire sword: HD handles, age/weapon guards, missing assets, cosmetics, pause, reset and unchanged "
         "player state");
    return 0;
}
