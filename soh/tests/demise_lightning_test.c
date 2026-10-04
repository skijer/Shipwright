// The item and native lightning/pool/matrix code are real. Only game input,
// animation, audio, unrelated particle allocation, and graphics allocation are
// fixtures. This verifies scheduling/render commands, not in-game appearance.
#include "global.h"
#include "overlays/effects/ovl_Effect_Ss_Lightning/z_eff_ss_lightning.h"
#include "mods/items/logic/item_demise_destruction.c"
#include "test_require.h"
#include <math.h>
#include <string.h>

extern EffectSsInfo sEffectSsInfo;
u32 EffectSsLightning_Init(PlayState*, u32, EffectSs*, void*);

CustomItemState gCustomItemState;
EffectSsOverlay gEffectSsOverlayTable[EFFECT_SS_TYPE_MAX];
Mtx gMtxClear;
f32 gSfxDefaultFreqAndVolScale = 1.0f;
s8 gSfxDefaultReverb;
static PlayState play;
static Player player;
static GraphicsContext gfx;
static Camera camera;
static EffectSs particles[85];
static Gfx commands[64];
static Mtx matrix;
static MtxF renderedMatrix;
static LinkAnimationHeader animation;
static ItemInputState input;
static EffectSsLightningInitParams roots[128];
static int rootCount, bombCount, smallBombCount, dustCount, magicSpent, animationChanges, animationUpdates;
static int collisionCalls, cameraRequests, cameraRestores, damaged, blocked, availableMagic, missingAnimation;
static int failedMatrix, forcedBranches;
static unsigned randomState;

static u32 recordLightning(PlayState* context, u32 index, EffectSs* effect, void* params) {
    REQUIRE(rootCount < ARRAY_COUNT(roots));
    roots[rootCount++] = *(EffectSsLightningInitParams*)params;
    return EffectSsLightning_Init(context, index, effect, params);
}
static EffectSsInit lightningInit = { EFFECT_SS_LIGHTNING, recordLightning };

