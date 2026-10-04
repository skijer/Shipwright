// Real actor code, game headers, and GBI. Engine allocation/resource services
// are fixtures; these checks cannot prove visual appearance in a running game.
#include "global.h"
#include "overlays/actors/ovl_Arrow_Fire/z_arrow_fire.h"
#include "overlays/actors/ovl_Arrow_Ice/z_arrow_ice.h"
#include "overlays/actors/ovl_Arrow_Light/z_arrow_light.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "soh/ResourceManagerHelpers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ArrowIce_Draw(Actor*, PlayState*);
void ArrowIce_Charge(ArrowIce*, PlayState*);
void ArrowIce_Fly(ArrowIce*, PlayState*);
void ArrowIce_Hit(ArrowIce*, PlayState*);
void ArrowFire_Charge(ArrowFire*, PlayState*);
void ArrowFire_Fly(ArrowFire*, PlayState*);
void ArrowFire_Hit(ArrowFire*, PlayState*);
void ArrowLight_Charge(ArrowLight*, PlayState*);
void ArrowLight_Fly(ArrowLight*, PlayState*);
void ArrowLight_Hit(ArrowLight*, PlayState*);

static GraphicsContext gfx;
static PlayState play;
static ArrowIce ice;
static EnArrow arrow;
static Gfx commands[256], reference[256], scroll[1];
static Mtx matrix;
static u8 pixels[64 * 64];
static int alt, asset, badLoad, customColors, matrixCount, matrixDepth;
static int audioCalls, magicCalls;
static int elementalImpactSounds, lastAudioFlagged;
static u16 lastAudioId;
static Actor* lastAudioActor;
static int interpolationDepth, interpolationIds[8], matrixIds[8];
static const void* interpolationKeys[8];
static const void* matrixKeys[8];
static f32 lastX, lastY, lastZ, lastScale, lastRotation, scales[8], rotations[8], positions[8][3];
static const char* texturePath = "__OTR__custom/henriko_effects/arrows/ice_snowflake_poc2";

#define REQUIRE(c)                                               \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            exit(1);                                             \
        }                                                        \
    } while (0)

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (!strcmp(name, CVAR_COSMETIC("Arrows.ElementalImpactSounds"))) {
        return elementalImpactSounds < 0 ? fallback : elementalImpactSounds;
    }
    return strstr(name, "Changed") ? customColors : fallback;
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 fallback) {
    return strstr(name, "Primary") ? (Color_RGB8){ 241, 73, 19 } : (Color_RGB8){ 11, 23, 47 };
}
bool ResourceMgr_IsAltAssetsEnabled(void) {
    return alt;
}
uint8_t ResourceMgr_FileAltExists(const char* path) {
    return asset && !strcmp(path, texturePath);
}
void* ResourceGetDataByName(const char* path) {
    REQUIRE(!strcmp(path, texturePath));
    return badLoad ? NULL : pixels;
}
void FrameInterpolation_RecordOpenChild(const void* key, int id) {
    REQUIRE(interpolationDepth < ARRAY_COUNT(interpolationKeys));
    interpolationKeys[interpolationDepth] = key;
    interpolationIds[interpolationDepth++] = id;
}
void FrameInterpolation_RecordCloseChild(void) {
    REQUIRE(interpolationDepth > 0);
    --interpolationDepth;
}
void Gfx_SetupDL_25Xlu(GraphicsContext* context) {
    gDPPipeSync(context->polyXlu.p++);
}
Gfx* Gfx_SetupDL_57(Gfx* p) {
    gDPPipeSync(p++);
    return p;
}
Gfx* Gfx_TwoTexScrollEx(GraphicsContext* context, s32 a, u32 b, u32 c, s32 d, s32 e, s32 f, u32 g, u32 h, s32 i, s32 j,
                        s32 k, s32 l, s32 m, s32 n) {
    return scroll;
}
void gSPDisplayList(Gfx* packet, Gfx* list) {
    __gSPDisplayList(packet, list);
}
void gSPVertex(Gfx* packet, uintptr_t vertices, int count, int first) {
    __gSPVertex(packet, vertices, count, first);
}
void Matrix_Push(void) {
    ++matrixDepth;
}
void Matrix_Pop(void) {
    --matrixDepth;
}
void Matrix_Translate(f32 x, f32 y, f32 z, u8 mode) {
    if (mode == MTXMODE_NEW) {
        lastX = x;
        lastY = y;
        lastZ = z;
        lastRotation = 0.0f;
    }
}
void Matrix_Scale(f32 x, f32 y, f32 z, u8 mode) {
    lastScale = x;
}
void Matrix_RotateX(f32 a, u8 mode) {
}
void Matrix_RotateY(f32 a, u8 mode) {
}
void Matrix_RotateZ(f32 a, u8 mode) {
    lastRotation = mode == MTXMODE_NEW ? a : lastRotation + a;
}
void Matrix_RotateZYX(s16 x, s16 y, s16 z, u8 mode) {
}
void Matrix_ReplaceRotation(MtxF* m) {
    lastRotation = 0.0f;
}
Mtx* Matrix_NewMtx(GraphicsContext* context, char* file, s32 line) {
    REQUIRE(matrixCount < 8 && interpolationDepth > 0);
    scales[matrixCount] = lastScale;
    rotations[matrixCount] = lastRotation;
    matrixKeys[matrixCount] = interpolationKeys[interpolationDepth - 1];
    matrixIds[matrixCount] = interpolationIds[interpolationDepth - 1];
    positions[matrixCount][0] = lastX;
    positions[matrixCount][1] = lastY;
    positions[matrixCount++][2] = lastZ;
    return &matrix;
}
void Actor_Kill(Actor* actor) {
    actor->update = NULL;
}
void Actor_SetScale(Actor* actor, f32 scale) {
    actor->scale = (Vec3f){ scale, scale, scale };
}
void Actor_ProcessInitChain(Actor* actor, InitChainEntry* chain) {
}
void Actor_PlaySfx_Flagged(Actor* actor, u16 id) {
    ++audioCalls;
    lastAudioActor = actor;
    lastAudioId = id;
    lastAudioFlagged = 1;
}
void Audio_PlayActorSound2(Actor* actor, u16 id) {
    ++audioCalls;
    lastAudioActor = actor;
    lastAudioId = id;
    lastAudioFlagged = 0;
}
void Magic_Reset(PlayState* context) {
    ++magicCalls;
}
f32 Math_Vec3f_DistXYZ(Vec3f* a, Vec3f* b) {
    return sqrtf(SQ(a->x - b->x) + SQ(a->y - b->y) + SQ(a->z - b->z));
}
static void alive(Actor* a, PlayState* p) {
}

