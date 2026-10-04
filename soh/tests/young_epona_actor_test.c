/* Real actor entry points; only the engine boundaries are substituted. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "overlays/actors/ovl_En_Horse/z_en_horse.c"

#define CHECK(condition)                                                         \
    do {                                                                         \
        if (!(condition)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)
#define NEAR(a, b) CHECK(fabsf((a) - (b)) < 0.001f)

static int limbCalls[64];
static int limbCallCount;
SaveContext gSaveContext;
static GameInfo gameInfo;
GameInfo* gGameInfo = &gameInfo;
Vec3f gSfxDefaultPos;
f32 gSfxDefaultFreqAndVolScale;
s8 gSfxDefaultReverb;
static int canUseYoung = 1;
static int killed;
static int animationFinished;
static AnimationHeader* selectedAnimation;
static SkeletonHeader* selectedSkeleton;
static Vec3s rootJoint;
static CollisionPoly wallPoly, topPoly, groundPoly, behindPoly;
static f32 obstacleTop, obstacleBehind, obstacleDistance;
static CollisionPoly* blockedPoly;
static CollisionPoly* specialFloorPoly;
static int raycastCount, wallPresent = 1;
static int childObjectMissing;
static int spawnedChildObject;

s32 Horse_CanUseYoungEpona(void) {
    return canUseYoung;
}
void Actor_Kill(Actor* actor) {
    actor->update = NULL;
    killed++;
}
void Actor_ProcessInitChain(Actor* actor, InitChainEntry* chain) {
}
void Actor_SetScale(Actor* actor, f32 scale) {
    actor->scale = (Vec3f){ scale, scale, scale };
}
void ActorShadow_DrawHorse(Actor* actor, Lights* lights, PlayState* play) {
}
void ActorShape_Init(ActorShape* shape, f32 offset, ActorShadowFunc shadowDraw, f32 scale) {
}
void Actor_SetObjectDependency(PlayState* play, Actor* actor) {
}
s32 Object_GetIndex(ObjectContext* context, s16 object) {
    if (object == OBJECT_HORSE_LINK_CHILD && childObjectMissing) {
        return -1;
    }
    return 1;
}
s32 Object_Spawn(ObjectContext* context, s16 object) {
    CHECK(object == OBJECT_HORSE_LINK_CHILD);
    spawnedChildObject++;
    return 2;
}
s32 Object_IsLoaded(ObjectContext* context, s32 index) {
    return 1;
}
s32 Flags_GetEventChkInf(s32 flag) {
    return 0;
}
s32 Collider_InitCylinder(PlayState* play, ColliderCylinder* collider) {
    return 1;
}
s32 Collider_SetCylinder(PlayState* play, ColliderCylinder* collider, Actor* actor, ColliderCylinderInit* init) {
    collider->dim = init->dim;
    return 1;
}
s32 Collider_InitJntSph(PlayState* play, ColliderJntSph* collider) {
    return 1;
}
s32 Collider_SetJntSph(PlayState* play, ColliderJntSph* collider, Actor* actor, ColliderJntSphInit* init,
                       ColliderJntSphElement* elements) {
    collider->elements = elements;
    collider->count = init->count;
    elements->dim.modelSphere = init->elements->dim.modelSphere;
    return 1;
}
DamageTable* DamageTable_Get(s32 index) {
    return NULL;
}
void CollisionCheck_SetInfo(CollisionCheckInfo* info, DamageTable* table, CollisionCheckInfoInit* init) {
}
void Skin_Init(PlayState* play, Skin* skin, SkeletonHeader* skeleton, AnimationHeader* animation) {
    selectedSkeleton = skeleton;
    selectedAnimation = animation;
    skin->skelAnime.jointTable = &rootJoint;
}
void Animation_Change(SkelAnime* skel, AnimationHeader* animation, f32 speed, f32 start, f32 end, u8 mode, f32 morph) {
    selectedAnimation = animation;
    skel->curFrame = start;
    skel->playSpeed = speed;
}
void Animation_PlayOnce(SkelAnime* skel, AnimationHeader* animation) {
    selectedAnimation = animation;
}
s16 Animation_GetLastFrame(void* animation) {
    return 40;
}
s32 SkelAnime_Update(SkelAnime* skel) {
    return animationFinished;
}
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* freq, f32* volume, s8* reverb) {
}
void Rumble_Request(f32 distance, u8 strength, u8 length, u8 decay) {
}
void func_80028A54(PlayState* play, f32 radius, Vec3f* pos) {
}
Actor* Actor_Spawn(ActorContext* context, PlayState* play, s16 id, f32 x, f32 y, f32 z, s16 rx, s16 ry, s16 rz,
                   s16 params) {
    return NULL;
}
void Interface_InitHorsebackArchery(PlayState* play) {
}
f32 Actor_WorldDistXZToActor(Actor* a, Actor* b) {
    return 25.0f;
}
s16 Actor_WorldYawTowardActor(Actor* a, Actor* b) {
    return 0;
}
f32 Math_SinS(s16 angle) {
    return sinf(angle * (M_PI / 32768.0f));
}
f32 Math_CosS(s16 angle) {
    return cosf(angle * (M_PI / 32768.0f));
}
f32 Math_FAtan2F(f32 y, f32 x) {
    return atan2f(y, x);
}
void Math_Vec3f_Copy(Vec3f* dest, Vec3f* src) {
    *dest = *src;
}
f32 Math3D_Vec3fDistSq(Vec3f* a, Vec3f* b) {
    return SQ(a->x - b->x) + SQ(a->y - b->y) + SQ(a->z - b->z);
}
f32 Math3D_DistPlaneToPos(f32 x, f32 y, f32 z, f32 d, Vec3f* p) {
    return x * p->x + y * p->y + z * p->z + d;
}
void Actor_UpdateBgCheckInfo(PlayState* play, Actor* actor, f32 h, f32 r, f32 c, s32 flags) {
    NEAR(h, play->sceneNum == SCENE_LON_LON_RANCH ? 19.0f : 40.0f);
    NEAR(r, 35.0f);
    NEAR(c, 100.0f);
    CHECK(flags == 29);
}
s32 BgCheck_EntityLineTest1(CollisionContext* context, Vec3f* a, Vec3f* b, Vec3f* hit, CollisionPoly** wall,
                            s32 checkWall, s32 checkFloor, s32 checkCeil, s32 oneFace, s32* bg) {
    *bg = BGCHECK_SCENE;
    *wall = wallPresent ? &wallPoly : NULL;
    *hit = *a;
    hit->z += obstacleDistance;
    return wallPresent;
}
f32 BgCheck_EntityRaycastFloor3(CollisionContext* context, CollisionPoly** floor, s32* bg, Vec3f* pos) {
    *bg = BGCHECK_SCENE;
    *floor = raycastCount == 0 ? &topPoly : &behindPoly;
    return raycastCount++ == 0 ? obstacleTop : obstacleBehind;
}
DynaPolyActor* DynaPoly_GetActor(CollisionContext* context, s32 bg) {
    return NULL;
}
u32 SurfaceType_IsHorseBlocked(CollisionContext* context, CollisionPoly* poly, s32 bg) {
    return poly == blockedPoly;
}
u32 SurfaceType_GetFloorType(CollisionContext* context, CollisionPoly* poly, s32 bg) {
    return poly == specialFloorPoly ? 7 : 0;
}

f32 Rand_ZeroOne(void) {
    return 0.0f;
}
void Skin_GetLimbPos(Skin* skin, s32 limb, Vec3f* offset, Vec3f* result) {
    limbCalls[limbCallCount++] = limb;
    SkinMatrix_Vec3fMtxFMultXYZ(&skin->mtx, offset, result);
}
s32 CollisionCheck_SetOC(PlayState* play, CollisionCheckContext* context, Collider* collider) {
    return 0;
}
s32 CollisionCheck_SetAC(PlayState* play, CollisionCheckContext* context, Collider* collider) {
    return 0;
}

static void test_drawn_vertex_attachment(void) {
    static PlayState play;
    EnHorse horse = { 0 };
    SkinLimbVtx limbs[46] = { 0 };
    Vtx vertices[2][121] = { 0 };
    horse.type = 2;
    horse.actor.world.pos = (Vec3f){ 20.0f, 40.0f, 60.0f };
    horse.skin.vtxTable = limbs;
    horse.skin.limbCount = 46;
    horse.skin.mtx.xx = horse.skin.mtx.yy = horse.skin.mtx.zz = 0.00648f;
    horse.skin.mtx.xw = 20.0f;
    horse.skin.mtx.yw = 40.0f;
    horse.skin.mtx.zw = 60.0f;
    horse.skin.mtx.ww = 1.0f;
    limbs[5].buf[0] = vertices[0];
    limbs[5].buf[1] = vertices[1];
    vertices[0][120].n.ob[0] = 100;
    vertices[0][120].n.ob[1] = 200;
    vertices[0][120].n.ob[2] = 300;
    vertices[1][120].n.ob[0] = 400;
    vertices[1][120].n.ob[1] = 500;
    vertices[1][120].n.ob[2] = 600;
    for (int drawn = 0; drawn < 2; drawn++) {
        limbs[5].index = drawn ^ 1;
        limbCallCount = 0;
        EnHorse_PostDraw(&horse.actor, &play, &horse.skin);
        NEAR(horse.riderPos.x, vertices[drawn][120].n.ob[0] * 0.00648f);
        NEAR(horse.riderPos.y, vertices[drawn][120].n.ob[1] * 0.00648f + 13.0f);
        NEAR(horse.riderPos.z, vertices[drawn][120].n.ob[2] * 0.00648f);
        CHECK(limbCalls[0] == 13);
    }
    horse.type = HORSE_EPONA;
    limbCallCount = 0;
    EnHorse_PostDraw(&horse.actor, &play, &horse.skin);
    CHECK(limbCalls[0] == 30);
    NEAR(horse.riderPos.x, 600.0f * 0.00648f);
    NEAR(horse.riderPos.y, -1670.0f * 0.00648f);
    puts("PASS: drawn young body vertex follows both skin buffers; adult saddle anchor preserved");
}

static void test_jump_motion_and_landing(void) {
    static PlayState play;
    for (int type = 0; type <= 2; type++) {
        for (int high = 0; high <= 1; high++) {
            EnHorse horse = { 0 };
            horse.type = type;
            horse.actor.scale.y = type == 2 ? 0.00648f : 0.01f;
            horse.actor.world.pos.y = 30.0f;
            horse.actor.floorHeight = 30.0f;
            horse.skin.skelAnime.jointTable = &rootJoint;
            rootJoint.y = 1000;
            horse.riderPos.y = 70.0f;
            if (high)
                EnHorse_StartHighJump(&horse, &play);
            else
                EnHorse_StartLowJump(&horse, &play);
            CHECK(horse.animationIdx == (high ? ENHORSE_ANIM_HIGH_JUMP : ENHORSE_ANIM_LOW_JUMP));
            CHECK(selectedAnimation == sAnimationHeaders[type][horse.animationIdx]);
            NEAR(horse.riderPos.y, 70.0f - 1000 * horse.actor.scale.y);
            NEAR(horse.actor.gravity, 0.0f);
            if (high)
                EnHorse_HighJump(&horse, &play);
            else
                EnHorse_LowJump(&horse, &play);
            NEAR(horse.actor.world.pos.y, 40.0f); /* Native physical root scale remains .01. */
            CHECK(horse.stateFlags & ENHORSE_JUMPING);
            horse.skin.skelAnime.curFrame = high ? 24.0f : 18.0f;
            horse.actor.world.pos.y = 200.0f;
            if (high)
                EnHorse_HighJump(&horse, &play);
            else
                EnHorse_LowJump(&horse, &play);
            CHECK(horse.stateFlags & ENHORSE_JUMPING);
            NEAR(horse.actor.gravity, -3.5f);
            CHECK(horse.actor.velocity.y < 0.0f);
            horse.actor.world.pos.y = 40.0f;
            if (high)
                EnHorse_HighJump(&horse, &play);
            else
                EnHorse_LowJump(&horse, &play);
            CHECK(!(horse.stateFlags & ENHORSE_JUMPING));
            CHECK(horse.action == ENHORSE_ACT_MOUNTED_GALLOP);
            CHECK(horse.postDrawFunc == NULL);
            CHECK(selectedAnimation == sAnimationHeaders[type][ENHORSE_ANIM_GALLOP]);
            NEAR(horse.actor.world.pos.y, 30.0f);
            NEAR(horse.riderPos.y, 70.0f);
        }
    }
    puts("PASS: young/adult/Ingo low and high jumps retain physical clearance and native landing");
}

