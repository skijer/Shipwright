// Real object drawers and wrist helper; only resource/graphics/projectile
// boundaries are replaced. These matrix cases are not gameplay captures.
#define main NeiArticulatedRegressionMain
#include "articulated_runtime_test.cpp"
#undef main
#include "mods/items/logic/item_rod_common.h"
#include "soh/Enhancements/randomizer/NeiUsedMagicPresentation.h"
#include "variables.h"
#include <array>

extern "C" {
#include "mods/actors/somaria_cubes.h"
#include "mods/items/logic/item_cane_of_somaria.h"
}

namespace {
constexpr const char *kOriginalSpinner =
    "__OTR__objects/object_nei_spinner/n0b0_opaque_dl";
constexpr const char *kOriginalCane =
    "__OTR__objects/object_somaria/g_somaria_cane_dl";
std::vector<std::string> resourceQueries, resourceLoads, caneEffects;
u8 selectedCane = CANE_TYPE_SOMARIA;
u8 aimingCane = 0, selectedCaneSkill = CANE_SKILL_SOMARIA_BLOCK;
CaneSummonKind previewKind = CANE_SUMMON_STATUE;
Vec3f previewPosition{};
s16 previewYaw = 0;
u8 previewValid = 0;
bool localRodSets[3]{};
RodProjSet rodSets[3][ROD_MAX_PROJ_SETS]{};
struct ProjectileCall {
  int element;
  Vec3f position, velocity;
  float scale;
};
std::vector<ProjectileCall> projectileCalls;
std::vector<int> trailCalls;
std::vector<std::array<Vec3f, 6>> trailPositions;
std::string rodDrawOrder;

void resetWrappers(GraphicsContext &graphics, Gfx *opa, Gfx *xlu) {
  reset();
  resourceQueries.clear();
  resourceLoads.clear();
  caneEffects.clear();
  graphics.polyOpa.p = opa;
  graphics.polyXlu.p = xlu;
}

std::vector<uint32_t> commandColors(Gfx *begin, Gfx *end, unsigned opcode) {
  std::vector<uint32_t> colors;
  for (Gfx *command = begin; command != end; ++command) {
    if (((command->words.w0 >> 24) & 255) == opcode) {
      colors.push_back(static_cast<uint32_t>(command->words.w1));
    }
  }
  return colors;
}

void unchanged(const Player &player, const Player &before,
               const CustomItemState &state) {
  assert(std::memcmp(&player, &before, sizeof(player)) == 0);
  assert(std::memcmp(&gCustomItemState, &state, sizeof(state)) == 0);
  assert(matrices.empty());
}
} // namespace