static void setup(void) {
    memset(&gfx, 0, sizeof(gfx));
    memset(&play, 0, sizeof(play));
    memset(&ice, 0, sizeof(ice));
    memset(&arrow, 0, sizeof(arrow));
    play.state.gfxCtx = &gfx;
    play.state.frames = play.gameplayFrames = 42;
    arrow.actor.update = alive;
    arrow.actor.world.pos = (Vec3f){ 10, 20, 30 };
    ice.actor.parent = &arrow.actor;
    ice.actor.world.pos = (Vec3f){ 100, 200, 300 };
    ice.actor.projectedW = 300.0f;
    ice.radius = 10;
    ice.alpha = 100;
    ice.unk_160 = 1.0f;
    ice.actionFunc = ArrowIce_Charge;
    alt = asset = 1;
    badLoad = customColors = audioCalls = magicCalls = 0;
    elementalImpactSounds = -1; // Unset CVar must retain the native impact sound.
}
static size_t draw(void) {
    ArrowIce before = ice;
    EnArrow parentBefore = arrow;
    int audioBefore = audioCalls, magicBefore = magicCalls;
    memset(commands, 0, sizeof(commands));
    gfx.polyXlu.p = commands;
    matrixCount = matrixDepth = interpolationDepth = 0;
    ArrowIce_Draw(&ice.actor, &play);
    REQUIRE(matrixDepth == 0 && interpolationDepth == 0 && gfx.polyXlu.p < commands + ARRAY_COUNT(commands));
    REQUIRE(memcmp(&before, &ice, sizeof(ice)) == 0);
    REQUIRE(memcmp(&parentBefore, &arrow, sizeof(arrow)) == 0);
    REQUIRE(audioCalls == audioBefore && magicCalls == magicBefore);
    return (size_t)(gfx.polyXlu.p - commands);
}
static int textures(size_t count, const char* suffix) {
    int found = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((commands[i].words.w0 >> 24) != G_SETTIMG)
            continue;
        uintptr_t address = commands[i].words.w1;
        REQUIRE(address && !(address & 1));
        REQUIRE(!strncmp((const char*)address, "__OTR__", 7));
        if (strstr((const char*)address, suffix))
            ++found;
    }
    return found;
}
static int primAlpha(size_t count, unsigned rgb) {
    for (size_t i = count; i-- > 0;) {
        if ((commands[i].words.w0 >> 24) == G_SETPRIMCOLOR && ((commands[i].words.w1 >> 8) & 0xFFFFFF) == rgb)
            return commands[i].words.w1 & 255;
    }
    return -1;
}
static int nativeGeometry(size_t count) {
    int found = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((commands[i].words.w0 >> 24) != G_DL || commands[i].words.w1 == (uintptr_t)scroll)
            continue;
        const char* path = (const char*)commands[i].words.w1;
        REQUIRE(path != NULL);
        if (!strcmp(path, "__OTR__overlays/ovl_Arrow_Ice/sMaterialDL") ||
            !strcmp(path, "__OTR__overlays/ovl_Arrow_Ice/sModelDL"))
            ++found;
    }
    return found;
}
static void hit(u16 timer) {
    ice.actionFunc = ArrowIce_Hit;
    ice.timer = timer;
    ice.alpha = 255;
    ice.unk_164 = 1.0f;
}

