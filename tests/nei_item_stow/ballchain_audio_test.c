// Real item handler + input + equip/unequip SFX helpers. Only engine boundaries
// (audio output, scene collisions/effects, camera, CVars) are replaced here.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "objects/gameplay_keep/gameplay_keep.h"
#include "z64.h"

// item_ballchain.c normally follows this definition in the z_player.c unity.
int Player_IsZTargeting(Player *player);
#include "mods/items/helpers/equip_helper.c"
#include "mods/items/logic/item_ballchain.c"

SaveContext gSaveContext;
CustomItemState gCustomItemState;
f32 gSfxDefaultFreqAndVolScale = 1.0f;
s8 gSfxDefaultReverb;

static Player player;
static PlayState play;
static Vec3s joints[PLAYER_LIMB_MAX];
static u16 sounds[64];
static size_t soundCount;
static size_t stoppedCount;
static int cameraExits;
static int deletedTrails;
static int registeredColliders;
static int dpadEnabled;
static int wallHit;
static f32 floorHeight;
static EffectBlure trail;

static void ResetFixture(int slot) {
  memset(&player, 0, sizeof(player));
  memset(&play, 0, sizeof(play));
  memset(&gSaveContext, 0, sizeof(gSaveContext));
  memset(&gCustomItemState, 0, sizeof(gCustomItemState));
  memset(&sEquipCache, 0, sizeof(sEquipCache));
  memset(joints, 0, sizeof(joints));
  memset(gSaveContext.equips.buttonItems, ITEM_NONE,
         sizeof(gSaveContext.equips.buttonItems));
  gSaveContext.equips.buttonItems[slot] = ITEM_BALL_AND_CHAIN;
  dpadEnabled = 1;
  play.gameplayFrames = 1;
  player.skelAnime.jointTable = joints;
  sBallChainColInitialized = 0;
  sBallChainPrevInvinc = 0;
  ItemEquip_ResetUnequipSound(&play, &player, PLAYER_IA_BALL_AND_CHAIN);
  Player_InitBallAndChainIA(&play, &player);
  soundCount = stoppedCount = 0;
  cameraExits = deletedTrails = registeredColliders = 0;
  wallHit = 0;
  floorHeight = BGCHECK_Y_MIN;
}

static void SeedUnrelatedState(void) {
  // These fields are shared with other items; an inactive Ball and Chain must
  // not clear them or take ownership of Link's roll/animation pose.
  gCustomItemState.timer1 = 27;
  gCustomItemState.timer2 = 4;
  gCustomItemState.somariaCooldown = 19;
  gCustomItemState.globalCooldownTimer = 41;
  gCustomItemState.sharedProjectilePos = (Vec3f){17.0f, 23.0f, 31.0f};
  player.upperLimbRot = (Vec3s){101, 202, 303};
  player.skelAnime.playSpeed = 1.75f;
  player.linearVelocity = 6.0f;
  player.actor.speedXZ = 6.0f;
  joints[PLAYER_LIMB_R_SHOULDER] = (Vec3s){400, 500, 600};
}

static void AssertInactivePressDoesNothing(u16 button, int repeats) {
  Player beforePlayer;
  CustomItemState beforeState;
  Vec3s beforeJoints[PLAYER_LIMB_MAX];
  ItemInputState input;
  play.state.input[0].press.button = button;
  play.state.input[0].cur.button = button;
  ItemInput_Update(&input, ITEM_BALL_AND_CHAIN, &player, &play);
  assert(input.wasEquipped && input.otherButtonPressed && !input.isPressed);
  memcpy(&beforePlayer, &player, sizeof(player));
  memcpy(&beforeState, &gCustomItemState, sizeof(gCustomItemState));
  memcpy(beforeJoints, joints, sizeof(joints));
  for (int i = 0; i < repeats; ++i) {
    Handle_BallAndChain(&player, &play);
    assert(soundCount == 0 && stoppedCount == 0);
    assert(memcmp(&beforePlayer, &player, sizeof(player)) == 0);
    assert(memcmp(&beforeState, &gCustomItemState, sizeof(gCustomItemState)) ==
           0);
    assert(memcmp(beforeJoints, joints, sizeof(joints)) == 0);
  }
  assert(cameraExits == 0 && deletedTrails == 0 && registeredColliders == 0);
}

static void TestAssignedOnlyButtons(void) {
  const u16 buttons[] = {BTN_A,     BTN_B,     BTN_R,      BTN_START,
                         BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, BTN_DUP,
                         BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT};
  for (int slot = 0; slot < 8; ++slot) {
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
      if (buttons[i] == sButtonMasks[slot])
        continue;
      ResetFixture(slot);
      SeedUnrelatedState();
      AssertInactivePressDoesNothing(buttons[i], 1);
    }
  }

  // CustomItems_Update calls once for every matching slot, even duplicates.
  ResetFixture(0);
  memset(gSaveContext.equips.buttonItems, ITEM_BALL_AND_CHAIN,
         sizeof(gSaveContext.equips.buttonItems));
  SeedUnrelatedState();
  AssertInactivePressDoesNothing(BTN_A, 8);
}