static void test_variant_initialization(void) {
    static PlayState play;
    play.sceneNum = SCENE_HYRULE_FIELD;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    const s16 params[] = { 0, 1, 2, 9, 3, 6, 7, 8 };
    for (int i = 0; i < ARRAY_COUNT(params); i++) {
        EnHorse horse = { 0 };
        horse.actor.params = ENHORSE_YOUNG_PARAM | params[i];
        killed = 0;
        EnHorse_Init(&horse.actor, &play);
        CHECK(!killed);
        CHECK(horse.type == HORSE_YOUNG_EPONA);
        CHECK(horse.actor.params == (params[i] <= 2 || params[i] == 9 ? params[i] : 0));
        CHECK(selectedSkeleton == (SkeletonHeader*)gChildEponaSkel);
        CHECK(horse.actor.objBankIndex == 1);
        CHECK(horse.action == (params[i] == 2 ? ENHORSE_ACT_INACTIVE : ENHORSE_ACT_IDLE));
        NEAR(horse.actor.scale.y, 0.00648f);
        CHECK(horse.boostSpeed == 14);
    }
    EnHorse missingObjectHorse = { 0 };
    missingObjectHorse.actor.params = ENHORSE_YOUNG_PARAM;
    childObjectMissing = 1;
    spawnedChildObject = 0;
    EnHorse_Init(&missingObjectHorse.actor, &play);
    CHECK(spawnedChildObject == 1);
    CHECK(missingObjectHorse.actor.objBankIndex == 2);
    childObjectMissing = 0;
    EnHorse horse = { 0 };
    horse.actor.params = -1;
    EnHorse_Init(&horse.actor, &play);
    CHECK(horse.type == HORSE_HNI);
    CHECK(horse.actor.params == 1);
    CHECK(selectedSkeleton == (SkeletonHeader*)gHorseIngoSkel);
    horse = (EnHorse){ 0 };
    horse.actor.params = ENHORSE_YOUNG_PARAM;
    canUseYoung = 0;
    killed = 0;
    selectedSkeleton = NULL;
    EnHorse_Init(&horse.actor, &play);
    CHECK(killed == 1);
    CHECK(selectedSkeleton == NULL);
    canUseYoung = 1;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    killed = 0;
    horse.actor.params = ENHORSE_YOUNG_PARAM;
    EnHorse_Init(&horse.actor, &play);
    CHECK(killed == 1);
    puts("PASS: third variant is child-only, gated, and preserves mounted-entry parameters without adult minigames");
}