static void heldChargeMotion(void) {
    setup();
    play.gameplayFrames = 0;
    size_t count = draw();
    f32 previousRotation = rotations[0];
    f32 previousScale = scales[0];
    int previousAlpha = primAlpha(count, 0xAAFFFF);
    f32 travel = 0.0f, minScale = previousScale, maxScale = previousScale;
    int minAlpha = previousAlpha, maxAlpha = previousAlpha;

    // Renderer inputs must move continuously over twelve seconds of held charge.
    // A static quad, rapid spin, abrupt loop or flashing pulse must fail.
    for (int frame = 1; frame <= 240; ++frame) {
        play.gameplayFrames = frame;
        count = draw();
        REQUIRE(matrixCount == 1);
        f32 rotation = rotations[0];
        f32 scale = scales[0];
        int alpha = primAlpha(count, 0xAAFFFF);
        f32 step = remainderf(rotation - previousRotation, 2.0f * M_PI);
        REQUIRE(step > 0.0f && step < 0.04f);
        REQUIRE(scale > 0.355f && scale < 0.395f);
        REQUIRE(alpha >= 80 && alpha <= 100);
        REQUIRE(fabsf(scale - previousScale) < 0.003f && abs(alpha - previousAlpha) <= 2);
        travel += step;
        minScale = MIN(minScale, scale);
        maxScale = MAX(maxScale, scale);
        minAlpha = MIN(minAlpha, alpha);
        maxAlpha = MAX(maxAlpha, alpha);
        previousRotation = rotation;
        previousScale = scale;
        previousAlpha = alpha;
    }
    REQUIRE(travel > 6.27f && travel < 7.55f); // One turn every ten to twelve seconds.
    REQUIRE(maxScale - minScale > 0.015f);
    REQUIRE(maxAlpha - minAlpha >= 8 && maxAlpha - minAlpha <= 24);

    play.gameplayFrames = 45;
    count = draw();
    memcpy(reference, commands, sizeof(commands));
    f32 pausedRotation = rotations[0], pausedScale = scales[0];
    // Render time continues while paused; only gameplay time may animate charge.
    play.pauseCtx.state = 6;
    play.state.frames += 123;
    REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    REQUIRE(rotations[0] == pausedRotation && scales[0] == pausedScale);
    REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    REQUIRE(rotations[0] == pausedRotation && scales[0] == pausedScale);
}

static void interpolationKeepsPhasesSeparate(void) {
    setup();
    draw();
    const void* chargeKey = matrixKeys[0];
    int chargeId = matrixIds[0];
    hit(24);
    draw();
    REQUIRE(matrixCount == 2);
    // The charge matrix must not interpolate into the impact flash, and the
    // two impact layers must retain independent interpolation identities.
    REQUIRE(matrixKeys[0] != chargeKey || matrixIds[0] != chargeId);
    REQUIRE(matrixKeys[1] != chargeKey || matrixIds[1] != chargeId);
    REQUIRE(matrixKeys[0] != matrixKeys[1] || matrixIds[0] != matrixIds[1]);
    REQUIRE(rotations[0] == 0.0f && rotations[1] == 0.0f);
    f32 impactScale = scales[1];
    play.gameplayFrames += 45;
    draw();
    REQUIRE(rotations[0] == 0.0f && rotations[1] == 0.0f && scales[1] == impactScale);
}

