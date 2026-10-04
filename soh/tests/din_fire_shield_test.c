// Compile the production renderer with real game types and GBI. Only engine
// resource services and graphics allocation are fixtures; no game is running.
#include "global.h"
#include "din_fire_shield.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static GraphicsContext gfx;
static PlayState play;
static Player player;
static Gfx commands[512], opaqueCommands[512], surface[1], rim[1];
static u8 texture[64 * 32];
static Mtx matrix;
static int enabled, alt, assets, bracer, transformed, invisible, loadFailure;
static const char* missingDependency;
static int customColors;
static int fireSound, audioRequests;
static u16 lastSound;
static Actor* soundActor;
static int matrixDepth, matrixCount;
static float drawX, drawY, drawZ;
SaveContext gSaveContext;

#define REQUIRE(c)                                               \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            exit(1);                                             \
        }                                                        \
    } while (0)

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (!strcmp(name, "gEnhancements.DinFireShield"))
        return enabled;
    if (!strcmp(name, "gSettings.AltAssets"))
        return alt;
    if (!strcmp(name, "gEnhancements.DinFireShieldSfx"))
        return fireSound;
    return fallback;
}

Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    if (!customColors)
        return fallback;
    if (!strcmp(name, "gCosmetics.Custom.DinFireShieldCore.Value"))
        return (Color_RGB8){ 41, 241, 199 };
    if (!strcmp(name, "gCosmetics.Custom.DinFireShieldOuter.Value"))
        return (Color_RGB8){ 12, 33, 177 };
    return fallback;
}

uint8_t ResourceMgr_FileExists(const char* path) {
    if (missingDependency && strstr(path, missingDependency))
        return 0;
    if (strstr(path, "DinSleekEquipment"))
        return bracer;
    return assets;
}

Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    if (loadFailure)
        return NULL;
    return strstr(path, "RimDL") ? rim : surface;
}

void* ResourceGetDataByName(const char* path) {
    if (missingDependency && strstr(path, missingDependency))
        return NULL;
    return loadFailure ? NULL : texture;
}

u8 TransformMasks_IsTransformedAny(void) {
    return transformed;
}
uint8_t GameInteractor_InvisibleLinkActive(void) {
    return invisible;
}
void Audio_PlayActorSound2(Actor* actor, u16 sfxId) {
    ++audioRequests;
    lastSound = sfxId;
    soundActor = actor;
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
    ++matrixDepth;
}
void Matrix_Pop(void) {
    --matrixDepth;
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
    (void)mode;
    drawX = x;
    drawY = y;
    drawZ = z;
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    (void)x;
    (void)y;
    (void)z;
    (void)mode;
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    (void)context;
    (void)file;
    (void)line;
    ++matrixCount;
    return &matrix;
}

static void setup(void) {
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&gfx, 0, sizeof(gfx));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    play.state.gfxCtx = &gfx;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    player.actor.scale.y = 0.01f;
    player.currentShield = PLAYER_SHIELD_HYLIAN;
    player.rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
    player.stateFlags1 = PLAYER_STATE1_SHIELDING;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    enabled = alt = assets = bracer = 1;
    transformed = invisible = loadFailure = matrixDepth = matrixCount = 0;
    missingDependency = NULL;
    customColors = 0;
    fireSound = audioRequests = 0;
    lastSound = 0;
    soundActor = NULL;
    DinFireShield_Reset();
}

static size_t draw(void) {
    gfx.polyXlu.p = commands;
    DinFireShield_Draw(&play, &player);
    REQUIRE(matrixDepth == 0);
    REQUIRE(gfx.polyXlu.p < commands + 512);
    return (size_t)(gfx.polyXlu.p - commands);
}

static void tick(void) {
    ++play.gameplayFrames;
    DinFireShield_Update(&play, &player);
}

static int findColor(size_t count, int opcode, unsigned rgb) {
    for (size_t i = 0; i < count; ++i) {
        if ((commands[i].words.w0 >> 24) == opcode && ((commands[i].words.w1 >> 8) & 0xFFFFFF) == rgb)
            return 1;
    }
    return 0;
}

// Raw pixel pointers discard the resource's physical size/HD upload flags.
// Check the real packets consumed by the interpreter, including its alignment
// requirement for recognizing an OTR resource name.
static void requireNamedTextures(size_t count) {
    const char* paths[] = {
        "__OTR__objects/din_fire_shield/poc1/FlowTex",
        "__OTR__objects/din_fire_shield/poc1/FlameTex",
    };
    size_t found = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((commands[i].words.w0 >> 24) != G_SETTIMG)
            continue;
        REQUIRE(found < ARRAY_COUNT(paths));
        uintptr_t address = commands[i].words.w1;
        REQUIRE(address != 0 && (address & 1) == 0);
        REQUIRE(strcmp((const char*)address, paths[found]) == 0);
        ++found;
    }
    REQUIRE(found == ARRAY_COUNT(paths));
}