f32 Rand_ZeroOne(void) {
    if (forcedBranches)
        return 0.0f; // Exercise the native effect's optional second branch every time.
    randomState = randomState * 1664525u + 1013904223u;
    return (randomState >> 8) * (1.0f / 16777216.0f);
}
f32 Rand_CenteredFloat(f32 scale) {
    return (Rand_ZeroOne() - 0.5f) * scale;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (3.14159265358979323846f / 32768.0f));
}
f32 Math_CosS(s16 angle) {
    return cosf(angle * (3.14159265358979323846f / 32768.0f));
}
void Math_Vec3f_Copy(Vec3f* destination, Vec3f* source) {
    *destination = *source;
}
void Color_RGBA8_Copy(Color_RGBA8* destination, Color_RGBA8* source) {
    *destination = *source;
}
s16 Camera_GetInputDirYaw(Camera* unused) {
    return camera.inputDir.y;
}
s32 FrameAdvance_IsEnabled(PlayState* unused) {
    return 0;
}
void Audio_StopSfxByPos(Vec3f* unused) {
}
void* ZeldaArena_MallocRDebug(size_t size, const char* file, s32 line) {
    REQUIRE(0); // The native gameplay_keep effect has no overlay allocation.
    return NULL;
}
s32 Overlay_Load(uintptr_t a, uintptr_t b, void* c, void* d, void* e) {
    REQUIRE(0);
    return 0;
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* format, ...) {
}
void EffectSsBomb2_SpawnLayered(PlayState* context, Vec3f* position, Vec3f* velocity, Vec3f* accel, s16 scale,
                                s16 step) {
    ++bombCount;
    if (scale == 15)
        ++smallBombCount;
    else {
        REQUIRE(scale == 80 || scale == 120);
        REQUIRE(step == 20 || step == 30);
    }
}
void func_8002829C(PlayState* context, Vec3f* position, Vec3f* velocity, Vec3f* accel, Color_RGBA8* prim,
                   Color_RGBA8* env, s16 scale, s16 step) {
    ++dustCount;
    REQUIRE(prim->r == 60 && prim->g == 0 && prim->b == 0);
    REQUIRE(scale == 300 && step == 10);
}
void Audio_PlaySoundGeneral(u16 sound, Vec3f* position, u8 token, f32* frequency, f32* volume, s8* reverb) {
}
void Rumble_Request(f32 a, u8 b, u8 c, u8 d) {
}
s32 ItemMagic_HasEnough(PlayState* context, s16 amount) {
    return availableMagic >= amount;
}
void ItemMagic_Consume(PlayState* context, s16 amount) {
    magicSpent += amount;
    availableMagic -= amount;
}
void ItemInput_Update(ItemInputState* state, u8 item, Player* p, PlayState* context) {
    REQUIRE(item == ITEM_DEMISE_DESTRUCTION);
    *state = input;
}
u8 ItemInput_CheckDamage(Player* p, s8* previous) {
    return damaged;
}
u8 ItemInput_IsBlocked(Player* p, PlayState* context) {
    return blocked;
}
Camera* Play_GetCamera(PlayState* context, s16 id) {
    return &camera;
}
s32 Camera_RequestSetting(Camera* unused, s16 setting) {
    REQUIRE(setting == CAM_SET_TURN_AROUND);
    ++cameraRequests;
    return 0;
}
void Camera_SetCameraData(Camera* unused, s16 flags, void* a, void* b, s16 c, s16 d, s32 e) {
    REQUIRE(flags == 4 && c == 10);
}
s16 func_8005B1A4(Camera* unused) {
    ++cameraRestores;
    return 0;
}
uint8_t ResourceMgr_FileExists(const char* path) {
    REQUIRE(!strcmp(path, NEI_ANIM_DEMISE_DESTRUCTION));
    return !missingAnimation;
}
LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* path) {
    return &animation;
}
s16 Animation_GetLastFrame(void* unused) {
    return 115;
}
void LinkAnimation_Change(PlayState* context, SkelAnime* skel, LinkAnimationHeader* anim, f32 speed, f32 start, f32 end,
                          u8 mode, f32 morph) {
    REQUIRE(anim == &animation && speed == 0.65f && start == 0.0f && end == 115.0f);
    REQUIRE(mode == ANIMMODE_ONCE && morph == -8.0f);
    ++animationChanges;
}
s32 LinkAnimation_Update(PlayState* context, SkelAnime* skel) {
    ++animationUpdates;
    return 0;
}
s32 Collider_InitCylinder(PlayState* context, ColliderCylinder* collider) {
    memset(collider, 0, sizeof(*collider));
    return 0;
}
s32 Collider_SetCylinder(PlayState* context, ColliderCylinder* collider, Actor* actor, ColliderCylinderInit* init) {
    collider->base.shape = init->base.shape;
    return 0;
}
s32 CollisionCheck_SetAT(PlayState* context, CollisionCheckContext* collision, Collider* collider) {
    REQUIRE(collider == &ddCollider.base);
    REQUIRE(ddCollider.dim.radius == 400 && ddCollider.dim.height == 200);
    REQUIRE(ddCollider.info.toucher.damage == 40 && ddCollider.info.toucher.effect == 1);
    REQUIRE(ddCollider.info.toucher.dmgFlags == (DMG_HAMMER_SWING | DMG_HAMMER_JUMP | DMG_BOOMERANG));
    REQUIRE(ddCollider.dim.pos.x == 100 && ddCollider.dim.pos.y == 200 && ddCollider.dim.pos.z == -300);
    ++collisionCalls;
    return 0;
}
void Gfx_SetupDL_61Xlu(GraphicsContext* context) {
    gDPPipeSync(context->polyXlu.p++);
}
void gSPDisplayList(Gfx* command, Gfx* list) {
    __gSPDisplayList(command, list);
}
void gSPSegment(void* command, int segment, uintptr_t address) {
    __gSPSegment(command, segment, address);
}
void FrameInterpolation_RecordOpenChild(const void* key, int id) {
}
void FrameInterpolation_RecordCloseChild(void) {
}
void* Graph_Alloc(GraphicsContext* context, size_t size) {
    REQUIRE(size == sizeof(Mtx));
    return failedMatrix ? NULL : &matrix;
}
void FrameInterpolation_RecordSkinMatrixMtxFToMtx(MtxF* source, Mtx* destination) {
    renderedMatrix = *source;
}
void guMtxF2L(float source[4][4], Mtx* destination) {
}

