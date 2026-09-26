/* The runner inserts unchanged production functions and tables. Only animation,
 * camera, input, and general player-action engine boundaries are replaced. */
#include <math.h>
#include <string.h>
#include "tests/test_require.h"
#include "global.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_horse_link_child/rideable_young_epona.h"
#include "young_epona.h"

/* PRODUCTION_TYPES */
/* PRODUCTION_TABLES */

SaveContext gSaveContext;
static GameInfo registers;
GameInfo* gGameInfo = &registers;
static PlayState play;
static Player player;
static EnHorse horse;
static Input input;
static Input* sControlInput = &input;
static Camera camera;
static s32 sUpperBodyIsBusy;
static u8 sUpperBodyLimbCopyMap[PLAYER_LIMB_MAX];
static int allowed, animationDone, horseCameraCalls, normalCameraCalls, idleSetups, lensDisabled;
static int ownership, cycleYaw, sitSounds, floorBlocked[2], wallBlocked[2];
static f32 floorHeight[2];

void Player_Action_8084CC98(Player*, PlayState*);
void Player_Action_8084D3E4(Player*, PlayState*);
void Player_Action_WaitForPutAway(Player*, PlayState*);
void Player_Action_Idle(Player* p, PlayState* state) {
}
s32 Player_SetupAction(PlayState* state, Player* p, PlayerActionFunc action, s32 flags) {
    p->actionFunc = action;
    p->av1.actionVar1 = p->av2.actionVar2 = 0;
    if (action == Player_Action_Idle)
        idleSetups++;
    return true;
}
LinkAnimationHeader* Player_GetIdleAnim(Player* p) {
    return (LinkAnimationHeader*)gPlayerAnim_link_normal_wait;
}
s32 Player_PutAwayHeldItem(PlayState* state, Player* p) {
    return false;
}
void Player_AnimPlayOnce(PlayState* state, Player* p, LinkAnimationHeader* animation) {
    p->skelAnime.animation = animation;
    p->skelAnime.curFrame = 0;
}
void Player_StartAnimMovement(PlayState* state, Player* p, s32 flags) {
    p->skelAnime.movementFlags = flags;
}
void func_80832224(Player* p) {
}
void Actor_DisableLens(PlayState* state) {
    lensDisabled++;
}
s16 MasterCycle_RideYaw(Actor* mount) {
    return cycleYaw ? 1234 : mount->shape.rot.y;
}
s32 Horse_CanUseYoungEpona(void) {
    return allowed;
}
s32 Flags_GetEventChkInf(s32 flag) {
    REQUIRE(flag == EVENTCHKINF_EPONA_OBTAINED);
    return ownership;
}
f32 Math_CosS(s16 angle) {
    return cosf((f32)angle * (3.14159265358979323846f / 32768));
}
f32 Math_SinS(s16 angle) {
    return sinf((f32)angle * (3.14159265358979323846f / 32768));
}
s32 LinkAnimation_Update(PlayState* state, SkelAnime* animation) {
    return animationDone;
}
s32 LinkAnimation_OnFrame(SkelAnime* animation, f32 frame) {
    return animation->curFrame == frame;
}
void Actor_RequestHorseCameraSetting(PlayState* state, Player* p) {
    horseCameraCalls++;
}
Camera* Play_GetCamera(PlayState* state, s16 id) {
    REQUIRE(id == CAM_ID_MAIN);
    return &camera;
}
s32 Camera_RequestSetting(Camera* cam, s16 setting) {
    REQUIRE(cam == &camera && setting == CAM_SET_NORMAL0);
    normalCameraCalls++;
    return 0;
}
void Player_PlaySfx(Actor* actor, u16 id) {
    if (id == NA_SE_PL_SIT_ON_HORSE)
        sitSounds++;
}
void Player_PlayVoiceSfx(Player* p, u16 id) {
}
void Player_ProcessAnimSfxList(Player* p, AnimSfxEntry* entries) {
}
f32 Rand_ZeroOne(void) {
    return 0.5f;
}
void Animation_SetMorph(PlayState* state, SkelAnime* animation, f32 frames) {
}
void func_80834644(PlayState* state, Player* p) {
}
s32 Player_IsTalking(PlayState* state) {
    return false;
}
void LinkAnimation_AnimateFrame(PlayState* state, SkelAnime* animation) {
}
void AnimationContext_SetCopyAll(PlayState* state, s32 count, Vec3s* dst, Vec3s* src) {
}
void AnimationContext_SetCopyTrue(PlayState* state, s32 count, Vec3s* dst, Vec3s* src, u8* map) {
}
s32 Player_UpdateUpperBody(Player* p, PlayState* state) {
    return false;
}
s32 Player_ActionHandler_Talk(Player* p, PlayState* state) {
    return false;
}
s32 Player_ActionHandler_Roll(Player* p, PlayState* state) {
    return false;
}
s32 Player_ActionHandler_13(Player* p, PlayState* state) {
    return false;
}
void LinkAnimation_PlayOnce(PlayState* state, SkelAnime* animation, LinkAnimationHeader* asset) {
}
s32 func_8083AD4C(PlayState* state, Player* p) {
    return false;
}
s32 Player_IsZTargeting(Player* p) {
    return false;
}
s16 func_8084ABD8(PlayState* state, Player* p, s32 arg2, s16 arg3) {
    return 0;
}
s32 func_8002DD78(Player* p) {
    return false;
}
s16 func_8083DB98(Player* p, s32 arg1) {
    return 0;
}
f32 func_8083973C(PlayState* state, Player* p, Vec3f* offset, Vec3f* destination) {
    int side = offset->x < 0;
    return floorBlocked[side] ? -1000 : floorHeight[side];
}
s32 Player_PosVsWallLineTest(PlayState* state, Player* p, Vec3f* offset, CollisionPoly** poly, s32* id, Vec3f* pos) {
    return wallBlocked[offset->x < 0];
}