static void test_native_fence_collision_decisions(void) {
    static PlayState play;
    play.sceneNum = SCENE_HYRULE_FIELD;
    DREG(4) = 70;
    for (int type = 0; type <= 2; type++) {
        for (int scenario = 0; scenario < 12; scenario++) {
            EnHorse horse = { 0 };
            horse.type = type;
            horse.playerControlled = true;
            horse.actor.speedXZ = scenario >= 2 ? 13.0f : 10.0f;
            horse.action = ENHORSE_ACT_MOUNTED_GALLOP;
            horse.actor.floorPoly = &groundPoly;
            groundPoly.normal = (Vec3s){ 0, 32767, 0 };
            topPoly.normal = behindPoly.normal = groundPoly.normal;
            wallPoly.normal = (Vec3s){ 0, 0, -32767 };
            obstacleTop = scenario < 2 ? 30.0f : 60.0f;
            obstacleBehind = 0.0f;
            obstacleDistance = scenario < 2 ? 100.0f : 180.0f;
            raycastCount = 0;
            blockedPoly = specialFloorPoly = NULL;
            wallPresent = 1;
            switch (scenario) {
                case 1:
                    obstacleDistance = 50.0f;
                    break;
                case 3:
                    blockedPoly = &wallPoly;
                    break;
                case 4:
                    blockedPoly = &topPoly;
                    break;
                case 5:
                    blockedPoly = &behindPoly;
                    break;
                case 6:
                    behindPoly.normal.y = 20000;
                    break;
                case 7:
                    obstacleBehind = -71.0f;
                    break;
                case 8:
                    specialFloorPoly = &behindPoly;
                    break;
                case 9:
                    wallPresent = 0;
                    break;
                case 10:
                    horse.stateFlags |= ENHORSE_CANT_JUMP;
                    break;
                case 11:
                    obstacleDistance = 100.0f;
                    break;
            }
            EnHorse_UpdateBgCheckInfo(&horse, &play);
            if (scenario == 0)
                CHECK(horse.postDrawFunc == EnHorse_LowJumpInit);
            else if (scenario == 2)
                CHECK(horse.postDrawFunc == EnHorse_HighJumpInit);
            else
                CHECK(horse.postDrawFunc == NULL);
            if ((scenario >= 4 && scenario <= 8) || scenario == 11) {
                CHECK(horse.action == ENHORSE_ACT_STOPPING);
                CHECK(horse.stateFlags & ENHORSE_FORCE_REVERSING);
            }
            if (scenario == 1)
                CHECK(horse.stateFlags & ENHORSE_FORCE_REVERSING);
            if (scenario == 3)
                CHECK(raycastCount == 0);
        }
    }
    puts("PASS: all variants share native fence thresholds, horse-blocked walls/floors, slope/drop and braking rules");
}