static void reset(void) {
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&gfx, 0, sizeof(gfx));
    memset(&camera, 0, sizeof(camera));
    memset(&gCustomItemState, 0, sizeof(gCustomItemState));
    play.state.gfxCtx = &gfx;
    play.cameraPtrs[0] = &camera;
    SkinMatrix_SetScale(&play.billboardMtxF, 1.0f, 1.0f, 1.0f);
    player.actor.world.pos = (Vec3f){ 100.0f, 200.0f, -300.0f };
    player.actor.bgCheckFlags = BGCHECKFLAG_GROUND;
    ddCollider.base.shape = COLSHAPE_CYLINDER;
    input = (ItemInputState){ .wasEquipped = 1 };
    sEffectSsInfo.table = particles;
    sEffectSsInfo.tableSize = ARRAY_COUNT(particles);
    sEffectSsInfo.searchStartIndex = 0;
    for (size_t i = 0; i < ARRAY_COUNT(particles); ++i)
        EffectSs_Reset(&particles[i]);
    gEffectSsOverlayTable[EFFECT_SS_LIGHTNING].initInfo = &lightningInit;
    rootCount = bombCount = smallBombCount = dustCount = magicSpent = animationChanges = animationUpdates = 0;
    collisionCalls = cameraRequests = cameraRestores = damaged = blocked = missingAnimation = failedMatrix = 0;
    availableMagic = 96;
    forcedBranches = 1;
    randomState = 23;
}

static int countParticles(void) {
    int count = 0;
    for (size_t i = 0; i < ARRAY_COUNT(particles); ++i) {
        if (particles[i].life >= 0) {
            ++count;
            REQUIRE(particles[i].pos.y >= player.actor.world.pos.y + 24.0f);
        }
    }
    return count;
}
static void checkPalette(EffectSsLightningInitParams* root) {
    REQUIRE(root->primColor.r >= 160 && root->primColor.g <= 100 && root->primColor.b == 255);
    REQUIRE(root->primColor.a == 255 && root->envColor.a == 255);
    REQUIRE(root->envColor.r <= 24 && root->envColor.g <= 8 && root->envColor.b <= 40);
    REQUIRE(root->scale >= 130 && root->life == 8);
    REQUIRE(root->pos.y >= player.actor.world.pos.y + 24.0f);
    REQUIRE(root->numBolts <= 1);
}