int main(void) {
    setup();
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_DEKU) != NULL);
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_HYLIAN) != NULL);
    REQUIRE(((uintptr_t)DinFireShield_ItemIcon(ITEM_SHIELD_DEKU) & 1) == 0);
    REQUIRE(strcmp(DinFireShield_ItemIcon(ITEM_SHIELD_DEKU), "__OTR__objects/din_fire_shield/poc1/IconTex") == 0);
    REQUIRE(strcmp(DinFireShield_ItemIcon(ITEM_SHIELD_HYLIAN), "__OTR__objects/din_fire_shield/poc1/IconTex") == 0);
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_MIRROR) == NULL);
    missingDependency = "IconTex";
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_DEKU) == NULL);
    setup();
    enabled = 0;
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_HYLIAN) == NULL);
    setup();
    alt = 0;
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_HYLIAN) == NULL);
    setup();
    bracer = 0;
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_HYLIAN) == NULL);
    setup();
    loadFailure = 1;
    REQUIRE(DinFireShield_ItemIcon(ITEM_SHIELD_HYLIAN) == NULL);
    setup();
    gfx.polyXlu.p = commands;
    gfx.polyOpa.p = opaqueCommands;
    customColors = 1;
    REQUIRE(DinFireShield_DrawItem(&play, GID_SHIELD_HYLIAN));
    REQUIRE(matrixDepth == 0 && gfx.polyOpa.p > opaqueCommands && gfx.polyXlu.p > commands);
    requireNamedTextures((size_t)(gfx.polyXlu.p - commands));
    // SetupDL_25 uses a fog blender. Disabling G_FOG alone does not disable it
    // in the port renderer, so the baked-color GI must set opaque non-fog mode.
    Gfx opaqueMode;
    gDPSetRenderMode(&opaqueMode, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    int foundOpaqueMode = 0;
    for (Gfx* command = opaqueCommands; command < gfx.polyOpa.p; ++command) {
        if (command->words.w0 == opaqueMode.words.w0 && command->words.w1 == opaqueMode.words.w1)
            foundOpaqueMode = 1;
    }
    REQUIRE(foundOpaqueMode);
    REQUIRE(findColor((size_t)(gfx.polyXlu.p - commands), G_SETPRIMCOLOR, 0x29F1C7));
    Gfx* beforeXlu = gfx.polyXlu.p;
    Gfx* beforeOpa = gfx.polyOpa.p;
    REQUIRE(!DinFireShield_DrawItem(&play, GID_SHIELD_MIRROR));
    missingDependency = "GIBracerVertices";
    REQUIRE(!DinFireShield_DrawItem(&play, GID_SHIELD_HYLIAN));
    REQUIRE(gfx.polyXlu.p == beforeXlu && gfx.polyOpa.p == beforeOpa);
    setup();
    tick();
    REQUIRE(draw() > 0);
    requireNamedTextures(draw());
    REQUIRE(matrixCount > 0);
    REQUIRE(drawX == -1500.0f && drawY == 0.0f && drawZ < -600.0f);
    // Live editor/Rainbow values must reach the emitted material commands,
    // including changes between draws while the simulation is paused.
    customColors = 1;
    size_t colorCount = draw();
    REQUIRE(findColor(colorCount, G_SETPRIMCOLOR, 0x29F1C7));
    REQUIRE(findColor(colorCount, G_SETENVCOLOR, 0x0C21B1));
    customColors = 0;
    colorCount = draw();
    REQUIRE(findColor(colorCount, G_SETPRIMCOLOR, 0xFFE17A));
    setup();
    enabled = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    assets = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    bracer = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    loadFailure = 1;
    REQUIRE(draw() == 0);
    const char* dependencies[] = { "FlowTex", "SurfaceVertices", "RimVertices" };
    for (int i = 0; i < 3; ++i) {
        setup();
        tick();
        missingDependency = dependencies[i];
        REQUIRE(draw() == 0);
    }
    setup();
    player.stateFlags1 = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.rightHandType = PLAYER_MODELTYPE_RH_CLOSED;
    tick();
    REQUIRE(draw() == 0);
    setup();
    gSaveContext.linkAge = LINK_AGE_CHILD;
    player.currentShield = PLAYER_SHIELD_DEKU;
    tick();
    REQUIRE(draw() > 0);
    requireNamedTextures(draw());
    setup();
    gSaveContext.linkAge = LINK_AGE_CHILD;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.currentShield = PLAYER_SHIELD_MIRROR;
    tick();
    REQUIRE(draw() == 0);
    setup();
    transformed = 1;
    tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    invisible = 1;
    REQUIRE(draw() == 0);
    setup();
    player.stateFlags1 |= PLAYER_STATE1_DEAD;
    tick();
    REQUIRE(draw() == 0);
    setup();
    player.stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;
    tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    alt = 0;
    REQUIRE(draw() == 0);
    tick();
    alt = 1;
    player.stateFlags1 = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    for (int i = 0; i < 8; ++i)
        tick();
    player.stateFlags1 = 0;
    tick();
    REQUIRE(draw() > 0);
    for (int i = 0; i < 10; ++i)
        tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    Player other = player;
    gfx.polyXlu.p = commands;
    DinFireShield_Draw(&play, &other);
    REQUIRE(gfx.polyXlu.p == commands);
    setup();
    tick();
    DinFireShield_Reset();
    REQUIRE(draw() == 0);
    // Reflective floors draw a negative-Y player pass before the normal pass.
    // Skipping that auxiliary draw must not erase the normal shield's state.
    setup();
    tick();
    player.actor.scale.y = -0.01f;
    REQUIRE(draw() == 0);
    player.actor.scale.y = 0.01f;
    REQUIRE(draw() > 0);
    setup();
    tick();
    ++play.sceneNum;
    player.stateFlags1 = 0;
    tick();
    REQUIRE(draw() == 0);
    setup();
    tick();
    gSaveContext.linkAge = LINK_AGE_CHILD;
    player.currentShield = PLAYER_SHIELD_DEKU;
    player.stateFlags1 = 0;
    tick();
    REQUIRE(draw() == 0);
    // Extra render calls and duplicate update calls for one simulation frame do not advance animation.
    setup();
    tick();
    draw();
    Gfx first[512];
    memcpy(first, commands, sizeof(first));
    size_t count = draw();
    REQUIRE(!memcmp(first, commands, count * sizeof(Gfx)));
    DinFireShield_Update(&play, &player);
    draw();
    REQUIRE(!memcmp(first, commands, count * sizeof(Gfx)));
    setup();
    tick();
    play.pauseCtx.state = 6;
    player.stateFlags1 = 0;
    for (int i = 0; i < 20; ++i)
        tick();
    REQUIRE(draw() > 0);
    play.pauseCtx.state = 0;
    for (int i = 0; i < 10; ++i)
        tick();
    REQUIRE(draw() == 0);
    // Opt-in audio uses Fire Arrow's sustained charge sound, only once per
    // simulation tick while an eligible, available shield is being guarded.
    setup();
    tick();
    REQUIRE(audioRequests == 0);
    fireSound = 1;
    tick();
    REQUIRE(audioRequests == 1);
    REQUIRE(lastSound == NA_SE_PL_ARROW_CHARGE_FIRE - SFX_FLAG && soundActor == &player.actor);
    REQUIRE(player.actor.sfx == 0);
    DinFireShield_Update(&play, &player);
    draw();
    draw();
    REQUIRE(audioRequests == 1);
    tick();
    REQUIRE(audioRequests == 2);
    player.stateFlags1 = 0;
    tick();
    REQUIRE(audioRequests == 2);
    player.stateFlags1 = PLAYER_STATE1_SHIELDING;
    play.pauseCtx.state = 6;
    tick();
    REQUIRE(audioRequests == 2);
    play.pauseCtx.state = 0;
    fireSound = 0;
    tick();
    REQUIRE(audioRequests == 2);
    setup();
    fireSound = 1;
    enabled = 0;
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    alt = 0;
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    bracer = 0;
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    missingDependency = "FlowTex";
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    loadFailure = 1;
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    player.currentShield = PLAYER_SHIELD_MIRROR;
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    transformed = 1;
    tick();
    REQUIRE(audioRequests == 0);
    setup();
    fireSound = 1;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    player.currentShield = PLAYER_SHIELD_DEKU;
    tick();
    REQUIRE(audioRequests == 1);
    puts("PASS Din fire shield: HD handles, opt-in Fire Arrow SFX, guard/fade, eligibility, resets, pause and draw "
         "stability");
    return 0;
}
