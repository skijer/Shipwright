"""Compile verbatim native reaction paths without unrelated actor draw loops."""
from pathlib import Path
import os
import re
import subprocess

from run_time_pedestal_tests import functions

ROOT = Path(__file__).resolve().parents[2]


def build_reactions(folder, flags):
    common = folder / "reaction_boundaries.h"
    common.write_text('''#include "global.h"
#include "din_fire_sword.h"
static void Test_Animation(SkelAnime* skel, AnimationHeader* animation, f32 morph) { skel->animLength = 10; }
static void Test_PlayOnce(SkelAnime* skel, AnimationHeader* animation) { skel->animLength = 10; }
static s16 Test_LastFrame(void* animation) { return 10; }
static void Test_AnimationInfo(SkelAnime* skel, AnimationInfo* info, s32 index) { skel->animLength = 10; }
static void Test_Sound(Actor* actor, u16 sound) {}
static void Test_Filter(Actor* actor, s16 color, s16 intensity, s16 xlu, s16 duration) {}
static void Test_Finish(PlayState* play, Actor* actor) {}
static void Test_Defeat(Actor* actor) {}
static void Test_ActorAction(Actor* actor, PlayState* play) {}
#define Animation_MorphToPlayOnce Test_Animation
#define Animation_PlayOnce Test_PlayOnce
#define Animation_GetLastFrame Test_LastFrame
#define Animation_ChangeByInfo Test_AnimationInfo
#define Audio_PlayActorSound2 Test_Sound
#define Actor_SetColorFilter Test_Filter
#define Enemy_StartFinishingBlow Test_Finish
#define GameInteractor_ExecuteOnEnemyDefeat Test_Defeat
#define GameInteractor_ExecuteOnBossDefeat Test_Defeat
''')
    recoil = [
        ("Wallmaster", "En_Wallmas", "EnWallmas", "EnWallmas_SetupTakeDamage", "EnWallmas_TakeDamage", "collider"),
        ("Floormaster", "En_Floormas", "EnFloormas", "EnFloormas_SetupTakeDamage", "EnFloormas_TakeDamage", "collider"),
        ("DekuScrub", "En_Dekunuts", "EnDekunuts", "EnDekunuts_SetupBeDamaged", "EnDekunuts_BeDamaged", "collider"),
        ("PoeField", "En_Po_Field", "EnPoField", "EnPoField_SetupDamage", "EnPoField_Damage", "collider"),
        ("Poe", "En_Poh", "EnPoh", "func_80ADE28C", "func_80ADEECC", "colliderCyl"),
        ("PoeSisters", "En_Po_Sisters", "EnPoSisters", "func_80AD95D8", "func_80ADAAA4", "collider"),
    ]
    paths = []

    def actor_source(actor):
        relative = f"overlays/actors/ovl_{actor}/z_{actor.lower()}"
        text = (ROOT / "soh/src" / (relative + ".c")).read_text()
        assets = re.findall(r'^#include "(objects/[^\"]+)"', text, re.M)
        prefix = f'#include "{relative}.h"\n' + ''.join(f'#include "{asset}"\n' for asset in assets)
        return text, prefix + '#include "reaction_boundaries.h"\n'

    def write(name, source):
        path = folder / (name + ".c")
        path.write_text(source)
        paths.append(str(path))

    for name, actor, type_, setup, next_action, collider in recoil:
        text, prefix = actor_source(actor)
        prefix += f'#define {setup} Test_{setup}\n#define {next_action} Test_{next_action}\n'
        prefix += f'static void {next_action}({type_}* actor, PlayState* play) {{}}\n'
        prefix += functions(text)[setup] + '\n'
        prefix += f'''s16 Test_{name}Recoil(ColliderInfo* toucher, Actor* attacker) {{
    {type_} actor = {{ 0 }};
    actor.{collider}.info.acHitInfo = toucher;
    actor.{collider}.base.ac = attacker;
    {setup}(&actor);
    return actor.actor.world.rot.y;
}}
'''
        write(name + "Reaction", prefix)

    text, prefix = actor_source("Obj_Comb")
    prefix += '''static void ObjComb_ChooseItemDrop(ObjComb* actor, PlayState* play) {}
void ObjComb_Break(ObjComb* actor, PlayState* play) {}
'''
    prefix += functions(text)["ObjComb_Wait"] + '''
int Test_BeehiveBreaks(PlayState* play, ColliderInfo* toucher) {
    ObjComb actor = { 0 };
    actor.actor.update = Test_ActorAction;
    actor.collider.elements = actor.colliderItems;
    actor.collider.base.acFlags = AC_HIT;
    actor.collider.elements[0].info.acHitInfo = toucher;
    ObjComb_Wait(&actor, play);
    return actor.actor.update == NULL;
}
'''
    write("BeehiveReaction", prefix)

    text, prefix = actor_source("En_St")
    prefix += re.search(r'typedef enum \{[^{}]*\} EnStAnimation;', text, re.S)[0] + '\n'
    prefix += re.search(r'static AnimationInfo sAnimationInfo\[\] = \{.*?^\};', text, re.S | re.M)[0] + '\n'
    prefix += '''static void EnSt_Die(EnSt* actor, PlayState* play) {}
static void EnSt_BounceAround(EnSt* actor, PlayState* play) {}
'''
    prefix += functions(text)["EnSt_SetupAction"] + '\n' + functions(text)["EnSt_CheckHitBackside"] + '''
int Test_SkulltulaBounces(PlayState* play, ColliderInfo* toucher, int collider) {
    EnSt actor = { 0 };
    actor.actor.colChkInfo.health = 1;
    actor.actor.colChkInfo.damage = 1;
    actor.colCylinder[collider].base.acFlags = AC_HIT;
    actor.colCylinder[collider].info.acHitInfo = toucher;
    EnSt_CheckHitBackside(&actor, play);
    return actor.actor.colChkInfo.health == 0 && actor.actionFunc == EnSt_BounceAround;
}
'''
    write("SkulltulaReaction", prefix)

    text, prefix = actor_source("En_Bili")
    prefix += re.search(r'typedef enum \{[^{}]*\} BiriDamageEffect;', text, re.S)[0] + '\n'
    for name in ("EnBili_SetupStunned", "EnBili_SetupDischargeLightning", "EnBili_SetupBurnt", "EnBili_SetupRecoil"):
        prefix += f'static void {name}(EnBili* actor) {{}}\n'
    prefix += '''static void EnBili_SetupFrozen(EnBili* actor, PlayState* play) {}
static void EnBili_Stunned(EnBili* actor, PlayState* play) {}
'''
    prefix += functions(text)["EnBili_UpdateDamage"] + '''
int Test_BiriPersistsOffscreen(PlayState* play, ColliderInfo* toucher) {
    EnBili actor = { 0 };
    actor.actor.colChkInfo.health = 2;
    actor.actor.colChkInfo.damage = 1;
    actor.actor.colChkInfo.damageEffect = BIRI_DMGEFF_SWORD;
    actor.collider.base.acFlags = AC_HIT;
    actor.collider.info.acHitInfo = toucher;
    EnBili_UpdateDamage(&actor, play);
    return !!(actor.actor.flags & ACTOR_FLAG_UPDATE_CULLING_DISABLED);
}
'''
    write("BiriReaction", prefix)

    text, prefix = actor_source("Boss_Ganondrof")
    prefix += '#include "overlays/actors/ovl_En_fHG/z_en_fhg.h"\n'
    prefix += '''#define GND_PIKA_RING_RADIUS 300.0f
static u8 gPikaGigantamaxMode;
static int BossSuperDamage_IsFormActive(PlayState* play) { return 0; }
static int BossSuperDamage_FormDamage(PlayState* play) { return 4; }
static void BossSuperDamage_StartElectricSparks(Actor* actor, int timer) {}
static void BossGanondrof_Stunned(BossGanondrof* actor, PlayState* play) {}
static void BossGanondrof_Charge(BossGanondrof* actor, PlayState* play) {}
static void BossGanondrof_SetupStunned(BossGanondrof* actor, PlayState* play) { actor->actionFunc = BossGanondrof_Stunned; }
static void BossGanondrof_SetupDeath(BossGanondrof* actor, PlayState* play) {}
'''
    prefix += functions(text)["BossGanondrof_CollisionCheck"] + '''
int Test_PhantomHealth(PlayState* play, ColliderInfo* toucher, int painting) {
    BossGanondrof actor = { 0 };
    EnfHG horse = { 0 };
    actor.actor.child = &horse.actor;
    actor.actor.params = GND_REAL_BOSS;
    actor.actor.colChkInfo.health = 10;
    actor.flyMode = painting ? GND_FLY_PAINTING : GND_FLY_NEUTRAL;
    actor.colliderBody.base.acFlags = AC_HIT;
    actor.colliderBody.info.acHitInfo = toucher;
    BossGanondrof_CollisionCheck(&actor, play);
    return actor.actor.colChkInfo.health;
}
'''
    write("PhantomReaction", prefix)

    # The randomizer branch is C++; only its option/identity store and item
    # delivery boundary are fixture data. The wait/break decision is verbatim.
    randomizer = folder / "RandomizerBeehiveReaction.cpp"
    text = (ROOT / "soh/soh/Enhancements/randomizer/ShuffleBeehives.cpp").read_text()
    randomizer.write_text('''#include "global.h"
extern "C" {
#include "overlays/actors/ovl_Obj_Comb/z_obj_comb.h"
}
#include "din_fire_sword.h"
class ObjectExtension {
  public:
    static ObjectExtension& GetInstance() { static ObjectExtension store; return store; }
    template<class T> T* Get(Actor*) { static T identity; return &identity; }
};
#define RAND_GET_OPTION(key) ((key) == RSK_SHUFFLE_BEEHIVES)
static bool Test_RandoInf(RandomizerInf flag) { return false; }
#define Flags_GetRandomizerInf Test_RandoInf
static void ObjComb_RandomizerChooseItemDrop(ObjComb* actor, PlayState* play) {}
static void Test_Update(Actor* actor, PlayState* play) {}
''' + functions(text)["ObjComb_RandomizerWait"] + '''
extern "C" int Test_RandomizerBeehiveBreaks(PlayState* play, ColliderInfo* toucher) {
    ObjComb actor{};
    actor.actor.update = Test_Update;
    actor.collider.elements = actor.colliderItems;
    actor.collider.base.acFlags = AC_HIT;
    actor.collider.elements[0].info.acHitInfo = toucher;
    ObjComb_RandomizerWait(&actor, play);
    return actor.actor.update == nullptr;
}
''')
    obj = folder / "RandomizerBeehiveReaction.o"
    cxxflags = [flag for flag in flags if flag not in ("-std=gnu2x", "-Werror=implicit-function-declaration")]
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", *cxxflags, "-c", str(randomizer), "-o", str(obj)],
                   cwd=ROOT, check=True)
    paths.append(str(obj))
    yaw = functions((ROOT / "soh/src/code/z_lib.c").read_text())["Math_Vec3f_Yaw"]
    write("Yaw", '#include "global.h"\n' + yaw)
    paths.append("soh/src/code/sys_math_atan.c")
    return paths