static void test_disabled_young_cannot_mount(void) {
    static PlayState play;
    Player player = { 0 };
    EnHorse horse = { 0 };
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    horse.type = HORSE_YOUNG_EPONA;
    horse.action = ENHORSE_ACT_IDLE;
    horse.animationIdx = ENHORSE_ANIM_IDLE;
    horse.playerDir = PLAYER_DIR_SIDE_L;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    canUseYoung = true;
    CHECK(EnHorse_GetMountSide(&horse, &play) == -1);
    CHECK(!EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    canUseYoung = false;
    CHECK(EnHorse_GetMountSide(&horse, &play) == 0);
    CHECK(EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    player.rideActor = &horse.actor;
    CHECK(!EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    gSaveContext.linkAge = LINK_AGE_ADULT;
    CHECK(!EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    player.rideActor = NULL;
    player.actor.parent = &horse.actor;
    CHECK(!EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    player.actor.parent = NULL;
    horse.playerControlled = true;
    CHECK(!EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    horse.playerControlled = false;
    CHECK(EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    horse.type = HORSE_EPONA;
    CHECK(EnHorse_GetMountSide(&horse, &play) == -1);
    CHECK(!EnHorse_ShouldRemoveYoungEpona(&horse, &play));
    canUseYoung = true;
    puts("PASS: disabled young cannot mount; removal waits for mount/dismount/age detach, adult path preserved");
}

int main(void) {
    test_drawn_vertex_attachment();
    test_jump_motion_and_landing();
    test_variant_initialization();
    test_native_fence_collision_decisions();
    test_disabled_young_cannot_mount();
    return 0;
}
