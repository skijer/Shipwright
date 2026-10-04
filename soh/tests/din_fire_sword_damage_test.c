// Exercise production collision, damage tables, and drop classification. The
// renderer fixture supplies only resource/config/graphics engine boundaries.
#define main DinFireSword_RenderFixtureMain
#define WeaponUpgrade_KokiriLevel DinFireSword_RenderFixtureKokiriLevel
#define WeaponUpgrade_HasGreatFairy DinFireSword_RenderFixtureGreatFairy
#include "din_fire_sword_test.c"
#undef main
#undef WeaponUpgrade_KokiriLevel
#undef WeaponUpgrade_HasGreatFairy
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

static u8 kokiriUpgrade, greatFairyUpgrade;
u8 WeaponUpgrade_KokiriLevel(void) {
    return kokiriUpgrade;
}
u8 WeaponUpgrade_HasGreatFairy(void) {
    return greatFairyUpgrade;
}
u8 gIvanPossessActive;
static GameInfo gameInfo;
GameInfo* gGameInfo = &gameInfo;
PlayState* gPlayState = &play;
u8 Sm64Mario_IsReady(void) {
    return 0;
}
u8 TransformMasks_IsFDSkinMode(void) {
    return 0;
}
bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...) {
    return result;
}
void GameInteractor_ExecuteOnActorKill(void* actor) {
}
void TimeCtl_NoteAcCollider(Collider* collider) {
}
uint8_t GameInteractor_SecondCollisionUpdate(void) {
    return 0;
}
s32 FrameAdvance_IsEnabled(PlayState* play) {
    return 0;
}
void lusprintf(const char* file, int32_t line, int32_t level, const char* fmt, ...) {
}

DamageTable* Test_DekuBabaTable(void);
DamageTable* Test_ReDeadTable(void);
DamageTable* Test_AnubisTable(void);
DamageTable* Test_WolfosTable(void);
DamageTable* Test_FreezardTable(void);
DamageTable* Test_LizalfosTable(void);
DamageTable* Test_ArmosTable(void);
DamageTable* Test_FireflyTable(void);
DamageTable* Test_TailpasaranTable(void);
DamageTable* Test_WallmasterTable(void);
DamageTable* Test_FloormasterTable(void);
DamageTable* Test_CrowTable(void);
DamageTable* Test_DekuNutsTable(void);
DamageTable* Test_PeahatTable(void);
u32 Test_BushMask(void);
u32 Test_WitheredMask(void);
s16 Test_WallmasterRecoil(ColliderInfo* toucher, Actor* attacker);
s16 Test_FloormasterRecoil(ColliderInfo* toucher, Actor* attacker);
s16 Test_DekuScrubRecoil(ColliderInfo* toucher, Actor* attacker);
s16 Test_PoeFieldRecoil(ColliderInfo* toucher, Actor* attacker);
s16 Test_PoeRecoil(ColliderInfo* toucher, Actor* attacker);
s16 Test_PoeSistersRecoil(ColliderInfo* toucher, Actor* attacker);
int Test_BeehiveBreaks(PlayState* play, ColliderInfo* toucher);
int Test_RandomizerBeehiveBreaks(PlayState* play, ColliderInfo* toucher);
int Test_SkulltulaBounces(PlayState* play, ColliderInfo* toucher, int collider);
int Test_BiriPersistsOffscreen(PlayState* play, ColliderInfo* toucher);
int Test_PhantomHealth(PlayState* play, ColliderInfo* toucher, int painting);
s32 CollisionCheck_NoSharedFlags(ColliderInfo* atInfo, ColliderInfo* acInfo);
void CollisionCheck_ApplyDamage(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider,
                                ColliderInfo* info);