static void releaseKeepsSnowflake(void) {
    Actor holder = { 0 };
    setup();
    arrow.actor.parent = &holder;
    arrow.timer = 50;
    ArrowIce_Charge(&ice, &play);
    size_t count = draw();
    f32 chargeScale = scales[0], chargeRotation = rotations[0];
    int chargeAlpha = primAlpha(count, 0xAAFFFF), chargeId = matrixIds[0];
    const void* chargeKey = matrixKeys[0];

    // Exercise the real release transition. Its native alpha jumps to 255;
    // the visible sprite must retain its size, opacity and interpolation key.
    arrow.actor.parent = NULL;
    ArrowIce_Charge(&ice, &play);
    REQUIRE(ice.actionFunc == ArrowIce_Fly && ice.alpha == 255 && ice.radius == 10);
    count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 1 && nativeGeometry(count) == 0);
    REQUIRE(matrixCount == 1 && scales[0] == chargeScale && rotations[0] == chargeRotation);
    REQUIRE(primAlpha(count, 0xAAFFFF) == chargeAlpha);
    REQUIRE(matrixKeys[0] == chargeKey && matrixIds[0] == chargeId);

    for (int frame = 0; frame < 8; ++frame) {
        ++play.gameplayFrames;
        arrow.actor.world.pos.x += 25;
        ArrowIce_Fly(&ice, &play);
        count = draw();
        REQUIRE(textures(count, "ice_snowflake_poc2") == 1 && nativeGeometry(count) == 0);
        REQUIRE(matrixCount == 1 && positions[0][0] == arrow.actor.world.pos.x);
        REQUIRE(scales[0] > 0.355f && scales[0] < 0.395f);
        REQUIRE(matrixKeys[0] == chargeKey && matrixIds[0] == chargeId);
    }
    memcpy(reference, commands, sizeof(commands));
    play.pauseCtx.state = 6;
    play.state.frames += 123;
    REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    play.pauseCtx.state = 0;

    // The pre-existing lifetime alpha must also fade the flight sprite, without
    // letting the native cone return during the tail of flight.
    arrow.timer = 33;
    int previousAlpha = primAlpha(count, 0xAAFFFF);
    for (int frame = 0; frame < 5; ++frame) {
        ArrowIce_Fly(&ice, &play);
        count = draw();
        REQUIRE(textures(count, "ice_snowflake_poc2") == 1 && nativeGeometry(count) == 0);
        int alpha = primAlpha(count, 0xAAFFFF);
        REQUIRE(alpha > 0 && alpha < previousAlpha);
        previousAlpha = alpha;
    }

    arrow.hitFlags = 1;
    ArrowIce_Fly(&ice, &play);
    REQUIRE(ice.actionFunc == ArrowIce_Hit && ice.timer == 32);
    count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 1 && textures(count, "gFlashTex") == 1);
    REQUIRE(nativeGeometry(count) == 0 && matrixCount == 2);
    REQUIRE(matrixKeys[0] != chargeKey || matrixIds[0] != chargeId);
    REQUIRE(matrixKeys[1] != chargeKey || matrixIds[1] != chargeId);
}

static void nativeImpactSounds(void) {
    static const int options[] = { -1, 1, 0 };
    Actor holder = { 0 };
    for (size_t step = 0; step < ARRAY_COUNT(options); ++step) {
        setup();
        elementalImpactSounds = options[step];
        ArrowFire fire = { 0 };
        ArrowLight light = { 0 };
        fire.actor.parent = light.actor.parent = &arrow.actor;
        arrow.actor.parent = &holder;
        arrow.timer = 50;
        ArrowFire_Charge(&fire, &play);
        REQUIRE(lastAudioId == NA_SE_PL_ARROW_CHARGE_FIRE - SFX_FLAG && lastAudioFlagged);
        ArrowIce_Charge(&ice, &play);
        REQUIRE(lastAudioId == NA_SE_PL_ARROW_CHARGE_ICE - SFX_FLAG && lastAudioFlagged);
        ArrowLight_Charge(&light, &play);
        REQUIRE(lastAudioId == NA_SE_PL_ARROW_CHARGE_LIGHT - SFX_FLAG && lastAudioFlagged);
        REQUIRE(audioCalls == 3);

        arrow.actor.parent = NULL;
        audioCalls = 0;
        ArrowFire_Fly(&fire, &play);
        ArrowIce_Fly(&ice, &play);
        ArrowLight_Fly(&light, &play);
        REQUIRE(audioCalls == 0);
        arrow.hitFlags = 1;

        ArrowFire_Fly(&fire, &play);
        REQUIRE(audioCalls == 1 && lastAudioActor == &fire.actor && !lastAudioFlagged);
        REQUIRE(lastAudioId == (options[step] > 0 ? NA_SE_EV_FLAME_IGNITION : NA_SE_IT_EXPLOSION_FRAME));
        REQUIRE(fire.actionFunc == ArrowFire_Hit && fire.timer == 32 && fire.alpha == 255);
        ArrowFire_Hit(&fire, &play);
        REQUIRE(audioCalls == 1);

        ArrowIce_Fly(&ice, &play);
        REQUIRE(audioCalls == 2 && lastAudioActor == &ice.actor && !lastAudioFlagged);
        REQUIRE(lastAudioId == (options[step] > 0 ? NA_SE_EV_ICE_BROKEN : NA_SE_IT_EXPLOSION_ICE));
        REQUIRE(ice.actionFunc == ArrowIce_Hit && ice.timer == 32 && ice.alpha == 255);
        ArrowIce_Hit(&ice, &play);
        REQUIRE(audioCalls == 2);

        ArrowLight_Fly(&light, &play);
        REQUIRE(audioCalls == 3 && lastAudioActor == &light.actor && !lastAudioFlagged);
        REQUIRE(lastAudioId == (options[step] > 0 ? NA_SE_EN_LIGHT_ARROW_HIT : NA_SE_IT_EXPLOSION_LIGHT));
        REQUIRE(light.actionFunc == ArrowLight_Hit && light.timer == 32 && light.alpha == 255);
        ArrowLight_Hit(&light, &play);
        REQUIRE(audioCalls == 3);
    }
}