static void TestActiveCancelOnce(void) {
  for (int state = BALLCHAIN_STATE_EQUIP; state <= BALLCHAIN_STATE_THROWN;
       ++state) {
    ResetFixture(0);
    memset(gSaveContext.equips.buttonItems, ITEM_BALL_AND_CHAIN,
           sizeof(gSaveContext.equips.buttonItems));
    bcActive = 1;
    bcState = state;
    bcCharge = 20;
    bcFirstPerson = 1;
    bcTrailActive = 1;
    bcTrailIndex = 2;
    bcCollider.base.atFlags = AT_ON | AT_HIT;
    player.skelAnime.playSpeed = 0.0f;
    player.upperLimbRot = (Vec3s){10, 20, 30};
    play.state.input[0].press.button = BTN_A;
    play.state.input[0].cur.button = BTN_A;
    for (int i = 0; i < 8; ++i)
      Handle_BallAndChain(&player, &play);
    assert(soundCount == 1 && sounds[0] == NA_SE_PL_CHANGE_ARMS);
    assert(stoppedCount == 2 && cameraExits == 1 && deletedTrails == 1);
    assert(!bcActive && bcState == BALLCHAIN_STATE_INACTIVE && bcCharge == 0);
    assert(!bcFirstPerson && !bcTrailActive);
    assert(!(bcCollider.base.atFlags & (AT_ON | AT_HIT)));
    assert(player.skelAnime.playSpeed == 1.0f);
    assert(player.upperLimbRot.x == 0 && player.upperLimbRot.y == 0 &&
           player.upperLimbRot.z == 0);
  }
}

static void TestOwnButtonStillStarts(void) {
  for (int slot = 0; slot < 8; ++slot) {
    ResetFixture(slot);
    play.state.input[0].press.button = sButtonMasks[slot];
    play.state.input[0].cur.button = sButtonMasks[slot];
    Handle_BallAndChain(&player, &play);
    assert(bcActive && bcState == BALLCHAIN_STATE_EQUIP);
    assert(soundCount == 1 && sounds[0] == NA_SE_PL_CHANGE_ARMS);
  }
}

static void TestActualImpactSounds(void) {
  // Exercise collision-hit handling through all active states. The actual
  // attack collider's pending AT_HIT produces one hammer hit, then clears.
  for (int state = BALLCHAIN_STATE_EQUIP; state <= BALLCHAIN_STATE_THROWN;
       ++state) {
    ResetFixture(1);
    bcActive = 1;
    bcState = state;
    bcPhase = BALLCHAIN_PHASE_REST;
    bcRestTimer = 5;
    bcCollider.base.atFlags |= AT_HIT;
    play.state.input[0].cur.button = BTN_CLEFT;
    Handle_BallAndChain(&player, &play);
    assert(soundCount >= 1 && sounds[0] == BALLCHAIN_SFX_HIT);
    assert(!(bcCollider.base.atFlags & AT_HIT));
    assert(bcActive && registeredColliders == 1);
  }

  // A falling thrown ball still sounds on its real floor-bounce condition.
  ResetFixture(1);
  bcActive = 1;
  bcState = BALLCHAIN_STATE_THROWN;
  bcPhase = BALLCHAIN_PHASE_FLY;
  bcBallPos = (Vec3f){0.0f, 12.0f, 0.0f};
  bcBallVel.y = -5.0f;
  floorHeight = 0.0f;
  Handle_BallAndChain(&player, &play);
  assert(soundCount == 1 && sounds[0] == BALLCHAIN_SFX_HIT);
  assert(bcBounces == 1 && bcBallVel.y > 0.0f);

  // A thrown ball still sounds and reflects on a real wall result.
  ResetFixture(1);
  bcActive = 1;
  bcState = BALLCHAIN_STATE_THROWN;
  bcPhase = BALLCHAIN_PHASE_FLY;
  bcBallPos.y = 100.0f;
  bcBallVel.x = 8.0f;
  wallHit = 1;
  Handle_BallAndChain(&player, &play);
  assert(soundCount == 1 && sounds[0] == BALLCHAIN_SFX_WALL_BOUNCE);
  assert(bcBallVel.x < 0.0f);
}

int main(void) {
  TestAssignedOnlyButtons();
  TestActiveCancelOnce();
  TestOwnButtonStillStarts();
  TestActualImpactSounds();
  puts("Ball and Chain: inactive slot/input, duplicate dispatch, active "
       "cancel/start, and impact SFX passed");
  return 0;
}