/* PRODUCTION_PLAYER */

static void reset(void) {
    memset(&play, 0, sizeof(play));
    memset(&player, 0, sizeof(player));
    memset(&horse, 0, sizeof(horse));
    memset(&input, 0, sizeof(input));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    memset(&registers, 0, sizeof(registers));
    memset(floorBlocked, 0, sizeof(floorBlocked));
    memset(wallBlocked, 0, sizeof(wallBlocked));
    allowed = 1;
    animationDone = horseCameraCalls = normalCameraCalls = idleSetups = lensDisabled = 0;
    ownership = cycleYaw = sitSounds = sUpperBodyIsBusy = 0;
    play.sceneNum = SCENE_HYRULE_FIELD;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.horseData = (HorseData){ SCENE_LAKE_HYLIA, { -900, 25, 700 }, -123 };
    horse.type = HORSE_YOUNG_EPONA;
    horse.actor.id = ACTOR_EN_HORSE;
    horse.actor.world.pos = (Vec3f){ 100, 50, 200 };
    horse.riderPos = (Vec3f){ 1, 60, 3 };
    horse.action = ENHORSE_ACT_MOUNTED_IDLE;
    floorHeight[0] = floorHeight[1] = 50;
    player.rideActor = &horse.actor;
    player.actor.world.pos.y = 45;
    player.mountSide = -1;
    input.press.button = BTN_A;
}

static void mount_case(void) {
    const struct {
        s32 type;
        s16 side, yaw;
        f32 x, z;
        const char* animation;
    } cases[] = {
        { HORSE_YOUNG_EPONA, -1, 0, 123.718237f, 205.3294117f, gYoungEponaMountLeftAnim },
        { HORSE_YOUNG_EPONA, 1, 0, 79, 204.9800001f, gYoungEponaMountRightAnim },
        { HORSE_YOUNG_EPONA, -1, 0x4000, 103.3294117f, 180.281763f, gYoungEponaMountLeftAnim },
        { HORSE_EPONA, -1, 0, 136.17f, 209.61f, gPlayerAnim_link_uma_left_up },
        { HORSE_HNI, 1, 0, 66.84f, 210.91f, gPlayerAnim_link_uma_right_up },
    };
    for (unsigned i = 0; i < ARRAY_COUNT(cases); ++i) {
        reset();
        horse.type = cases[i].type;
        horse.actor.shape.rot.y = cases[i].yaw;
        player.mountSide = cases[i].side;
        cycleYaw = horse.type == HORSE_HNI;
        int result = Player_ActionHandler_3(&player, &play);
        REQUIRE(result == 1);
        REQUIRE(fabsf(player.actor.world.pos.x - cases[i].x) < 0.001f);
        REQUIRE(fabsf(player.actor.world.pos.z - cases[i].z) < 0.001f);
        REQUIRE(strcmp((char*)player.skelAnime.animation, cases[i].animation) == 0);
        REQUIRE(player.unk_878 == 5 && player.skelAnime.movementFlags == 0x9B);
        REQUIRE(player.actor.parent == &horse.actor && horse.actor.child == &player.actor);
        REQUIRE(player.stateFlags1 & PLAYER_STATE1_ON_HORSE);
        REQUIRE(player.actionFunc == Player_Action_WaitForPutAway && lensDisabled == 1);
        Player_Action_WaitForPutAway(&player, &play);
        REQUIRE(player.actionFunc == Player_Action_8084CC98);
        REQUIRE(player.skelAnime.movementFlags == 0x9B);
        REQUIRE(player.yaw == (cycleYaw ? 1234 : cases[i].yaw));
    }
    puts("PASS: young/adult left/right mount clips, rotated MM offsets, rider action and Master Cycle yaw");
}