extern "C" {
SaveContext gSaveContext{};
Gfx Cylinder_001_opaque_dl[1];
Gfx ice_rod_opaque_dl[1], ice_rod_transparent_dl[1];
Gfx Cylinder_002_opaque_dl[1], Cylinder_002_transparent_dl[1];
u8 FireRod_HasAnyActiveSet() { return localRodSets[0]; }
u8 IceRod_HasAnyActiveSet() { return localRodSets[1]; }
u8 LightRod_HasAnyActiveSet() { return localRodSets[2]; }
RodProjSet *FireRod_GetProjSets() { return rodSets[0]; }
RodProjSet *IceRod_GetProjSets() { return rodSets[1]; }
RodProjSet *LightRod_GetProjSets() { return rodSets[2]; }
void Gfx_SetupDL_25Xlu(GraphicsContext *) {}
void gSPSegment(void *, int, uintptr_t) {}
Gfx *Gfx_TwoTexScroll(GraphicsContext *, s32, u32, u32, s32, s32, s32, u32, u32,
                      s32, s32) {
  return nullptr;
}
s16 Camera_GetCamDirYaw(Camera *) { return 0; }
f32 Rand_ZeroOne() { return 0; }
void Matrix_ReplaceRotation(MtxF *) {}
void NeiUsedMagic_DrawProjectile(PlayState *, int element,
                                 const Vec3f *position, const Vec3f *velocity,
                                 float scale, unsigned) {
  projectileCalls.push_back({element, *position, *velocity, scale});
  rodDrawOrder += 'H';
}
void NeiUsedMagic_DrawTrail(PlayState *, int element, const Vec3f *points,
                            unsigned count, float) {
  assert(count == 6);
  trailCalls.push_back(element);
  rodDrawOrder += 'T';
  std::array<Vec3f, 6> copy;
  std::copy(points, points + 6, copy.begin());
  trailPositions.push_back(copy);
}
bool NeiHeld_DrawRod(PlayState *play, int element) {
  const char *paths[] = {NEI_HELD_PATH("fire_rod"), NEI_HELD_PATH("ice_rod"),
                         NEI_HELD_PATH("light_rod")};
  return NeiHeld_DrawModel(play, paths[element], nullptr);
}
u8 ResourceMgr_FileExists(const char *path) {
  resourceQueries.emplace_back(path);
  return available.contains(path);
}
Gfx *ResourceMgr_LoadGfxByName(const char *path) {
  assert(available.contains(path));
  resourceLoads.emplace_back(path);
  static Gfx resource[1];
  return resource;
}
u8 Cane_GetType() { return selectedCane; }
u8 Cane_IsAiming() { return aimingCane; }
u8 Cane_GetActiveSkill() { return selectedCaneSkill; }
void Pacci_UltrahandDrawVfx(PlayState *, Player *) {
  caneEffects.emplace_back("ultrahand");
}
void Pacci_FuseDrawPreview(PlayState *) { caneEffects.emplace_back("fuse"); }
void PacciFlipVfx_Draw(PlayState *, Player *) {
  caneEffects.emplace_back("flip");
}
void Trirod_DrawPreview(PlayState *, Player *) {
  caneEffects.emplace_back("trirod");
}
void CaneSummon_DrawPreview(PlayState *, CaneSummonKind kind, Vec3f *position,
                            s16 yaw, u8 valid) {
  caneEffects.emplace_back("summon");
  previewKind = kind;
  previewPosition = *position;
  previewYaw = yaw;
  previewValid = valid;
}
}