static void testChargeAndDraw(void) {
    reset();
    ddActive = 1;
    ddState = DEMISE_STATE_WINDUP;
    ddTimer = 27;
    Demise_StateWindup(&player, &play);
    REQUIRE(rootCount >= 2);
    for (int i = 0; i < rootCount; ++i)
        checkPalette(&roots[i]);
    REQUIRE(smallBombCount == 0);
    EffectSs_UpdateAll(&play);
    REQUIRE(countParticles() <= 6);
    REQUIRE(countParticles() > rootCount); // Real native branching remains visible during charging.
    EffectSs* bolt = &particles[0];
    memset(commands, 0, sizeof(commands));
    gfx.polyXlu.p = commands;
    bolt->draw(&play, 0, bolt);
    int prim = 0, env = 0, displayList = 0, texture = 0;
    for (Gfx* command = commands; command < gfx.polyXlu.p; ++command) {
        u32 opcode = command->words.w0 >> 24;
        if (opcode == G_SETPRIMCOLOR) {
            REQUIRE(command->words.w1 == 0xB941FFFF);
            ++prim;
        } else if (opcode == G_SETENVCOLOR) {
            REQUIRE(command->words.w1 == 0x0E0018FF);
            ++env;
        } else if (opcode == G_DL) {
            REQUIRE(!strcmp((const char*)command->words.w1, gEffLightningDL));
            ++displayList;
        } else if (opcode == G_MOVEWORD && ((command->words.w0 >> 16) & 0xFF) == G_MW_SEGMENT) {
            REQUIRE((command->words.w0 & 0xFFFF) == 8 * 4);
            REQUIRE(!strcmp((const char*)command->words.w1, gEffLightning2Tex));
            ++texture;
        }
    }
    REQUIRE(prim == 1 && env == 1 && displayList == 1 && texture == 1);
    REQUIRE(renderedMatrix.yw >= player.actor.world.pos.y + 24.0f);
    // A horizontal native roll must put its long local Y axis along screen X.
    REQUIRE(fabsf(renderedMatrix.xy) >= 1.3f && fabsf(renderedMatrix.yy) < 0.01f);
    failedMatrix = 1;
    gfx.polyXlu.p = commands;
    bolt->draw(&play, 0, bolt); // Native graphics-allocation failure safely skips the visible commands.
    REQUIRE(gfx.polyXlu.p - commands == 1);
    for (int i = 0; i < 12; ++i)
        EffectSs_UpdateAll(&play);
    REQUIRE(countParticles() == 0);
}

static void testCastTimeline(void) {
    reset();
    input.isPressed = 1;
    Handle_DemiseDestruction(&player, &play);
    input.isPressed = 0;
    REQUIRE(ddActive && ddTimer == -2 && magicSpent == 12);
    int maxParticles = 0, beforeSlam = 0, groundWaves = 0;
    f32 lastWaveRadius = 0.0f;
    for (int tick = -1; tick <= 75; ++tick) {
        int previousRoots = rootCount;
        Handle_DemiseDestruction(&player, &play);
        int spawned = rootCount - previousRoots;
        if (tick < 21)
            REQUIRE(spawned == 0);
        if (tick == 52 || tick == 56 || tick == 60 || tick == 64 || tick == 68)
            REQUIRE(spawned == 2);
        if (tick == 69)
            beforeSlam = rootCount;
        if (tick == 70 || tick == 72 || tick == 74) {
            REQUIRE(spawned == 6);
            ++groundWaves;
            EffectSsLightningInitParams* root = &roots[previousRoots];
            f32 radius = hypotf(root->pos.x - 100.0f, root->pos.z + 300.0f);
            REQUIRE(radius > lastWaveRadius + 100.0f || groundWaves == 1);
            REQUIRE(radius <= 400.0f);
            lastWaveRadius = radius;
            for (int i = previousRoots; i < rootCount; ++i) {
                REQUIRE(roots[i].numBolts == 0); // Ground arcs cannot recursively walk below the floor.
                REQUIRE(fabsf(hypotf(roots[i].pos.x - 100.0f, roots[i].pos.z + 300.0f) - radius) < 0.1f);
            }
        }
        if (tick == 70) {
            REQUIRE(ddState == DEMISE_STATE_FINISH && ddTimer == 0);
            REQUIRE(bombCount == 13 && smallBombCount == 0);
            REQUIRE(collisionCalls == 0);
            REQUIRE(!(player.stateFlags1 & (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_INPUT_DISABLED)));
        }
        if (tick >= 71 && tick <= 75)
            REQUIRE(collisionCalls == tick - 70);
        EffectSs_UpdateAll(&play);
        int live = countParticles();
        if (live > maxParticles)
            maxParticles = live;
        ++play.gameplayFrames;
    }
    REQUIRE(groundWaves == 3 && rootCount - beforeSlam == 18);
    REQUIRE(maxParticles <= 30);
    REQUIRE(!ddActive && ddState == DEMISE_STATE_IDLE && !(ddCollider.base.atFlags & AT_ON));
    REQUIRE(magicSpent == 12 && animationChanges == 1 && animationUpdates == 71);
    REQUIRE(cameraRequests == 1 && cameraRestores == 1);
    for (int i = 0; i < rootCount; ++i)
        checkPalette(&roots[i]);
    int endedRoots = rootCount;
    for (int i = 0; i < 16; ++i) {
        Handle_DemiseDestruction(&player, &play);
        EffectSs_UpdateAll(&play);
    }
    REQUIRE(rootCount == endedRoots && countParticles() == 0);
    printf("PASS Demise timeline: %d roots, peak %d native lightning slots, 13 final explosions, 5 damage ticks\n",
           rootCount, maxParticles);
}