static void gate_case(void) {
    reset();
    allowed = 0;
    int result = Player_ActionHandler_3(&player, &play);
    REQUIRE(result == 0 && player.actor.parent == NULL && player.skelAnime.animation == NULL);
    REQUIRE(!(player.stateFlags1 & PLAYER_STATE1_ON_HORSE));
    horse.type = HORSE_EPONA;
    result = Player_ActionHandler_3(&player, &play);
    REQUIRE(result == 1);
    reset();
    input.press.button = 0;
    result = Player_ActionHandler_3(&player, &play);
    REQUIRE(result == 0);
    reset();
    player.rideActor = NULL;
    result = Player_ActionHandler_3(&player, &play);
    REQUIRE(result == 0);
    puts("PASS: denied young access cannot load mount assets; native mount and input guards preserved");
}

static void camera_case(void) {
    for (s16 side = -1; side <= 1; side += 2) {
        reset();
        player.mountSide = side;
        animationDone = 1;
        Player_Action_8084CC98(&player, &play);
        REQUIRE(horseCameraCalls == 1 && sitSounds == 1 && player.av2.actionVar2 == 99);
        REQUIRE(player.skelAnime.animation == (void*)gPlayerAnim_link_uma_wait_1);
        reset();
        horse.type = HORSE_EPONA;
        player.mountSide = side;
        player.skelAnime.curFrame = side < 0 ? 58 : 42;
        Player_Action_8084CC98(&player, &play);
        REQUIRE(horseCameraCalls == 1 && sitSounds == 1);
        animationDone = 1;
        Player_Action_8084CC98(&player, &play);
        REQUIRE(horseCameraCalls == 1 && sitSounds == 1);
    }
    reset();
    player.av2.actionVar2 = horse.animationIdx = ENHORSE_ANIM_WALK;
    horse.curFrame = 12;
    player.csAction = 1;
    Player_Action_8084CC98(&player, &play);
    REQUIRE(player.actor.world.pos.x == 101 && player.actor.world.pos.y == 83 && player.actor.world.pos.z == 203);
    REQUIRE(player.skelAnime.curFrame == 12 && horseCameraCalls == 1);
    puts("PASS: short child mount completion, native adult camera cues and shared seated animation positioning");
}

static void save_case(void) {
    for (int obtained = 0; obtained <= 1; ++obtained) {
        reset();
        HorseData adultBefore = gSaveContext.horseData;
        ownership = obtained;
        horse.actor.shape.rot.y = 456;
        player.stateFlags1 = PLAYER_STATE1_ON_HORSE;
        player.actor.parent = &horse.actor;
        AREG(6) = 1;
        animationDone = 1;
        Player_Action_8084D3E4(&player, &play);
        REQUIRE(memcmp(&adultBefore, &gSaveContext.horseData, sizeof(adultBefore)) == 0);
        REQUIRE(gSaveContext.ship.youngHorseDataValid);
        REQUIRE(gSaveContext.ship.youngHorseData.scene == SCENE_HYRULE_FIELD);
        REQUIRE(gSaveContext.ship.youngHorseData.pos.x == 100 && gSaveContext.ship.youngHorseData.pos.y == 50);
        REQUIRE(gSaveContext.ship.youngHorseData.pos.z == 200 && gSaveContext.ship.youngHorseData.angle == 456);
        REQUIRE(!(player.stateFlags1 & PLAYER_STATE1_ON_HORSE) && player.actor.parent == NULL && AREG(6) == 0);
        REQUIRE(player.actionFunc == Player_Action_Idle && idleSetups == 1);
    }
    reset();
    horse.type = HORSE_EPONA;
    ownership = 1;
    animationDone = 1;
    Player_Action_8084D3E4(&player, &play);
    REQUIRE(gSaveContext.horseData.pos.x == 100 && !gSaveContext.ship.youngHorseDataValid);
    reset();
    horse.type = HORSE_EPONA;
    HorseData adultBefore = gSaveContext.horseData;
    animationDone = 1;
    Player_Action_8084D3E4(&player, &play);
    REQUIRE(memcmp(&adultBefore, &gSaveContext.horseData, sizeof(adultBefore)) == 0);
    puts("PASS: real child dismount save is isolated from adult location and native adult ownership gate");
}