namespace {
void testMissingWrapperResources(Player &player, PlayState &play,
                                 GraphicsContext &graphics, Gfx *opa,
                                 Gfx *xlu) {
  // Run in a fresh process: each real wrapper caches its first legacy lookup.
  // Neither missing archive path may reach the crash-prone resource loader.
  resetWrappers(graphics, opa, xlu);
  gCustomItemState = {};
  gCustomItemState.spinnerActive = 1;
  Matrix_Translate(100, 200, 300, MTXMODE_NEW);
  const MtxF before = current;
  CustomItems_DrawSpinner(&player, &play);
  assert(drawn.empty() && nativeDraws == 0 && resourceLoads.empty());
  assert(resourceQueries == std::vector<std::string>{kOriginalSpinner});
  assert(std::memcmp(&current, &before, sizeof(current)) == 0);
  assert(matrices.empty());

  resetWrappers(graphics, opa, xlu);
  Matrix_Scale(.01f, .01f, .01f, MTXMODE_APPLY);
  ItemEquip_CaptureHandMatrix();
  gCustomItemState = {};
  gCustomItemState.somariaActive = 1;
  CustomItems_DrawCaneOfSomaria(&player, &play);
  assert(drawn.empty() && nativeDraws == 0 && resourceLoads.empty());
  assert(resourceQueries == std::vector<std::string>{kOriginalCane});
  assert(caneEffects ==
         std::vector<std::string>({"ultrahand", "fuse", "flip"}));
  assert(matrices.empty());
  std::cout << "PASS: real Spinner/Somaria wrappers safely decline absent "
               "replacement and legacy resources\n";
}

void testSpinnerWrapper(Player &player, PlayState &play,
                        GraphicsContext &graphics, Gfx *opa, Gfx *xlu) {
  resetWrappers(graphics, opa, xlu);
  available.insert(NEI_HELD_PATH("spinner"));
  available.insert(kOriginalSpinner);
  gCustomItemState = {};
  CustomItems_DrawSpinner(&player, &play);
  assert(drawn.empty() && nativeDraws == 0 && resourceQueries.empty());

  // Sample quadrant boundaries, signed binary-angle wrap, and whole turns at
  // the existing ride, attack and homing heights. Expected positions use an
  // independent 32-frame circle, so raw s16-as-radians fails this boundary.
  for (float height : {17.f, 27.f, 137.f}) {
    for (int frame : {0, 1, 7, 8, 15, 16, 23, 31, 32, 63, 1025}) {
      resetWrappers(graphics, opa, xlu);
      available.insert(NEI_HELD_PATH("spinner"));
      available.insert(kOriginalSpinner);
      player.actor.world.pos = {12, height, -8};
      play.gameplayFrames = frame;
      gCustomItemState = {};
      gCustomItemState.spinnerActive = 1;
      gCustomItemState.spinnerSpinAttackTimer = 13;
      const Player before = player;
      const CustomItemState state = gCustomItemState;
      Matrix_Translate(200, 300, 400, MTXMODE_NEW);
      Matrix_RotateZ(.37f, MTXMODE_APPLY);
      const MtxF caller = current;
      CustomItems_DrawSpinner(&player, &play);
      assert(drawn == std::vector<std::string>{NEI_HELD_PATH("spinner")});
      assert(nativeDraws == 0 && resourceQueries.empty());
      const float angle = (frame % 32) * (2.f * M_PI / 32.f);
      const float c = std::cos(angle) * .2f, s = std::sin(angle) * .2f;
      const MtxF &model = poses.front();
      near(transform(model, {0, 0, 0}), {12, height, -8});
      near(transform(model, {1, 0, 0}), {12 + c, height, -8 - s});
      near(transform(model, {0, 1, 0}), {12, height + .2f, -8});
      near(transform(model, {0, 0, 1}), {12 + s, height, -8 + c});
      assert(std::memcmp(&current, &caller, sizeof(current)) == 0);
      unchanged(player, before, state);
    }
  }

  resetWrappers(graphics, opa, xlu);
  available.insert(kOriginalSpinner);
  const Player before = player;
  const CustomItemState state = gCustomItemState;
  CustomItems_DrawSpinner(&player, &play);
  assert(drawn.empty() && nativeDraws == 1);
  assert(resourceLoads == std::vector<std::string>{kOriginalSpinner});
  // The fallback retains its original .2 scale and -150 native-unit offset.
  near(transform(current, {0, 0, 0}), {12, 107, -8});
  near(transform(current, {0, 1, 0}), {12, 107.2f, -8});
  unchanged(player, before, state);
}

void testSomariaWrapper(Player &player, PlayState &play,
                        GraphicsContext &graphics, Gfx *opa, Gfx *xlu) {
  selectedCane = CANE_TYPE_SOMARIA;
  aimingCane = 0;
  player.bodyPartsPos[PLAYER_BODYPART_R_HAND] = {10, 20, 30};
  player.bodyPartsPos[PLAYER_BODYPART_R_FOREARM] = {10, 10, 30};
  for (int age : {LINK_AGE_CHILD, LINK_AGE_ADULT}) {
    gSaveContext.linkAge = age;
    const Vec3f grip =
        age == LINK_AGE_CHILD ? Vec3f{0, 216.22f, -4.5f} : Vec3f{0, 328, 77};
    for (float bodyScale : {.01f, .001f}) {
      player.actor.scale.x = player.actor.scale.y = player.actor.scale.z =
          bodyScale;
      for (int sample = 0; sample < 8; ++sample) {
        resetWrappers(graphics, opa, xlu);
        available.insert(NEI_HELD_PATH("cane_of_somaria"));
        available.insert(kOriginalCane);
        // Keep the left wrist and hand/forearm endpoints unrelated to this
        // full right-wrist frame. Either wrong-hand routing or lost roll fails.
        Matrix_Translate(-400, -500, -600, MTXMODE_NEW);
        Matrix_Scale(bodyScale, bodyScale, bodyScale, MTXMODE_APPLY);
        ItemEquip_CaptureLeftHandMatrix();
        Matrix_Translate(42 + sample, 60, 90, MTXMODE_NEW);
        Matrix_RotateY(sample * .31f, MTXMODE_APPLY);
        Matrix_RotateX(sample * -.47f, MTXMODE_APPLY);
        Matrix_RotateZ(sample * .61f, MTXMODE_APPLY);
        Matrix_Scale(bodyScale, bodyScale, bodyScale, MTXMODE_APPLY);
        const MtxF wrist = current;
        ItemEquip_CaptureHandMatrix();
        Matrix_Translate(-100, -200, -300, MTXMODE_NEW);
        const MtxF caller = current;
        gCustomItemState = {};
        gCustomItemState.somariaActive = 1;
        gCustomItemState.somariaAnimating = sample % 2;
        gCustomItemState.somariaAnimTimer = sample * 3;
        const Player before = player;
        const CustomItemState state = gCustomItemState;
        CustomItems_DrawCaneOfSomaria(&player, &play);
        assert(drawn ==
               std::vector<std::string>{NEI_HELD_PATH("cane_of_somaria")});
        assert(nativeDraws == 0 && resourceQueries.empty());
        near(transform(poses.front(), {0, 0, 0}), transform(wrist, grip));
        near(transform(poses.front(), {0, 10, 0}),
             transform(wrist, {10 / bodyScale, grip.y, grip.z}));
        assert(std::memcmp(&current, &caller, sizeof(current)) == 0);
        assert(caneEffects ==
               std::vector<std::string>({"ultrahand", "fuse", "flip"}));
        unchanged(player, before, state);
      }
    }
  }

  // A missing replacement keeps the original red staff; no captured right
  // wrist also takes that fallback even when only a left wrist is present.
  for (bool replacementPresent : {false, true}) {
    resetWrappers(graphics, opa, xlu);
    available.insert(kOriginalCane);
    if (replacementPresent) {
      available.insert(NEI_HELD_PATH("cane_of_somaria"));
      ItemEquip_ReleaseHandMatrix();
      ItemEquip_CaptureLeftHandMatrix();
    }
    const Player before = player;
    const CustomItemState state = gCustomItemState;
    CustomItems_DrawCaneOfSomaria(&player, &play);
    assert(drawn.empty() && nativeDraws == 1);
    assert(commandColors(opa, graphics.polyOpa.p, G_SETPRIMCOLOR) ==
           std::vector<uint32_t>{0xFF3C3CFF});
    assert(commandColors(opa, graphics.polyOpa.p, G_SETENVCOLOR) ==
           std::vector<uint32_t>{0x8C0000FF});
    unchanged(player, before, state);
  }

  // Another cane family must not opt into Somaria just because its resources
  // and wrist exist. Check the actual graphics colors and existing VFX gates.
  for (int type : {CANE_TYPE_PACCI, CANE_TYPE_ULTRAHAND, CANE_TYPE_TRIROD}) {
    selectedCane = type;
    for (bool animating : {false, true}) {
      resetWrappers(graphics, opa, xlu);
      available.insert(NEI_HELD_PATH("cane_of_somaria"));
      available.insert(kOriginalCane);
      ItemEquip_CaptureHandMatrix();
      gCustomItemState = {};
      gCustomItemState.somariaActive = 1;
      gCustomItemState.somariaAnimating = animating;
      const Player before = player;
      const CustomItemState state = gCustomItemState;
      CustomItems_DrawCaneOfSomaria(&player, &play);
      assert(drawn.empty());
      assert(nativeDraws == (type == CANE_TYPE_ULTRAHAND ? 0 : 1));
      std::vector<std::string> expected = {"ultrahand", "fuse", "flip"};
      if (type == CANE_TYPE_TRIROD && !animating)
        expected.emplace_back("trirod");
      assert(caneEffects == expected);
      if (type == CANE_TYPE_ULTRAHAND) {
        assert(resourceQueries.empty() && graphics.polyOpa.p == opa);
      } else {
        assert(commandColors(opa, graphics.polyOpa.p, G_SETPRIMCOLOR) ==
               std::vector<uint32_t>{type == CANE_TYPE_PACCI ? 0xFFD746FF
                                                             : 0xFF3C3CFF});
        assert(commandColors(opa, graphics.polyOpa.p, G_SETENVCOLOR) ==
               std::vector<uint32_t>{type == CANE_TYPE_PACCI ? 0x966900FF
                                                             : 0x8C0000FF});
      }
      unchanged(player, before, state);
    }
  }

  resetWrappers(graphics, opa, xlu);
  gCustomItemState = {};
  CustomItems_DrawCaneOfSomaria(&player, &play);
  assert(drawn.empty() && nativeDraws == 0 && graphics.polyOpa.p == opa);
  assert(caneEffects == std::vector<std::string>({"ultrahand", "fuse"}));

  // Replacement dispatch cannot swallow the block/platform landing previews.
  selectedCane = CANE_TYPE_SOMARIA;
  aimingCane = 1;
  for (int skill : {CANE_SKILL_SOMARIA_BLOCK, CANE_SKILL_SOMARIA_PLATFORM}) {
    selectedCaneSkill = skill;
    for (bool animating : {false, true}) {
      resetWrappers(graphics, opa, xlu);
      available.insert(NEI_HELD_PATH("cane_of_somaria"));
      gCustomItemState = {};
      gCustomItemState.somariaActive = 1;
      gCustomItemState.somariaAnimating = animating;
      canePreviewPos = {4, 5, 6};
      canePreviewYaw = 123;
      canePreviewValid = 1;
      const Player before = player;
      const CustomItemState state = gCustomItemState;
      CustomItems_DrawCaneOfSomaria(&player, &play);
      assert(drawn ==
             std::vector<std::string>{NEI_HELD_PATH("cane_of_somaria")});
      std::vector<std::string> expected = {"ultrahand", "fuse", "flip"};
      if (!animating) {
        expected.emplace_back("summon");
        assert(previewKind == (skill == CANE_SKILL_SOMARIA_PLATFORM
                                   ? CANE_SUMMON_PLATFORM
                                   : CANE_SUMMON_BLOCK));
        near(previewPosition, {4, 5, 6});
        assert(previewYaw == 123 && previewValid == 1);
      }
      assert(caneEffects == expected);
      unchanged(player, before, state);
    }
  }
}
} // namespace