static void testInterruptedAndUnavailableEffects(void) {
    for (int reason = 0; reason < 3; ++reason) {
        reset();
        ddActive = 1;
        ddState = DEMISE_STATE_WINDUP;
        ddTimer = 67;
        Handle_DemiseDestruction(&player, &play);
        EffectSs_UpdateAll(&play);
        REQUIRE(rootCount == 2);
        if (reason == 0)
            input.wasEquipped = 0;
        else if (reason == 1)
            damaged = 1;
        else
            input.otherButtonPressed = 1;
        for (int i = 0; i < 16; ++i) {
            Handle_DemiseDestruction(&player, &play);
            EffectSs_UpdateAll(&play);
        }
        REQUIRE(!ddActive && rootCount == 2 && countParticles() == 0 && bombCount == 0);
    }
    reset();
    // A full pool of higher-priority effects makes the real native allocator
    // reject bolts; the item still reaches its unchanged damage/finish path.
    for (size_t i = 0; i < ARRAY_COUNT(particles); ++i) {
        particles[i].life = 100;
        particles[i].priority = 0;
    }
    ddActive = 1;
    ddState = DEMISE_STATE_WINDUP;
    ddTimer = 69;
    Handle_DemiseDestruction(&player, &play);
    for (int i = 0; i < 5; ++i)
        Handle_DemiseDestruction(&player, &play);
    REQUIRE(rootCount == 0 && bombCount == 13 && collisionCalls == 5 && !ddActive);
    for (size_t i = 0; i < ARRAY_COUNT(particles); ++i)
        REQUIRE(particles[i].life == 100 && particles[i].priority == 0);
}

static void testStartRestrictionsAndFallbackAnimation(void) {
    reset();
    availableMagic = 11;
    Demise_Start(&player, &play);
    REQUIRE(!ddActive && !magicSpent && !rootCount);
    availableMagic = 96;
    player.actor.bgCheckFlags = 0;
    Demise_Start(&player, &play);
    REQUIRE(!ddActive && !magicSpent && !rootCount);
    player.actor.bgCheckFlags = BGCHECKFLAG_GROUND;
    missingAnimation = 1;
    Demise_Start(&player, &play);
    for (int i = 0; i < 72; ++i)
        Demise_StateWindup(&player, &play);
    REQUIRE(animationChanges == 0 && animationUpdates == 71 && ddState == DEMISE_STATE_FINISH);
    REQUIRE(magicSpent == 12 && bombCount == 13);
}

int main(void) {
    testChargeAndDraw();
    testCastTimeline();
    testInterruptedAndUnavailableEffects();
    testStartRestrictionsAndFallbackAnimation();
    puts("PASS Demise native draw, palette, scheduling, stop, allocation fallback, and gameplay invariants");
    return 0;
}