static void dismount_case(void) {
    reset();
    horse.actor.child = &player.actor;
    floorBlocked[0] = 1;
    int result = func_8084C9BC(&player, &play);
    REQUIRE(result == 1 && player.mountSide == 1 && horse.actor.child == NULL);
    REQUIRE(player.actionFunc == Player_Action_8084D3E4);
    REQUIRE(player.skelAnime.animation == (void*)gPlayerAnim_link_uma_right_down);
    reset();
    wallBlocked[0] = wallBlocked[1] = 1;
    result = func_8084C9BC(&player, &play);
    REQUIRE(result == 0 && player.actionFunc == NULL);
    reset();
    floorHeight[0] = 75;
    floorHeight[1] = 25;
    result = func_8084C9BC(&player, &play);
    REQUIRE(result == 0);
    puts("PASS: native young dismount side fallback and wall/height rejection");
}

static void age_case(void) {
#ifdef HAVE_YOUNG_EPONA_AGE_DETACH
    reset();
    gSaveContext.linkAge = LINK_AGE_ADULT;
    player.stateFlags1 = PLAYER_STATE1_ON_HORSE;
    player.stateFlags2 = PLAYER_STATE2_DISABLE_ROTATION_ALWAYS;
    player.actor.parent = &horse.actor;
    horse.actor.child = &player.actor;
    AREG(6) = 1;
    HorseData adultBefore = gSaveContext.horseData;
    Player_DetachYoungEponaOnAgeChange(&player, &play);
    REQUIRE(player.rideActor == NULL && player.actor.parent == NULL && horse.actor.child == NULL);
    REQUIRE(!(player.stateFlags1 & PLAYER_STATE1_ON_HORSE));
    REQUIRE(!(player.stateFlags2 & PLAYER_STATE2_DISABLE_ROTATION_ALWAYS));
    REQUIRE(player.actionFunc == Player_Action_Idle && normalCameraCalls == 1 && AREG(6) == 0);
    REQUIRE(memcmp(&adultBefore, &gSaveContext.horseData, sizeof(adultBefore)) == 0);
    reset();
    player.stateFlags1 = PLAYER_STATE1_ON_HORSE;
    Player_DetachYoungEponaOnAgeChange(&player, &play);
    REQUIRE(player.rideActor == &horse.actor && (player.stateFlags1 & PLAYER_STATE1_ON_HORSE));
    gSaveContext.linkAge = LINK_AGE_ADULT;
    horse.type = HORSE_EPONA;
    Player_DetachYoungEponaOnAgeChange(&player, &play);
    REQUIRE(player.rideActor == &horse.actor && (player.stateFlags1 & PLAYER_STATE1_ON_HORSE));
    puts("PASS: live adult age change detaches young mount safely and preserves native mounts");
#else
    REQUIRE(!"production young age-change detach is absent");
#endif
}

int main(int argc, char** argv) {
    const struct {
        const char* name;
        void (*test)(void);
    } cases[] = {
        { "mount", mount_case }, { "gate", gate_case },         { "camera", camera_case },
        { "save", save_case },   { "dismount", dismount_case }, { "age", age_case },
    };
    for (unsigned i = 0; i < ARRAY_COUNT(cases); ++i)
        if (argc < 2 || strcmp(argv[1], "all") == 0 || strcmp(argv[1], cases[i].name) == 0)
            cases[i].test();
    return 0;
}