int main(int argc, char **argv) {
  Player player{};
  PlayState play{};
  GraphicsContext graphics{};
  Gfx opa[1024], xlu[1024];
  graphics.polyOpa.p = opa;
  graphics.polyXlu.p = xlu;
  play.state.gfxCtx = &graphics;
  play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
  player.actor.scale.x = player.actor.scale.y = player.actor.scale.z = .01f;
  player.bodyPartsPos[PLAYER_BODYPART_L_HAND] = {10, 20, 30};
  player.bodyPartsPos[PLAYER_BODYPART_L_FOREARM] = {10, 10, 30};
  if (argc > 1 && std::strcmp(argv[1], "--missing-wrapper-resources") == 0) {
    testMissingWrapperResources(player, play, graphics, opa, xlu);
    return 0;
  }
  const char *paths[] = {NEI_HELD_PATH("fire_rod"), NEI_HELD_PATH("ice_rod"),
                         NEI_HELD_PATH("light_rod")};
  void (*draw[])(Player *, PlayState *) = {CustomItems_DrawFireRod,
                                           CustomItems_DrawIceRod,
                                           CustomItems_DrawLightRod};
  // First-person transitions must hide both replacement and fallback rods,
  // while the actual single shot keeps rendering. Leaving aim restores the
  // normal held route for either age without changing gameplay state.
  for (int age : {LINK_AGE_ADULT, LINK_AGE_CHILD})
    for (int element = 0; element < 3; ++element)
      for (bool heldResource : {false, true})
        for (bool wrist : {false, true})
          for (int cameraTimer : {14, 13, 10, 1}) {
            gSaveContext.linkAge = age;
            player.unk_6AD = cameraTimer;
            gCustomItemState = {};
            gCustomItemState.fireRodActive = gCustomItemState.iceRodActive =
                gCustomItemState.lightRodActive = 1;
            auto firstPerson = std::array<u8 *, 3>{
                &gCustomItemState.fireRodFirstPerson,
                &gCustomItemState.iceRodFirstPerson,
                &gCustomItemState.lightRodFirstPerson};
            localRodSets[element] = true;
            for (auto &set : rodSets[element]) set = {};
            rodSets[element][0].active = 1;
            rodSets[element][0].count = 1;
            rodSets[element][0].scale = 2;
            rodSets[element][0].pos[0] = {100, 20, 30};
            rodSets[element][0].vel[0] = {18, 0, 0};
            for (bool aiming : {true, false}) {
              resetWrappers(graphics, opa, xlu);
              if (heldResource) available.insert(paths[element]);
              if (wrist) ItemEquip_CaptureLeftHandMatrix();
              else ItemEquip_ReleaseHandMatrix();
              *firstPerson[element] = aiming;
              projectileCalls.clear();
              trailCalls.clear();
              const Player before = player;
              const CustomItemState state = gCustomItemState;
              draw[element](&player, &play);
              if (aiming) {
                assert(drawn.empty() && nativeDraws == 0);
              } else if (heldResource && wrist) {
                assert(drawn == std::vector<std::string>{paths[element]});
                assert(nativeDraws == 0);
              } else {
                assert(drawn.empty() && nativeDraws > 0);
              }
              assert(projectileCalls.size() == 1 && trailCalls.size() == 1);
              near(projectileCalls[0].position, {100, 20, 30});
              near(projectileCalls[0].velocity, {18, 0, 0});
              unchanged(player, before, state);
              // Four Sword clones share the local state but remain visible.
              if (aiming) {
                Player clone = player;
                resetWrappers(graphics, opa, xlu);
                if (heldResource) available.insert(paths[element]);
                if (wrist) ItemEquip_CaptureLeftHandMatrix();
                else ItemEquip_ReleaseHandMatrix();
                draw[element](&clone, &play);
                assert(!drawn.empty() || nativeDraws > 0);
                unchanged(clone, before, state);
              }
            }
            localRodSets[element] = false;
          }
  player.unk_6AD = 0;
  std::cout << "PASS all three rods hide held models in first-person aim, "
               "retain single shots, and restore third-person models\n";
  // Rotate the wrist while arm endpoints remain fixed. A direction rebuilt
  // from those endpoints cannot preserve either the grip or the wrist roll.
  for (int sample = 0; sample < 16; ++sample) {
    gSaveContext.linkAge = sample / 8;
    // Independent landmarks from native Master/Kokiri Sword hilt sections
    // at wrist-local X=0. The child point also fits the YoungDin closed fist.
    const Vec3f grip = gSaveContext.linkAge == LINK_AGE_CHILD
                           ? Vec3f{0, 216.22f, 4.5f}
                           : Vec3f{0, 328, -77};
    Matrix_Translate(10, 20, 30, MTXMODE_NEW);
    Matrix_RotateY(sample * .29f, MTXMODE_APPLY);
    Matrix_RotateX(sample * -.41f, MTXMODE_APPLY);
    Matrix_RotateZ(sample * .63f, MTXMODE_APPLY);
    Matrix_Scale(.01f, .01f, .01f, MTXMODE_APPLY);
    const MtxF wrist = current;
    ItemEquip_CaptureLeftHandMatrix();
    for (int element = 0; element < 3; ++element) {
      reset();
      available.insert(paths[element]);
      gCustomItemState = {};
      gCustomItemState.fireRodActive = gCustomItemState.iceRodActive =
          gCustomItemState.lightRodActive = 1;
      const CustomItemState state = gCustomItemState;
      const Player before = player;
      draw[element](&player, &play);
      assert(drawn == std::vector<std::string>{paths[element]} &&
             nativeDraws == 0);
      near(transform(poses.front(), {0, 0, 0}), transform(wrist, grip));
      const float tip = (element == 1 ? 68.f : 66.f) * 4.f;
      const float angle = 32.f * M_PI / 180.f;
      // Serialized shaft axis is 122 degrees from model +X. After fit,
      // its focus must lie on the native left-hand weapon's +X axis.
      near(transform(poses.front(),
                     {-std::sin(angle) * tip, std::cos(angle) * tip, 0}),
           transform(wrist, {tip * 5.f, grip.y, grip.z}));
      assert(std::memcmp(&state, &gCustomItemState, sizeof(state)) == 0);
      assert(std::memcmp(&before, &player, sizeof(before)) == 0);
    }
  }
  // A right-hand capture cannot replace the left wrist, and a new player
  // draw without a left wrist cannot borrow the preceding player's grip.
  const ItemHandPose neutral = {0, 0, 0, 0, 0, 0, 1};
  Matrix_Translate(100, 200, 300, MTXMODE_NEW);
  Matrix_Scale(.01f, .01f, .01f, MTXMODE_APPLY);
  ItemEquip_CaptureHandMatrix();
  assert(ItemEquip_ApplyLeftHandPose(&player, &neutral));
  near(transform(current, {0, 0, 0}), {10, 20, 30});
  assert(ItemEquip_ApplyHandPose(&player, &neutral));
  near(transform(current, {0, 0, 0}), {100, 200, 300});
  ItemEquip_ReleaseHandMatrix();
  assert(!ItemEquip_ApplyLeftHandPose(&player, &neutral));
  assert(!ItemEquip_ApplyHandPose(&player, &neutral));
  for (int element = 0; element < 3; ++element) {
    reset();
    available.insert(paths[element]);
    draw[element](&player, &play);
    assert(drawn.empty() && nativeDraws > 0);
  }
  ItemEquip_CaptureLeftHandMatrix();
  for (int element = 0; element < 3; ++element) {
    reset();
    draw[element](&player, &play);
    assert(drawn.empty() && nativeDraws > 0);
  }
  // Execute the full production rod drawers, not just the pure effect samplers.
  // Held replacement availability and wrist capture must never swallow shots.
  for (int element = 0; element < 3; ++element)
    for (bool heldResource : {false, true})
      for (bool wrist : {false, true}) {
        resetWrappers(graphics, opa, xlu);
        if (heldResource)
          available.insert(paths[element]);
        if (wrist)
          ItemEquip_CaptureLeftHandMatrix();
        else
          ItemEquip_ReleaseHandMatrix();
        gCustomItemState = {};
        gCustomItemState.fireRodActive = gCustomItemState.iceRodActive =
            gCustomItemState.lightRodActive = 1;
        localRodSets[element] = true;
        for (int set : {0, 4}) {
          rodSets[element][set] = {};
          rodSets[element][set].active = 1;
          rodSets[element][set].count = set == 0 ? 3 : 2;
          rodSets[element][set].scale = 2;
          for (int i = 0; i < 6; ++i)
            rodSets[element][set].trail[i] = {
                float(set * 100), 20 - float(i) * 3, 30 - float(i) * 15};
          for (int head = 0; head < 3; ++head) {
            rodSets[element][set].pos[head] = {float(set * 100 + head), 20, 30};
            rodSets[element][set].vel[head] = {18, 0, 0};
          }
        }
        projectileCalls.clear();
        trailCalls.clear();
        trailPositions.clear();
        rodDrawOrder.clear();
        draw[element](&player, &play);
        assert(projectileCalls.size() == 5 &&
               trailCalls == std::vector<int>(element == 1 ? 5 : 2, element));
        if (element == 1)
          assert(rodDrawOrder == "TTTHHHTTHH");
        for (size_t head = 0; head < 5; ++head) {
          const auto &call = projectileCalls[head];
          assert(call.element == element && call.scale == 2 &&
                 call.velocity.x == 18);
          assert(call.position.x == float(head < 3 ? head : 400 + head - 3));
          if (element == 1) {
            const auto &wake = trailPositions[head];
            assert(wake[0].x == call.position.x &&
                   wake[0].y == call.position.y &&
                   wake[0].z == call.position.z);
            const int index = head < 3 ? head : head - 3;
            const float angle = (index == 1   ? -1.f
                                 : index == 2 ? 1.f
                                              : 0.f) *
                                5460.f * (6.2831853071795864769f / 65536.f);
            assert(std::abs(wake[5].x -
                            (call.position.x - 75 * std::sin(angle))) < .001f);
            assert(std::abs(wake[5].z -
                            (call.position.z - 75 * std::cos(angle))) < .001f);
            assert(wake[5].y == call.position.y - 15);
          }
        }
        localRodSets[element] = false;
        gCustomItemState.fireRodProjActive = gCustomItemState.iceRodProjActive =
            gCustomItemState.lightRodProjActive = 1;
        gCustomItemState.fireRodProjCount = gCustomItemState.iceRodProjCount =
            gCustomItemState.lightRodProjCount = 3;
        gCustomItemState.fireRodProjScale = gCustomItemState.iceRodProjScale =
            gCustomItemState.lightRodProjScale = 2;
        gCustomItemState.iceRodProjPos = {10, 20, 30};
        gCustomItemState.iceRodProjPos2 = {-40, 20, 15};
        gCustomItemState.iceRodProjPos3 = {60, 20, 15};
        // A stopped/fading center has duplicate newest history entries. Side
        // reconstruction must preserve these, not invent velocity or extension.
        for (int i = 0; i < 6; ++i)
          gCustomItemState.iceRodProjTrail[i] = {
              10, 20, 30 - float(std::max(i - 2, 0)) * 15};
        projectileCalls.clear();
        trailCalls.clear();
        trailPositions.clear();
        rodDrawOrder.clear();
        draw[element](&player, &play);
        assert(projectileCalls.size() == 3 &&
               trailCalls == std::vector<int>(element == 1 ? 3 : 1, element));
        if (element == 1)
          assert(rodDrawOrder == "TTTHHH");
        for (const auto &call : projectileCalls)
          assert(call.element == element && call.scale == 2);
        if (element == 1)
          for (int head = 0; head < 3; ++head) {
            const auto p = projectileCalls[head].position;
            for (int i = 0; i < 3; ++i)
              assert(trailPositions[head][i].x == p.x &&
                     trailPositions[head][i].z == p.z);
            const float dx = trailPositions[head][5].x - p.x,
                        dz = trailPositions[head][5].z - p.z;
            assert(std::abs(std::sqrt(dx * dx + dz * dz) - 45) < .001f);
            const float angle = (head == 1   ? -1.f
                                 : head == 2 ? 1.f
                                             : 0.f) *
                                5460.f * (6.2831853071795864769f / 65536.f);
            const auto velocity = projectileCalls[head].velocity;
            assert(std::abs(velocity.x - 15 * std::sin(angle)) < .001f);
            assert(std::abs(velocity.z - 15 * std::cos(angle)) < .001f &&
                   velocity.y == 0);
          }
        gCustomItemState.fireRodActive = gCustomItemState.iceRodActive =
            gCustomItemState.lightRodActive = 0;
        projectileCalls.clear();
        trailCalls.clear();
        draw[element](&player, &play);
        assert(projectileCalls.empty() && trailCalls.empty());
      }
  std::cout
      << "PASS real Fire/Ice/Light object draw dispatch: local three-shot sets "
         "and remote sync, held-resource/wrist fallback, inactive guards\n";
  // Adult and child reductions keep the two-hand midpoint fixed.
  player.bodyPartsPos[PLAYER_BODYPART_R_HAND] = {-10, 20, 30};
  for (int age : {LINK_AGE_ADULT, LINK_AGE_CHILD}) {
    reset();
    available.insert(NEI_HELD_PATH("shovel"));
    gSaveContext.linkAge = age;
    gCustomItemState = {};
    gCustomItemState.shovelActive = 1;
    CustomItems_DrawShovel(&player, &play);
    assert(drawn == std::vector<std::string>{NEI_HELD_PATH("shovel")});
    near(transform(poses.front(), {0, 0, 0}), {0, 20, 30});
    const MtxF &m = poses.front();
    const float scale = std::sqrt(m.xx * m.xx + m.yx * m.yx + m.zx * m.zx);
    assert(std::fabs(scale - (age == LINK_AGE_CHILD ? .048f : .054f)) <
           .00001f);
  }
  reset();
  CustomItems_DrawShovel(&player, &play);
  assert(drawn.empty() && nativeDraws == 1);
  assert(std::fabs(std::sqrt(current.xx * current.xx + current.yx * current.yx +
                             current.zx * current.zx) -
                   .06f) < .00001f);
  std::cout << "PASS: real rod drawers retain wrist roll and grip across eight "
               "poses; child/adult shovel scale\n";
  testSpinnerWrapper(player, play, graphics, opa, xlu);
  testSomariaWrapper(player, play, graphics, opa, xlu);
  std::cout
      << "PASS: real Spinner rotation/height/fallback and Somaria right-wrist "
         "child/adult grip; Pacci/Ultrahand/Trirod colors and VFX preserved\n";
}