// Engine boundaries. No item/input/SFX helper is mocked.
int32_t CVarGetInteger(const char *name, int32_t fallback) {
  return strcmp(name, "gEnhancements.DpadEquips") == 0 ? dpadEnabled : fallback;
}
u8 Pacci_UltrahandModeActive(void) { return 0; }
u8 MasterCycle_IsRiding(void) { return 0; }
u8 Cryonis_ModeActive(void) { return 0; }
uint8_t Sw97_IsBowItem(uint16_t item) { return 0; }
uint8_t Sw97_EffectiveElement(uint8_t isSling) { return 0; }
int Player_IsZTargeting(Player *p) { return 0; }
s32 Player_InBlockingCsMode(PlayState *p, Player *link) { return 0; }
void Audio_PlaySoundGeneral(u16 id, Vec3f *pos, u8 token, f32 *freq,
                            f32 *volume, s8 *reverb) {
  assert(soundCount < sizeof(sounds) / sizeof(sounds[0]));
  sounds[soundCount++] = id;
}
void Audio_StopSfxById(u32 id) { ++stoppedCount; }
void Actor_PlaySfx_Flagged(Actor *actor, u16 id) {
  Audio_PlaySoundGeneral(id, &actor->world.pos, 4, NULL, NULL, NULL);
}
void ItemVoice_Play(Player *p, u16 adult, u16 child) {
  assert(0 && "unexpected launch voice");
}
void FirstPerson_Init(Player *p, PlayState *s) { assert(0); }
void FirstPerson_Update(Player *p, PlayState *s) { assert(0); }
void FirstPerson_Exit(Player *p, PlayState *s) { ++cameraExits; }
s16 FirstPerson_GetAimYaw(Player *p) {
  assert(0);
  return 0;
}
s16 FirstPerson_GetAimPitch(Player *p) {
  assert(0);
  return 0;
}
s32 Collider_InitCylinder(PlayState *p, ColliderCylinder *collider) {
  memset(collider, 0, sizeof(*collider));
  return 1;
}
s32 Collider_SetCylinder(PlayState *p, ColliderCylinder *collider, Actor *actor,
                         ColliderCylinderInit *source) {
  collider->base.actor = actor;
  return 1;
}
s32 CollisionCheck_SetAT(PlayState *p, CollisionCheckContext *context,
                         Collider *collider) {
  ++registeredColliders;
  return 1;
}
s32 CollisionCheck_SetOC(PlayState *p, CollisionCheckContext *context,
                         Collider *collider) {
  return 1;
}
void Effect_Add(PlayState *p, s32 *index, s32 type, u8 a, u8 b, void *init) {
  *index = 2;
}
void *Effect_GetByIndex(s32 index) { return &trail; }
void EffectBlure_AddVertex(EffectBlure *effect, Vec3f *a, Vec3f *b) {}
void Effect_Delete(PlayState *p, s32 index) { ++deletedTrails; }
f32 Math_SinS(s16 angle) {
  return sinf(angle * (3.14159265358979323846f / 32768.0f));
}
f32 Math_CosS(s16 angle) {
  return cosf(angle * (3.14159265358979323846f / 32768.0f));
}
f32 Math_Vec3f_DistXYZ(Vec3f *a, Vec3f *b) {
  return sqrtf(SQ(a->x - b->x) + SQ(a->y - b->y) + SQ(a->z - b->z));
}
s16 Math_Vec3f_Yaw(Vec3f *a, Vec3f *b) {
  return (s16)(atan2f(b->x - a->x, b->z - a->z) *
               (32768.0f / 3.14159265358979323846f));
}
s32 BgCheck_EntitySphVsWall1(CollisionContext *context, Vec3f *result,
                             Vec3f *next, Vec3f *prev, f32 radius,
                             CollisionPoly **poly, f32 height) {
  static CollisionPoly wall;
  wall.normal.x = 32767;
  *poly = wallHit ? &wall : NULL;
  return wallHit;
}
f32 BgCheck_EntityRaycastFloor5(PlayState *p, CollisionContext *context,
                                CollisionPoly **poly, s32 *bgId, Actor *actor,
                                Vec3f *pos) {
  return floorHeight;
}

// The focused fixtures have no destructible actors. Fail loudly if a test
// accidentally enters those unrelated gameplay paths.
EnItem00 *Item_DropCollectible(PlayState *p, Vec3f *pos, s16 params) {
  assert(0);
  return NULL;
}
s32 Flags_GetCollectible(PlayState *p, s32 flag) {
  assert(0);
  return 0;
}
void Sfx_PlaySfxCentered(u16 id) { assert(0); }
void Actor_Kill(Actor *actor) { assert(0); }
void EffectSsBomb2_SpawnLayered(PlayState *p, Vec3f *pos, Vec3f *vel,
                                Vec3f *accel, s16 scale, s16 step) {
  assert(0);
}
void SoundSource_PlaySfxAtFixedWorldPos(PlayState *p, Vec3f *pos, s32 duration,
                                        u16 id) {
  assert(0);
}
void EffectSsHahen_SpawnBurst(PlayState *p, Vec3f *pos, f32 burst, s16 unused,
                              s16 scale, s16 range, s16 count, s16 object,
                              s16 life, Gfx *dl) {
  assert(0);
}
void BgIceShelter_ShatterMelt(Actor *actor, PlayState *p) { assert(0); }
void BgJyaIronobj_DestroyInstantly(Actor *actor, PlayState *p) { assert(0); }
void BgIceTurara_Break(BgIceTurara *actor, PlayState *p, f32 a) { assert(0); }
void EnFz_SetupMelt(EnFz *actor) { assert(0); }