static int failures;
#define CHECK(label, condition)                                                      \
    do {                                                                             \
        if (!(condition)) {                                                          \
            fprintf(stderr, "FAIL %s (line %d): %s\n", label, __LINE__, #condition); \
            ++failures;                                                              \
        }                                                                            \
    } while (0)

static const u32 swordFlags[3][3] = {
    { DMG_SLASH_KOKIRI, DMG_SPIN_KOKIRI, DMG_JUMP_KOKIRI },
    { DMG_SLASH_MASTER, DMG_SPIN_MASTER, DMG_JUMP_MASTER },
    { DMG_SLASH_GIANT, DMG_SPIN_GIANT, DMG_JUMP_GIANT },
};
static const u8 swordDamage[3][3] = { { 1, 1, 2 }, { 2, 2, 4 }, { 4, 4, 8 } };

static void setupSword(int sword) {
    setup();
    kokiriUpgrade = greatFairyUpgrade = 0;
    // Invalidate the production cache between independent test worlds.
    static s16 scene;
    play.sceneNum = ++scene;
    fireDamage = 1;
    if (sword == 0) {
        gSaveContext.linkAge = LINK_AGE_CHILD;
        player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_KOKIRI;
    } else if (sword == 2) {
        player.itemAction = player.heldItemAction = PLAYER_IA_SWORD_BIGGORON;
        player.leftHandType = PLAYER_MODELTYPE_LH_BGS;
        gSaveContext.swordHealth = 8.0f;
    }
}

static ColliderInfo* setStrike(u32 flags) {
    for (int quad = 0; quad < 2; ++quad) {
        player.meleeWeaponQuads[quad].info.toucher.dmgFlags = DinFireSword_SetDamageFlags(&play, &player, quad, flags);
        player.meleeWeaponQuads[quad].info.toucher.damage = 7;
    }
    return &player.meleeWeaponQuads[0].info;
}

static Actor hit(s16 actorId, DamageTable* table, u32 mask, ColliderInfo* atInfo) {
    Actor actor = { 0 };
    Collider collider = { 0 };
    ColliderInfo info = { 0 };
    actor.id = actorId;
    actor.colChkInfo.damageTable = table;
    collider.actor = &actor;
    collider.acFlags = AC_HIT;
    info.bumper.dmgFlags = mask;
    info.bumperFlags = BUMP_HIT;
    info.acHitInfo = atInfo;
    if (!CollisionCheck_NoSharedFlags(atInfo, &info)) {
        CollisionCheck_ApplyDamage(&play, &play.colChkCtx, &collider, &info);
    }
    return actor;
}

static void testSwordHits(void) {
    for (int sword = 0; sword < 3; ++sword) {
        for (int strike = 0; strike < 3; ++strike) {
            setupSword(sword);
            ColliderInfo* atInfo = setStrike(swordFlags[sword][strike]);
            ColliderInfo receiver = { 0 };
            receiver.bumper.dmgFlags = Test_BushMask();
            CHECK("bush retains sword collision", !CollisionCheck_NoSharedFlags(atInfo, &receiver));
            receiver.bumper.dmgFlags = Test_WitheredMask();
            CHECK("upright withered Baba retains sword collision", !CollisionCheck_NoSharedFlags(atInfo, &receiver));
            CHECK("sword-only boss finisher damage",
                  CollisionCheck_GetSwordDamage(atInfo->toucher.dmgFlags, &play) == swordDamage[sword][strike]);

            Actor baba = hit(ACTOR_EN_DEKUBABA, Test_DekuBabaTable(), 0xFFCFFFFF, atInfo);
            CHECK("Baba retains native sword power", baba.colChkInfo.damage == swordDamage[sword][strike]);
            CHECK("Baba retains cutting and stick-drop effect", baba.colChkInfo.damageEffect == 15);
            Actor redead = hit(ACTOR_EN_RD, Test_ReDeadTable(), 0xFFCFFFFF, atInfo);
            CHECK("fire-resistant enemy retains sword damage", redead.colChkInfo.damage == swordDamage[sword][strike]);
            CHECK("fire-resistant enemy retains sword effect", redead.colChkInfo.damageEffect == 15);

            receiver.acHitInfo = atInfo;
            Actor_SetDropFlag(&baba, &receiver, 1);
            CHECK("single collider retains vanilla drops", baba.dropFlag == 0);
            ColliderJntSphElement element = { 0 };
            ColliderJntSph sphere = { 0 };
            sphere.count = 1;
            sphere.elements = &element;
            element.info.acHitInfo = atInfo;
            Actor_SetDropFlagJntSph(&baba, &sphere, 1);
            CHECK("joint sphere retains vanilla drops", baba.dropFlag == 0);

            Actor wolfos = hit(ACTOR_EN_WF, Test_WolfosTable(), 0xFFCFFFFF, atInfo);
            CHECK("Wolfos retains native sword power", wolfos.colChkInfo.damage == swordDamage[sword][strike]);
            CHECK("Wolfos adds its native burn effect", wolfos.colChkInfo.damageEffect == 14);
            Actor keese = hit(ACTOR_EN_FIREFLY, Test_FireflyTable(), 0xFFCFFFFF, atInfo);
            CHECK("Keese retains native sword power", keese.colChkInfo.damage == swordDamage[sword][strike]);
            CHECK("ice Keese supports fire-arrow combustion", keese.colChkInfo.damageEffect == 15);
            const struct {
                s16 actorId;
                DamageTable* table;
                u8 effect;
            } additiveFire[] = {
                { ACTOR_EN_WALLMAS, Test_WallmasterTable(), 2 }, { ACTOR_EN_FLOORMAS, Test_FloormasterTable(), 2 },
                { ACTOR_EN_CROW, Test_CrowTable(), 2 },          { ACTOR_EN_DEKUNUTS, Test_DekuNutsTable(), 2 },
                { ACTOR_EN_PEEHAT, Test_PeahatTable(), 12 },
            };
            for (size_t i = 0; i < ARRAY_COUNT(additiveFire); ++i) {
                Actor burning = hit(additiveFire[i].actorId, additiveFire[i].table, 0xFFCFFFFF, atInfo);
                CHECK("supported burning preserves native sword damage",
                      burning.colChkInfo.damage == swordDamage[sword][strike]);
                CHECK("supported burning uses this actor's effect code",
                      burning.colChkInfo.damageEffect == additiveFire[i].effect);
            }
            Actor lizalfos = hit(ACTOR_EN_ZF, Test_LizalfosTable(), 0xFFCFFFFF, atInfo);
            CHECK("Lizalfos keeps sword recoil instead of projectile recoil", lizalfos.colChkInfo.damageEffect == 0);
            Actor armos = hit(ACTOR_EN_AM, Test_ArmosTable(), 0xFFCFFFFF, atInfo);
            CHECK("Armos keeps each sword's native reaction", armos.colChkInfo.damageEffect == (sword == 0 ? 0 : 15));
            Actor tail = hit(ACTOR_EN_TP, Test_TailpasaranTable(), 0xFFCFFFFF, atInfo);
            CHECK("Tailpasaran retains sword damage", tail.colChkInfo.damage == swordDamage[sword][strike]);
            CHECK("Tailpasaran retains native shocking effect", tail.colChkInfo.damageEffect == 14);

            Actor anubis = hit(ACTOR_EN_ANUBICE, Test_AnubisTable(), 0xFFCFFFFF, atInfo);
            CHECK("Anubis sees required fire effect", anubis.colChkInfo.damageEffect == 2);
            Actor fireOnly = hit(ACTOR_EN_FZ, Test_FreezardTable(), DMG_ARROW_FIRE, atInfo);
            CHECK("fire-only receiver chooses fire table row",
                  fireOnly.colChkInfo.damage == 4 && fireOnly.colChkInfo.damageEffect == 2);

            fireDamage = 0;
            DinFireSword_RefreshDamage(&play, &player);
            for (int quad = 0; quad < 2; ++quad) {
                CHECK("crouch stab restores previous strike when toggled off",
                      player.meleeWeaponQuads[quad].info.toucher.dmgFlags == swordFlags[sword][strike]);
            }
            fireDamage = 1;
            DinFireSword_RefreshDamage(&play, &player);
            Actor crouch = hit(ACTOR_EN_RD, Test_ReDeadTable(), 0xFFCFFFFF, &player.meleeWeaponQuads[1].info);
            CHECK("crouch stab retains prior sword damage when toggled on",
                  crouch.colChkInfo.damage == swordDamage[sword][strike]);
        }
    }
}

static void testIsolation(void) {
    setupSword(1);
    ColliderInfo* owned = setStrike(DMG_JUMP_MASTER);
    ColliderInfo sameFlags = *owned;
    Actor copyResult = hit(ACTOR_EN_WF, Test_WolfosTable(), 0xFFCFFFFF, &sameFlags);
    CHECK("copied toucher is not the owned sword",
          copyResult.colChkInfo.damage == 4 && copyResult.colChkInfo.damageEffect == 0);
    ColliderInfo copiedReceiver = { 0 };
    copiedReceiver.acHitInfo = &sameFlags;
    Actor_SetDropFlag(&copyResult, &copiedReceiver, 1);
    CHECK("identical foreign flags still receive native fire drops", copyResult.dropFlag == 1);
    ColliderInfo foreign = { 0 };
    foreign.toucher.dmgFlags = DMG_SLASH_MASTER | DMG_ARROW_FIRE;
    Actor result = hit(ACTOR_EN_DEKUBABA, Test_DekuBabaTable(), 0xFFCFFFFF, &foreign);
    CHECK("foreign combined attack retains existing highest-bit semantics",
          result.colChkInfo.damage == 4 && result.colChkInfo.damageEffect == 2);
    foreign.toucher.dmgFlags = DMG_JUMP_GIANT | DMG_ARROW_FIRE;
    result = hit(ACTOR_EN_DEKUBABA, Test_DekuBabaTable(), 0xFFCFFFFF, &foreign);
    CHECK("foreign jump combination retains native highest-bit semantics",
          result.colChkInfo.damage == 8 && result.colChkInfo.damageEffect == 15);
    ColliderInfo receiver = { 0 };
    receiver.acHitInfo = &foreign;
    Actor_SetDropFlag(&result, &receiver, 1);
    CHECK("foreign fire attack retains fire-arrow drops", result.dropFlag == 1);
    foreign.toucher.dmgFlags = DMG_ARROW_FIRE;
    result = hit(ACTOR_EN_RD, Test_ReDeadTable(), 0xFFCFFFFF, &foreign);
    CHECK("real fire-arrow resistance unchanged", result.colChkInfo.damage == 0 && result.colChkInfo.damageEffect == 0);

    const u32 excluded[] = { DMG_HAMMER_SWING, DMG_DEKU_STICK, DMG_SLASH_MASTER | DMG_FIXED_DAMAGE, 0x16171617 };
    for (size_t i = 0; i < ARRAY_COUNT(excluded); ++i) {
        CHECK("other attack owners retain original flags",
              DinFireSword_DamageFlags(&play, &player, excluded[i]) == excluded[i]);
    }
    for (int sword = 0; sword < 3; ++sword) {
        setupSword(sword);
        transformed = 1;
        CHECK("transformation excluded",
              DinFireSword_DamageFlags(&play, &player, swordFlags[sword][0]) == swordFlags[sword][0]);
        transformed = 0;
        otherOwner = 1;
        CHECK("replacement weapon excluded",
              DinFireSword_DamageFlags(&play, &player, swordFlags[sword][0]) == swordFlags[sword][0]);
        otherOwner = 0;
        kokiriUpgrade = greatFairyUpgrade = 1;
        if (sword != 1) {
            CHECK("Kokiri and Great Fairy upgrades retain their own damage",
                  DinFireSword_DamageFlags(&play, &player, swordFlags[sword][0]) == swordFlags[sword][0]);
        }
        kokiriUpgrade = greatFairyUpgrade = 0;
        missing = "FlameTex";
        CHECK("missing resources exclude damage",
              DinFireSword_DamageFlags(&play, &player, swordFlags[sword][0]) == swordFlags[sword][0]);
    }
    setupSword(0);
    ColliderInfo* atInfo = setStrike(DMG_SLASH_KOKIRI);
    result = hit(ACTOR_EN_FZ, Test_FreezardTable(), 0xFFCFFFFF, atInfo);
    CHECK("inert Kokiri strike can burn Freezard",
          result.colChkInfo.damage == 4 && result.colChkInfo.damageEffect == 2);
    // An external owner reusing a player quad must not inherit cached sword
    // classification when it changes the attack flags.
    atInfo->toucher.dmgFlags = DMG_ARROW_FIRE;
    result = hit(ACTOR_EN_DEKUBABA, Test_DekuBabaTable(), 0xFFCFFFFF, atInfo);
    CHECK("replaced toucher flags invalidate sword ownership",
          result.colChkInfo.damage == 4 && result.colChkInfo.damageEffect == 2);
}

static void testNativeReactions(void) {
    s16 (*recoil[])(ColliderInfo*, Actor*) = {
        Test_WallmasterRecoil, Test_FloormasterRecoil, Test_DekuScrubRecoil,
        Test_PoeFieldRecoil,   Test_PoeRecoil,         Test_PoeSistersRecoil,
    };
    for (int sword = 0; sword < 3; ++sword) {
        for (int strike = 0; strike < 3; ++strike) {
            setupSword(sword);
            player.actor.world.pos.z = 100.0f;
            player.actor.world.rot.y = 0x1234;
            ColliderInfo* owned = setStrike(swordFlags[sword][strike]);
            ColliderInfo foreign = *owned;
            for (size_t actor = 0; actor < ARRAY_COUNT(recoil); ++actor) {
                CHECK("sword recoil points away from player", recoil[actor](owned, &player.actor) == (s16)0x8000);
                CHECK("foreign fire-arrow recoil keeps projectile yaw",
                      recoil[actor](&foreign, &player.actor) == 0x1234);
            }
            CHECK("sword breaks vanilla beehive", Test_BeehiveBreaks(&play, owned));
            CHECK("sword breaks shuffled beehive", Test_RandomizerBeehiveBreaks(&play, owned));
            CHECK("foreign fire flag retains vanilla hive shake", !Test_BeehiveBreaks(&play, &foreign));
            CHECK("foreign fire flag retains shuffled hive shake", !Test_RandomizerBeehiveBreaks(&play, &foreign));
            for (int collider = 0; collider < 2; ++collider) {
                CHECK("sword preserves Skulltula bounce death", Test_SkulltulaBounces(&play, owned, collider));
                CHECK("foreign fire flag preserves Skulltula arrow death",
                      !Test_SkulltulaBounces(&play, &foreign, collider));
            }
            CHECK("sword preserves Biri culling behavior", !Test_BiriPersistsOffscreen(&play, owned));
            CHECK("foreign fire flag preserves Biri arrow persistence", Test_BiriPersistsOffscreen(&play, &foreign));
            CHECK("airborne Phantom Ganon retains native sword damage",
                  Test_PhantomHealth(&play, owned, 0) == 10 - swordDamage[sword][strike]);
            CHECK("airborne Phantom Ganon still rejects foreign projectile",
                  Test_PhantomHealth(&play, &foreign, 0) == 10);
            CHECK("painting Phantom Ganon still admits added fire", Test_PhantomHealth(&play, owned, 1) == 8);
        }
    }
}

int main(void) {
    testSwordHits();
    testIsolation();
    testNativeReactions();
    if (failures) {
        fprintf(stderr, "%d fire-sword damage regression failures\n", failures);
        return 1;
    }
    puts("PASS Din fire sword damage: production collision, sword power/reactions/drops, fire interactions, toggles "
         "and owner isolation");
    return 0;
}