int main(void) {
    nativeImpactSounds();
    // Missing addition: charging must submit one named high-resolution sprite.
    setup();
    size_t count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 1);
    REQUIRE(primAlpha(count, 0xAAFFFF) > 0 && primAlpha(count, 0xAAFFFF) < 128);
    REQUIRE(nativeGeometry(count) == 0);
    heldChargeMotion();
    interpolationKeepsPhasesSeparate();
    releaseKeepsSnowflake();

    // Fallback paths must produce identical native packets for every phase.
    for (int phase = 0; phase < 3; ++phase) {
        setup();
        if (phase == 1)
            ice.actionFunc = ArrowIce_Fly;
        if (phase == 2)
            hit(24);
        alt = 0;
        count = draw();
        REQUIRE(nativeGeometry(count) == 2);
        memcpy(reference, commands, sizeof(commands));
        alt = 1;
        asset = 0;
        REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
        asset = 1;
        badLoad = 1;
        REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    }
    setup();
    ice.actionFunc = ArrowIce_Fly;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 1);
    setup();
    ice.radius = 0;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 0);
    setup();
    ice.actor.parent = NULL;
    REQUIRE(draw() == 0);
    setup();
    arrow.actor.update = NULL;
    REQUIRE(draw() == 0);

    // Impact expands at its captured position; moving the parent cannot drag it.
    setup();
    hit(32);
    count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 1);
    REQUIRE(nativeGeometry(count) == 0);
    REQUIRE(textures(count, "gFlashTex") == 1);
    f32 initialScale = scales[matrixCount - 1];
    hit(24);
    count = draw();
    REQUIRE(scales[matrixCount - 1] > initialScale);
    for (int i = 0; i < matrixCount; ++i)
        REQUIRE(positions[i][0] == 100 && positions[i][1] == 200 && positions[i][2] == 300);
    int peakAlpha = primAlpha(count, 0xAAFFFF);
    arrow.actor.world.pos = (Vec3f){ -999, -999, -999 };
    hit(17);
    count = draw();
    REQUIRE(primAlpha(count, 0xAAFFFF) > 0 && primAlpha(count, 0xAAFFFF) < peakAlpha);
    REQUIRE(positions[matrixCount - 1][0] == 100);
    hit(16);
    count = draw();
    REQUIRE(textures(count, "ice_snowflake_poc2") == 0);
    REQUIRE(textures(count, "gFlashTex") == 0);
    hit(255);
    REQUIRE(draw() == 0);

    // Live cosmetics and pause redraws must not advance the effect or mutate state.
    setup();
    hit(24);
    customColors = 1;
    count = draw();
    REQUIRE(primAlpha(count, 0xF14913) > 0);
    memcpy(reference, commands, sizeof(commands));
    play.pauseCtx.state = 6;
    REQUIRE(draw() == count && !memcmp(reference, commands, sizeof(commands)));
    alt = 0;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 0);
    alt = 1;
    REQUIRE(textures(draw(), "ice_snowflake_poc2") == 1);
    puts("PASS: continuous custom charge/release/flight/impact, gentle held motion, phase/layer interpolation, "
         "native fallbacks, missing/failed assets, phase gates, flight fade, captured position, expansion/fade, "
         "cosmetics, pause, draw-only state preservation, native Fire/Ice/Light opt-in impact sounds");
    return 0;
}
