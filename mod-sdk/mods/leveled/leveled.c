// RPG levels after Arrenton's Leveled (Shipwright PR #13). Damage is scaled in vanilla health units with a
// per-enemy remainder, so every vanilla health threshold keeps working without touching actor code.
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Boss_Dodongo/z_boss_dodongo.h"
#include "overlays/actors/ovl_Boss_Ganon2/z_boss_ganon2.h"
#include "overlays/actors/ovl_En_Ba/z_en_ba.h"
#include "overlays/gamestates/ovl_file_choose/file_choose.h"
#include "soh/ModApi/ModApi.h"
#include "soh/SaveManager.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define MOD_NAME "leveled"
#define CVAR(name) "gMods.Leveled." name
#define EXPERIENCE_KEY "experience"
#define BONUS_HEALTH_KEY "bonusHealth"

#define MAX_LEVEL 99
#define MAX_EXPERIENCE 999999
#define MAX_SHOWN_NUMBER 9999
#define MAX_TRACKED_ENEMIES 128
#define HEART_HEALTH 16
#define ENEMY_POWER_BONUS 1
#define DEFAULT_ENEMY_RATE 60
#define GANONS_CASTLE_MAXED_LEVEL 120
#define ALL_TOKENS 100
#define NEXT_LEVEL_SHOW_FRAMES 60
#define ATTACKER_MEMORY_FRAMES 3
#define NEAREST_ATTACKER_RANGE 250.0f
#define RED_HIT_FLASH 0x4000
#define NAVI_ENEMY_TEXT_FIRST 0x0600
#define NAVI_ENEMY_TEXT_LAST 0x06FF
#define MESSAGE_NEWLINE '\x01'
#define KALEIDO_STATE_OPEN 6
#define LEVEL_UP_PARTS 5
#define FILE_SLOTS 3
#define NO_SAVED_FILE -1
#define GANON_STRONG_WEAPONS 0x9000200
#define GANON_MASTER_SWORD 0x8000000
#define GANON_HEAD_ELEMENT 0
#define GANON_BODY_ELEMENT 15
#define FILE_SELECT_DEPTH 157.6f
#define FILE_SELECT_HALF_FOV 0.52359878f
#define FILE_LEVEL_X 53.5f
#define FILE_LEVEL_Y -16

typedef enum {
    HEALTH_FROM_COLLIDER,
    HEALTH_FROM_KING_DODONGO,
    HEALTH_FROM_PARENT,
} HealthSource;

typedef enum {
    NUMBERS_FULL,
    NUMBERS_CURRENT,
    NUMBERS_NONE,
} NumbersMode;

typedef enum {
    LEVEL_UP_TEXT,
    LEVEL_UP_POWER,
    LEVEL_UP_COURAGE,
    LEVEL_UP_HEARTS,
    LEVEL_UP_MAGIC,
} LevelUpPart;

typedef struct {
    u8 level;
    u16 experience;
    s8 zoneModifier;
    u16 zoneRate;
    u8 rewardShare;
    bool fixedLevel;
} EnemyRank;

typedef struct {
    s16 scene;
    s8 childLevel;
    s8 adultLevel;
    bool ganonsCastle;
} ZoneLevel;

typedef struct {
    u16 value;
    s16 life;
    f32 x;
    f32 y;
    f32 velocityX;
    f32 velocityY;
} FloatingPopup;

typedef struct {
    Actor* actor;
    u8 level;
    u16 experience;
    u8 rewardShare;
    s16 baseHealth;
    f32 healthUnitValue;
    f32 pendingUnits;
    s16 healthBeforeUpdate;
    u8 damageBeforeUpdate;
    u8 swordUnits;
    u32 hitFrame;
    s16 compensation;
    u8 compensationUnits;
    bool compensated;
    bool reachedZeroHealth;
    bool rewarded;
    FloatingPopup damagePopup;
} LeveledEnemy;

typedef struct {
    s8 power;
    s8 courage;
} StatModifiers;

typedef struct {
    const char* const* textures;
    s16 width;
    s16 height;
    s16 advance;
    bool intensity;
    bool shadowed;
} DigitFont;

typedef struct {
    u8 r;
    u8 g;
    u8 b;
} TextColor;

void Message_FindMessage(PlayState* play, u16 textId);
s16 getHealthMeterXOffset(void);
s16 getHealthMeterYOffset(void);
int16_t OTRGetRectDimensionFromLeftEdge(float value);
extern HOST_DATA s16 Left_HUD_Margin;
extern HOST_DATA s16 Top_HUD_Margin;

static const u32 sExperienceTable[MAX_LEVEL + 1] = {
    0,      30,     64,     105,    155,    217,    292,    385,    497,    632,    792,    981,    1202,
    1458,   1753,   2089,   2471,   2903,   3388,   3930,   4534,   5203,   5942,   6755,   7648,   8625,
    9691,   10851,  12110,  13474,  14949,  16540,  18253,  20094,  22070,  24188,  26453,  28874,  31456,
    34209,  37139,  40254,  43562,  47073,  50794,  54734,  58904,  63312,  67968,  72883,  78067,  83531,
    89286,  95345,  101719, 108421, 115463, 122859, 130623, 138770, 147314, 156270, 165655, 175485, 185778,
    196552, 207824, 219614, 231943, 244831, 258300, 272372, 287071, 302422, 318450, 335182, 352645, 370870,
    389885, 409724, 430419, 452005, 474520, 498001, 522489, 548027, 574659, 602434, 631400, 661611, 693122,
    725993, 760287, 796070, 833414, 872395, 913094, 955597, 999999,
};

static const ZoneLevel sZoneLevels[] = {
    { SCENE_DEKU_TREE, 2, 2, false },
    { SCENE_DEKU_TREE_BOSS, 2, 2, false },
    { SCENE_DODONGOS_CAVERN, 10, 10, false },
    { SCENE_DODONGOS_CAVERN_BOSS, 10, 10, false },
    { SCENE_JABU_JABU, 16, 16, false },
    { SCENE_JABU_JABU_BOSS, 16, 16, false },
    { SCENE_FOREST_TEMPLE, 24, 24, false },
    { SCENE_FOREST_TEMPLE_BOSS, 24, 24, false },
    { SCENE_FIRE_TEMPLE, 30, 30, false },
    { SCENE_FIRE_TEMPLE_BOSS, 30, 30, false },
    { SCENE_WATER_TEMPLE, 35, 35, false },
    { SCENE_WATER_TEMPLE_BOSS, 35, 35, false },
    { SCENE_SPIRIT_TEMPLE, 45, 45, false },
    { SCENE_SPIRIT_TEMPLE_BOSS, 45, 45, false },
    { SCENE_SHADOW_TEMPLE, 40, 40, false },
    { SCENE_SHADOW_TEMPLE_BOSS, 40, 40, false },
    { SCENE_BOTTOM_OF_THE_WELL, 38, 38, false },
    { SCENE_ICE_CAVERN, 32, 32, false },
    { SCENE_INSIDE_GANONS_CASTLE, 52, 52, true },
    { SCENE_GANONS_TOWER, 52, 52, true },
    { SCENE_GANONDORF_BOSS, 52, 52, true },
    { SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR, 52, 52, true },
    { SCENE_GANON_BOSS, 52, 52, true },
    { SCENE_GERUDO_TRAINING_GROUND, 43, 43, false },
    { SCENE_THIEVES_HIDEOUT, 42, 42, false },
    { SCENE_GANONS_TOWER_COLLAPSE_INTERIOR, 53, 53, false },
    { SCENE_INSIDE_GANONS_CASTLE_COLLAPSE, 53, 53, false },
    { SCENE_MARKET_RUINS, 23, 23, false },
    { SCENE_REDEAD_GRAVE, 10, 10, false },
    { SCENE_ROYAL_FAMILYS_TOMB, 8, 8, false },
    { SCENE_WINDMILL_AND_DAMPES_GRAVE, 22, 22, false },
    { SCENE_HOUSE_OF_SKULLTULA, 99, 99, false },
    { SCENE_HYRULE_FIELD, 5, 20, false },
    { SCENE_GRAVEYARD, 7, 20, false },
    { SCENE_ZORAS_RIVER, 8, 24, false },
    { SCENE_KOKIRI_FOREST, 2, 21, false },
    { SCENE_SACRED_FOREST_MEADOW, 9, 23, false },
    { SCENE_LAKE_HYLIA, 7, 22, false },
    { SCENE_ZORAS_FOUNTAIN, 13, 30, false },
    { SCENE_GERUDO_VALLEY, 10, 28, false },
    { SCENE_LOST_WOODS, 7, 22, false },
    { SCENE_DESERT_COLOSSUS, 42, 42, false },
    { SCENE_GERUDOS_FORTRESS, 40, 40, false },
    { SCENE_DEATH_MOUNTAIN_TRAIL, 8, 24, false },
    { SCENE_DEATH_MOUNTAIN_CRATER, 11, 11, false },
    { SCENE_LON_LON_RANCH, 6, 6, false },
    { SCENE_OUTSIDE_GANONS_CASTLE, 25, 25, false },
    { SCENE_TEST01, 45, 45, false },
    { SCENE_BESITU, 45, 45, false },
    { SCENE_DEPTH_TEST, 45, 45, false },
    { SCENE_SYOTES, 45, 45, false },
    { SCENE_SYOTES2, 45, 45, false },
    { SCENE_SUTARU, 45, 45, false },
    { SCENE_HAIRAL_NIWA2, 45, 45, false },
    { SCENE_SASATEST, 45, 45, false },
    { SCENE_TESTROOM, 45, 45, false },
};

static const char* const sCounterDigitTextures[] = {
    "__OTR__textures/parameter_static/gCounterDigit0Tex", "__OTR__textures/parameter_static/gCounterDigit1Tex",
    "__OTR__textures/parameter_static/gCounterDigit2Tex", "__OTR__textures/parameter_static/gCounterDigit3Tex",
    "__OTR__textures/parameter_static/gCounterDigit4Tex", "__OTR__textures/parameter_static/gCounterDigit5Tex",
    "__OTR__textures/parameter_static/gCounterDigit6Tex", "__OTR__textures/parameter_static/gCounterDigit7Tex",
    "__OTR__textures/parameter_static/gCounterDigit8Tex", "__OTR__textures/parameter_static/gCounterDigit9Tex",
};

static const char* const sAmmoDigitTextures[] = {
    "__OTR__textures/parameter_static/gAmmoDigit0Tex", "__OTR__textures/parameter_static/gAmmoDigit1Tex",
    "__OTR__textures/parameter_static/gAmmoDigit2Tex", "__OTR__textures/parameter_static/gAmmoDigit3Tex",
    "__OTR__textures/parameter_static/gAmmoDigit4Tex", "__OTR__textures/parameter_static/gAmmoDigit5Tex",
    "__OTR__textures/parameter_static/gAmmoDigit6Tex", "__OTR__textures/parameter_static/gAmmoDigit7Tex",
    "__OTR__textures/parameter_static/gAmmoDigit8Tex", "__OTR__textures/parameter_static/gAmmoDigit9Tex",
};

#define FONT_TEXTURE(name) "__OTR__textures/nes_font_static/gMsgChar" name "Tex"
#define ITEM_ICON_TEXTURE(name) "__OTR__textures/icon_item_static/gItemIcon" name "Tex"

static const char* const sLevelUpLetters[] = {
    FONT_TEXTURE("4CLatinCapitalLetterL"), FONT_TEXTURE("45LatinCapitalLetterE"), FONT_TEXTURE("56LatinCapitalLetterV"),
    FONT_TEXTURE("45LatinCapitalLetterE"), FONT_TEXTURE("4CLatinCapitalLetterL"), NULL,
    FONT_TEXTURE("55LatinCapitalLetterU"), FONT_TEXTURE("50LatinCapitalLetterP"),
};

static const char sSlashTexture[] = FONT_TEXTURE("2FSolidus");
static const char sCapitalLTexture[] = FONT_TEXTURE("4CLatinCapitalLetterL");
static const char sSmallVTexture[] = FONT_TEXTURE("76LatinSmallLetterV");
static const char sPowerIconTexture[] = ITEM_ICON_TEXTURE("SilverGauntlets");
static const char sCourageIconTexture[] = ITEM_ICON_TEXTURE("ShieldHylian");
static const char sAttackIconTexture[] = ITEM_ICON_TEXTURE("SwordKokiri");
static const char sExperienceIconTexture[] = ITEM_ICON_TEXTURE("GoronsBracelet");
static const char sMagicIconTexture[] = "__OTR__textures/icon_item_24_static/gQuestIconMagicJarBigTex";
static const char sHeartTexture[] = "__OTR__textures/parameter_static/gHeartFullTex";
static const char sNextTexture[] = "__OTR__textures/do_action_static/gNextDoActionENGTex";

static const DigitFont sBigDigits = { sCounterDigitTextures, 8, 16, 8, true, true };
static const DigitFont sSmallDigits = { sAmmoDigitTextures, 8, 8, 6, false, false };

static const TextColor sWhite = { 255, 255, 255 };
static const TextColor sCriticalOrange = { 255, 127, 0 };
static const TextColor sMaximumGreen = { 120, 255, 0 };
static const TextColor sPenaltyRed = { 255, 0, 0 };
static const TextColor sLinkDamageOrange = { 255, 100, 0 };
static const TextColor sExperienceYellow = { 190, 190, 0 };
static const TextColor sHeartRed = { 255, 70, 0 };

static const char* const sRequiredHooks[] = {
    "OnLoadFile",           "OnActorInit",
    "OnActorDestroy",       "ShouldActorUpdate",
    "OnActorUpdate",        "OnCollisionResolveDamage",
    "OnResolveSwordDamage", "OnEnemyDefeat",
    "OnBossDefeat",         "OnFlagSet",
    "OnPlayerHealthChange", "OnInterfaceUpdate",
    "OnInterfaceDrawEnd",   "OnOpenText",
    "OnSaveFile",           "OnFileChooseMain",
    "OnSceneInit",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static LeveledEnemy sEnemies[MAX_TRACKED_ENEMIES];
static s32 sEnemyCount;
static Actor* sUpdatingActor;
static u32 sExperience;
static u8 sLevel = 1;
static s16 sBonusHealth;
static s16 sNextLevelTimer;
static u8 sRecentAttackerPower;
static u32 sRecentAttackFrame;
static bool sHasRecentAttacker;
static FloatingPopup sLinkDamagePopup;
static FloatingPopup sExperiencePopup;
static FloatingPopup sLevelUpPopups[LEVEL_UP_PARTS];
static volatile s32 sSavedFile = NO_SAVED_FILE;
static volatile u8 sSavedLevel;

static u8 GetAttackHealthMultiplier(void) {
    return CLAMP(CVarGetInteger(CVAR("AttackHealthMultiplier"), 9), 1, 32);
}

static u8 GetEnemyPower(u8 level) {
    return 3 + (u8)(84 * level / 99.0f);
}

static u8 GetEnemyCourage(u8 level) {
    return 2 + (u8)(75 * level / 99.0f);
}

static u8 GetLinkBasePower(u8 level) {
    return 2 + (u8)(75 * level / 99.0f);
}

static u8 GetLinkBaseCourage(u8 level) {
    return 3 + (u8)(84 * level / 99.0f);
}

static f32 ScaleAttack(f32 attack, s32 power) {
    const f32 steps = power - 2;
    return attack * (1.0f + steps * (0.14f + steps * 0.0006f));
}

static f32 ScaleEnemyAttack(f32 attack, s32 power) {
    const f32 steps = power - 2;
    return attack * (1.0f + steps * (0.007f + steps * 0.0002f));
}

static f32 ComputeDamageOnEnemy(f32 attack, s32 power, s32 courage) {
    const f32 damage = (u16)ScaleAttack(attack, power);
    if (power >= courage) {
        return damage * powf(1.01f, (f32)(power - courage));
    }
    return damage * powf(0.97f, (f32)(courage - power));
}

static f32 ComputeDamageOnLink(f32 attack, s32 power, s32 courage) {
    const bool scalesWithLevel = CVarGetInteger(CVAR("EnemyAttackScales"), 1);
    f32 damage = scalesWithLevel ? ScaleEnemyAttack(attack, power) : attack;

    if (power < courage) {
        return damage * powf(0.96f, (f32)(courage - power));
    }
    for (s32 step = 0; step < power - courage; step++) {
        if (scalesWithLevel) {
            damage *= 1.04f + MAX((0.07f - power * 0.0005f) - step / (100.0f - power * 0.33f), 0.0f);
        } else {
            damage *= 1.05f + MAX(0.05f - step / 100.0f, 0.0f);
        }
    }
    return damage;
}

static u16 RoundDamage(f32 damage, f32 attack) {
    f32 varied = damage;
    if (varied >= 1.25f) {
        varied += Rand_ZeroOne() - 0.2f;
    }
    if (varied >= 6.0f) {
        varied += Rand_ZeroFloat(varied * 0.12f) - varied * 0.06f;
    }
    const u16 rounded = (u16)(MIN(varied, (f32)MAX_SHOWN_NUMBER) + 0.5f);
    return (attack > 0.0f) ? MAX(rounded, 1) : rounded;
}

static u16 ComputeEnemyExperience(u8 level, u16 rate) {
    if (rate == 0) {
        return 0;
    }
    const f64 levelBonus = MIN((level / 6) * 4, 4);
    const f64 growth = (0.1 + level / 95.0) + pow(MAX(level - 8, 0), 1.25) / 80.0;
    const f64 experience = (3 + levelBonus + (level - 1) * growth) * rate / 100.0;
    return (u16)CLAMP(round(experience), 1, MAX_SHOWN_NUMBER);
}

static u32 ComputeTokenExperience(u8 tokens) {
    u16 experience = 5;
    for (u8 token = 0; token < tokens; token++) {
        experience = (u16)(experience + 5 + 5 * token / 10.0f);
    }
    return (u32)(experience * CVarGetFloat(CVAR("TokenExperienceRate"), 1.0f));
}

static u32 ApplyExperienceRate(u32 experience) {
    return (u32)roundf(experience * CVarGetFloat(CVAR("ExperienceRate"), 1.0f));
}

static u8 GetLevelForExperience(u32 experience) {
    u8 level = 0;
    while (level < MAX_LEVEL && sExperienceTable[level] <= experience) {
        level++;
    }
    return level;
}

static u32 GetExperienceToNextLevel(void) {
    if (sLevel >= MAX_LEVEL) {
        return 0;
    }
    return sExperienceTable[sLevel] - MIN(sExperience, sExperienceTable[sLevel]);
}

static u32 GetExperienceSpanOfLevel(u8 level) {
    if (level == 0 || level >= MAX_LEVEL) {
        return 0;
    }
    return sExperienceTable[level] - sExperienceTable[level - 1];
}

static s16 GetBonusHealth(u8 level) {
    if (!CVarGetInteger(CVAR("LevelHearts"), 1)) {
        return 0;
    }
    return MIN((level + 1) / 8, 10) * HEART_HEALTH;
}

static s16 GetMagicUnits(u8 level) {
    if (!CVarGetInteger(CVAR("LevelMagic"), 1)) {
        return MAGIC_NORMAL_METER;
    }
    return MIN(12 + (s16)(level / 2.8f) * 2, 72);
}

static bool IsGiantsKnifeBroken(void) {
    return (gSaveContext.inventory.equipment & (1 << EQUIP_INV_SWORD_BROKENGIANTKNIFE)) != 0;
}

static void AddSwordModifiers(StatModifiers* modifiers) {
    switch (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)) {
        case EQUIP_VALUE_SWORD_MASTER:
            modifiers->courage += 1;
            break;
        case EQUIP_VALUE_SWORD_BIGGORON:
            modifiers->power += IsGiantsKnifeBroken() ? -7 : 2;
            modifiers->courage += IsGiantsKnifeBroken() ? -12 : -8;
            break;
        default:
            break;
    }
}

static void AddClothingModifiers(StatModifiers* modifiers) {
    switch (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)) {
        case EQUIP_VALUE_TUNIC_GORON:
            modifiers->power += 3;
            modifiers->courage -= 3;
            break;
        case EQUIP_VALUE_TUNIC_ZORA:
            modifiers->power -= 3;
            modifiers->courage += 3;
            break;
        default:
            break;
    }
    switch (CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS)) {
        case EQUIP_VALUE_BOOTS_IRON:
            modifiers->power += 2;
            modifiers->courage += 1;
            break;
        case EQUIP_VALUE_BOOTS_HOVER:
            modifiers->courage -= 1;
            break;
        default:
            break;
    }
}

static void AddShieldModifiers(StatModifiers* modifiers) {
    switch (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) {
        case EQUIP_VALUE_SHIELD_DEKU:
            modifiers->courage += 1;
            break;
        case EQUIP_VALUE_SHIELD_HYLIAN:
            modifiers->courage += 2;
            break;
        case EQUIP_VALUE_SHIELD_MIRROR:
            modifiers->power -= 2;
            modifiers->courage += 3;
            break;
        default:
            break;
    }
}

static StatModifiers GetEquipmentModifiers(void) {
    StatModifiers modifiers = { 0, 0 };
    if (!CVarGetInteger(CVAR("EquipmentStats"), 1)) {
        return modifiers;
    }
    AddSwordModifiers(&modifiers);
    AddClothingModifiers(&modifiers);
    AddShieldModifiers(&modifiers);
    modifiers.power += Player_GetStrength();
    return modifiers;
}

static u8 GetLinkPower(void) {
    return CLAMP(GetLinkBasePower(sLevel) + GetEquipmentModifiers().power, 0, 255);
}

static u8 GetLinkCourage(void) {
    return CLAMP(GetLinkBaseCourage(sLevel) + GetEquipmentModifiers().courage, 0, 255);
}

static s32 GetZoneLevel(s16 scene) {
    for (u32 i = 0; i < ARRAY_COUNT(sZoneLevels); i++) {
        const ZoneLevel* zone = &sZoneLevels[i];
        if (zone->scene != scene) {
            continue;
        }
        const bool castleMaxed =
            CVarGetInteger(CVAR("MaxedGanonsCastle"), 0) && gSaveContext.inventory.gsTokens >= ALL_TOKENS;
        if (zone->ganonsCastle && castleMaxed) {
            return GANONS_CASTLE_MAXED_LEVEL;
        }
        return LINK_IS_ADULT ? zone->adultLevel : zone->childLevel;
    }
    return -1;
}

static u8 GetLocalLevel(PlayState* play) {
    const s32 zoneLevel = GetZoneLevel(play->sceneNum);
    return (zoneLevel >= 0) ? MIN(zoneLevel, MAX_LEVEL) : sLevel;
}

static EnemyRank Rank(u8 level, u16 experience, s8 zoneModifier, u16 zoneRate) {
    EnemyRank rank = { level, experience, zoneModifier, zoneRate, 1, false };
    return rank;
}

static EnemyRank RankAdult(EnemyRank childRank, u8 adultLevel, u16 adultExperience) {
    if (LINK_IS_ADULT) {
        childRank.level = adultLevel;
        childRank.experience = adultExperience;
    }
    return childRank;
}

static EnemyRank RankShared(EnemyRank rank, u8 rewardShare) {
    rank.rewardShare = rewardShare;
    return rank;
}

static EnemyRank RankFixed(u8 level, u16 experience) {
    EnemyRank rank = Rank(level, experience, 0, 0);
    rank.fixedLevel = true;
    return rank;
}

static bool RankBoss(Actor* actor, EnemyRank* rank) {
    const s16 params = actor->params;
    switch (actor->id) {
        case ACTOR_BOSS_GOMA:
            *rank = Rank(7, 120, 3, 3000);
            return true;
        case ACTOR_BOSS_DODONGO:
            *rank = Rank(15, 500, 5, 4000);
            return true;
        case ACTOR_BOSS_VA:
            *rank = (params == -1) ? Rank(21, 850, 5, 5000) : Rank(21, 0, 5, 0);
            return true;
        case ACTOR_BOSS_GANONDROF:
            *rank = Rank(28, (params == 1) ? 1400 : 0, 5, 6000);
            return true;
        case ACTOR_BOSS_FD:
        case ACTOR_BOSS_FD2:
            *rank = Rank(35, (actor->id == ACTOR_BOSS_FD2) ? 2475 : 0, 5, 6300);
            return true;
        case ACTOR_BOSS_MO:
            *rank = Rank(40, 3500, 5, 6600);
            return true;
        case ACTOR_BOSS_SST:
            *rank = Rank(45, 4666, 5, 7000);
            return true;
        case ACTOR_BOSS_TW:
            *rank = Rank(52, (params == 2) ? 6000 : 0, 6, 7000);
            return true;
        case ACTOR_BOSS_GANON:
            *rank = Rank(56, 9999, 5, 40000);
            return true;
        case ACTOR_BOSS_GANON2:
            *rank = Rank(60, 0, 8, 0);
            return true;
        default:
            return false;
    }
}

static bool RankMiniboss(PlayState* play, Actor* actor, EnemyRank* rank) {
    const s16 params = actor->params;
    switch (actor->id) {
        case ACTOR_EN_BIGOKUTA:
            *rank = Rank(20, 320, 4, 2100);
            return true;
        case ACTOR_EN_FD:
        case ACTOR_EN_FW:
        case ACTOR_EN_FD_FIRE:
            *rank = (actor->id == ACTOR_EN_FW) ? Rank(34, 1000, 4, 3000) : Rank(34, 0, 4, 0);
            return true;
        case ACTOR_EN_TORCH2:
            *rank = RankFixed(sLevel, (u16)(GetExperienceSpanOfLevel(sLevel) / 2));
            return true;
        case ACTOR_EN_DH:
        case ACTOR_EN_DHA:
            *rank = (actor->id == ACTOR_EN_DH) ? Rank(44, 2465, 4, 3666) : Rank(44, 0, 4, 0);
            return true;
        case ACTOR_EN_IK:
            *rank = (params == 0) ? Rank(52, 3100, 6, 5000) : Rank(50, 2733, 5, 5000);
            return true;
        case ACTOR_EN_ZF:
            if (params == -2) {
                *rank = Rank(52, 250, 4, 320);
            } else if (params == -1) {
                *rank = Rank(47, 210, 3, 290);
            } else {
                *rank = RankShared(Rank(13, 140, 3, 1150), 2);
            }
            return true;
        case ACTOR_EN_TEST:
            *rank =
                (play->sceneNum == SCENE_FOREST_TEMPLE) ? RankShared(Rank(27, 250, 4, 800), 2) : Rank(27, 64, 4, 345);
            return true;
        case ACTOR_EN_GELDB:
            *rank = Rank(45, 455, 2, 1250);
            return true;
        default:
            return false;
    }
}

static bool RankForestCreature(Actor* actor, EnemyRank* rank) {
    const s16 params = actor->params;
    switch (actor->id) {
        case ACTOR_EN_GOMA:
            *rank = (params < 10) ? Rank(5, 5, 1, 100) : Rank(5, 0, 1, 0);
            return true;
        case ACTOR_EN_DEKUBABA:
            *rank = (params == 1) ? RankAdult(Rank(5, 8, 1, 116), 24, 32) : Rank(2, 3, -2, 60);
            return true;
        case ACTOR_EN_KAREBABA:
            *rank = Rank(1, 1, -99, 1);
            return true;
        case ACTOR_EN_ST:
            *rank = (params == 1) ? Rank(6, 5, 1, 90) : Rank(3, 3, 0, 80);
            return true;
        case ACTOR_EN_SW:
            if (((params & 0xE000) >> 0xD) != 0) {
                *rank = RankFixed(gSaveContext.inventory.gsTokens + 1, 0);
            } else {
                *rank = Rank(3, 2, -2, 40);
            }
            return true;
        case ACTOR_EN_DEKUNUTS:
            *rank = Rank(8, 7, 0, 75);
            return true;
        case ACTOR_EN_TITE:
            *rank = (params == -2) ? RankAdult(Rank(12, 12, 2, 100), 25, 21) : RankAdult(Rank(9, 9, 1, 90), 27, 24);
            return true;
        case ACTOR_EN_CROW:
            *rank = RankAdult(Rank(8, 4, -1, 50), 21, 13);
            return true;
        case ACTOR_EN_OKUTA:
            *rank = (params == 0) ? Rank(13, 12, 0, 70) : Rank(0, 0, 0, 0);
            return true;
        case ACTOR_EN_WF:
            *rank = (params == 0) ? RankAdult(Rank(10, 24, 1, 180), 20, 29) : Rank(37, 126, 3, 180);
            return true;
        case ACTOR_EN_SKB:
            *rank = Rank(6 + params / 2, (u16)(3 + params / 2.5), 1 + params / 4, 45 + params * 3);
            return true;
        default:
            return false;
    }
}

static bool RankDungeonCreature(Actor* actor, EnemyRank* rank) {
    const s16 params = actor->params;
    switch (actor->id) {
        case ACTOR_EN_DODONGO:
            *rank = Rank(14, 18, 4, 160);
            return true;
        case ACTOR_EN_AM:
            *rank = Rank(12, 17, 2, 135);
            return true;
        case ACTOR_EN_DODOJR:
            *rank = Rank(11, 7, 1, 75);
            return true;
        case ACTOR_EN_FIREFLY:
            *rank = (params == 4) ? Rank(34, 33, 1, 60) : Rank(9, 5, -1, 50);
            return true;
        case ACTOR_EN_PEEHAT:
            *rank = (params == 1) ? Rank(13, 47, 5, 0) : Rank(18, 47, 10, 275);
            return true;
        case ACTOR_EN_POH:
            *rank = (params == 2 || params == 3) ? Rank(15, 25, 1, 130) : Rank(10, 14, 0, 130);
            return true;
        case ACTOR_EN_PO_FIELD:
            *rank = Rank(24, 25, 0, 130);
            return true;
        case ACTOR_EN_RD:
            *rank = (params >= -1) ? Rank(25, 45, 2, 280) : Rank(43, 183, 3, 305);
            return true;
        case ACTOR_EN_BUBBLE:
            *rank = Rank(15, 0, -1, 0);
            return true;
        case ACTOR_EN_VALI:
            *rank = Rank(18, 25, 2, 85);
            return true;
        case ACTOR_EN_BILI:
            *rank = Rank(16, 17, 1, 100);
            return true;
        case ACTOR_EN_EIYER:
        case ACTOR_EN_WEIYER:
            *rank = Rank(17, 19, 1, 125);
            return true;
        case ACTOR_EN_BA:
        case ACTOR_EN_BX:
            *rank = (params < 3) ? Rank(19, 61, 3, 400) : Rank(19, 0, 3, 0);
            return true;
        case ACTOR_EN_TP:
            *rank = Rank(19, 22, 3, 125);
            return true;
        case ACTOR_EN_VM:
            *rank = (params == 0) ? Rank(48, 90, 1, 150) : Rank(11, 17, 0, 150);
            return true;
        default:
            return false;
    }
}

static EnemyRank RankBubble(s16 params) {
    switch (params) {
        case -1:
            return Rank(25, 29, 1, 105);
        case -2:
            return Rank(32, 47, 1, 110);
        case -3:
            return Rank(45, 76, 1, 107);
        case -4:
            return Rank(41, 82, 2, 100);
        default:
            return Rank(26, 31, 3, 93);
    }
}

static EnemyRank RankMoblin(s16 params) {
    switch (params) {
        case -1:
            return Rank(24, 32, 2, 140);
        case 0:
            return Rank(25, 47, 3, 240);
        default:
            return Rank(24, 24, 2, 140);
    }
}

static bool RankAdultCreature(Actor* actor, EnemyRank* rank) {
    switch (actor->id) {
        case ACTOR_EN_MB:
            *rank = RankMoblin(actor->params);
            return true;
        case ACTOR_EN_BB:
            *rank = RankBubble(actor->params);
            return true;
        case ACTOR_EN_FLOORMAS:
            *rank = RankShared(Rank(26, 41, 2, 170), 3);
            return true;
        case ACTOR_EN_WALLMAS:
            *rank = Rank(27, 44, 2, 175);
            return true;
        case ACTOR_EN_PO_SISTERS:
            *rank = Rank(28, 127, 4, 450);
            return true;
        case ACTOR_EN_RR:
            *rank = Rank(32, 99, 2, 245);
            return true;
        case ACTOR_EN_BW:
            *rank = Rank(33, 67, 1, 135);
            return true;
        case ACTOR_EN_FZ:
            *rank = Rank(35, 101, 3, 205);
            return true;
        case ACTOR_EN_SB:
            *rank = Rank(38, 129, 2, 200);
            return true;
        case ACTOR_EN_NY:
            *rank = Rank(37, 100, 1, 180);
            return true;
        case ACTOR_EN_REEBA:
            *rank = Rank(45, 15, 0, 20);
            return true;
        case ACTOR_EN_ANUBICE:
        case ACTOR_EN_ANUBICE_FIRE:
            *rank = (actor->id == ACTOR_EN_ANUBICE) ? Rank(48, 120, 3, 155) : Rank(48, 0, 3, 0);
            return true;
        case ACTOR_EN_CLEAR_TAG:
            *rank = Rank(20, 50, 3, 215);
            return true;
        default:
            return false;
    }
}

static bool RankEnemy(PlayState* play, Actor* actor, EnemyRank* rank) {
    if (RankBoss(actor, rank) || RankMiniboss(play, actor, rank) || RankForestCreature(actor, rank) ||
        RankDungeonCreature(actor, rank) || RankAdultCreature(actor, rank)) {
        return true;
    }
    if (actor->category != ACTORCAT_ENEMY && actor->category != ACTORCAT_BOSS) {
        return false;
    }
    *rank = Rank(sLevel, ComputeEnemyExperience(sLevel, DEFAULT_ENEMY_RATE), 0, DEFAULT_ENEMY_RATE);
    return true;
}

static void ApplyZoneLevel(PlayState* play, EnemyRank* rank) {
    const s32 zoneLevel = GetZoneLevel(play->sceneNum);
    if (rank->fixedLevel || zoneLevel < 0) {
        return;
    }
    const f32 scaledLevel = (zoneLevel + rank->zoneModifier) * CVarGetFloat(CVAR("LevelScale"), 1.0f);
    rank->level = (u8)CLAMP(scaledLevel, 1.0f, (f32)MAX_LEVEL);
    rank->experience = ComputeEnemyExperience(rank->level, rank->zoneRate);
}

static f32 GetHealthUnitValue(u8 level) {
    const f32 healthScale = MAX(CVarGetFloat(CVAR("EnemyHealth"), 1.0f), 0.1f);
    return MAX(ScaleAttack(GetAttackHealthMultiplier(), GetLinkBasePower(level)) * healthScale, 0.01f);
}

static LeveledEnemy* FindEnemy(const void* actor) {
    if (actor == NULL) {
        return NULL;
    }
    for (s32 i = 0; i < sEnemyCount; i++) {
        if (sEnemies[i].actor == actor) {
            return &sEnemies[i];
        }
    }
    return NULL;
}

static u32 GetDisplayedMaxHealth(const LeveledEnemy* enemy) {
    return (u32)(enemy->baseHealth * enemy->healthUnitValue);
}

static HealthSource GetHealthSource(const Actor* actor) {
    switch (actor->id) {
        case ACTOR_BOSS_DODONGO:
            return HEALTH_FROM_KING_DODONGO;
        case ACTOR_BOSS_FD2:
            return (actor->parent != NULL) ? HEALTH_FROM_PARENT : HEALTH_FROM_COLLIDER;
        default:
            return HEALTH_FROM_COLLIDER;
    }
}

static s32 ReadEnemyHealth(const Actor* actor) {
    switch (GetHealthSource(actor)) {
        case HEALTH_FROM_KING_DODONGO:
            return ((const BossDodongo*)actor)->health;
        case HEALTH_FROM_PARENT:
            return actor->parent->colChkInfo.health;
        default:
            return actor->colChkInfo.health;
    }
}

static void WriteEnemyHealth(Actor* actor, s32 health) {
    switch (GetHealthSource(actor)) {
        case HEALTH_FROM_KING_DODONGO:
            ((BossDodongo*)actor)->health = (s16)health;
            break;
        case HEALTH_FROM_PARENT:
            actor->parent->colChkInfo.health = (u8)CLAMP(health, 0, 255);
            break;
        default:
            actor->colChkInfo.health = (u8)CLAMP(health, 0, 255);
            break;
    }
}

static u8 GetAppliedColliderDamage(const Actor* actor) {
    return (GetHealthSource(actor) == HEALTH_FROM_COLLIDER) ? actor->colChkInfo.damage : 0;
}

static void TrackEnemy(void* actorRef) {
    Actor* actor = actorRef;
    PlayState* play = gPlayState;
    EnemyRank rank;
    if (play == NULL || sEnemyCount >= MAX_TRACKED_ENEMIES || !RankEnemy(play, actor, &rank)) {
        return;
    }
    ApplyZoneLevel(play, &rank);

    LeveledEnemy* enemy = &sEnemies[sEnemyCount++];
    memset(enemy, 0, sizeof(*enemy));
    enemy->actor = actor;
    enemy->level = rank.level;
    enemy->experience = rank.experience;
    enemy->rewardShare = MAX(rank.rewardShare, 1);
    enemy->baseHealth = ReadEnemyHealth(actor);
    enemy->healthUnitValue = GetHealthUnitValue(rank.level);
}

static void StartPopup(FloatingPopup* popup, u16 value, s16 life, f32 velocityY) {
    popup->value = value;
    popup->life = life;
    popup->x = 0.0f;
    popup->y = 0.0f;
    popup->velocityX = 0.0f;
    popup->velocityY = velocityY;
}

static void StartDamagePopup(FloatingPopup* popup, u16 damage, const char* enabledCvar) {
    if (damage == 0 || !CVarGetInteger(enabledCvar, 1)) {
        return;
    }
    StartPopup(popup, damage, 18, -11.0f);
}

static bool IsLinksOwnActor(const Actor* actor) {
    const bool linksCategory = actor->category == ACTORCAT_PLAYER || actor->category == ACTORCAT_EXPLOSIVE ||
                               actor->category == ACTORCAT_ITEMACTION;
    return linksCategory || (actor->parent != NULL && actor->parent->category == ACTORCAT_PLAYER);
}

static u8 GetAttackerPower(PlayState* play, Actor* attacker) {
    LeveledEnemy* enemy = FindEnemy(attacker);
    if (enemy == NULL && attacker != NULL) {
        enemy = FindEnemy(attacker->parent);
    }
    if (enemy != NULL) {
        return CLAMP(GetEnemyPower(enemy->level) + ENEMY_POWER_BONUS, 0, 255);
    }
    if (attacker == NULL || IsLinksOwnActor(attacker)) {
        return GetLinkPower();
    }
    return GetEnemyPower(GetLocalLevel(play)) + ENEMY_POWER_BONUS;
}

static s32 ConvertHitToHealthUnits(LeveledEnemy* enemy, u8 attackerPower, f32 vanillaDamage) {
    const f32 attack = vanillaDamage * GetAttackHealthMultiplier();
    const f32 damage = ComputeDamageOnEnemy(attack, attackerPower, GetEnemyCourage(enemy->level));
    const u16 dealt = RoundDamage(damage, attack);

    StartDamagePopup(&enemy->damagePopup, dealt, CVAR("Popup.EnemyDamage"));
    enemy->pendingUnits += dealt / enemy->healthUnitValue;
    const s32 units = (enemy->pendingUnits >= 1.0f) ? (s32)enemy->pendingUnits : 0;
    enemy->pendingUnits -= units;
    return units;
}

static void KeepHit(LeveledEnemy* enemy, ColliderInfo* attack) {
    enemy->hitFrame = gPlayState->gameplayFrames;
    attack->atHit->acFlags |= AC_HIT;
    attack->atHitInfo->bumperFlags |= BUMP_HIT;
}

// A hit too weak to take a whole health unit is hidden from the actor: one-hit enemies (Keese, Guay, Biri)
// would otherwise run their death on any hit.
static void AbsorbHit(LeveledEnemy* enemy, ColliderInfo* attack) {
    attack->atHitInfo->bumperFlags &= ~BUMP_HIT;
    if (enemy->hitFrame != gPlayState->gameplayFrames) {
        attack->atHit->acFlags &= ~AC_HIT;
        enemy->actor->colChkInfo.damageEffect = 0;
    }
    Actor_SetColorFilter(enemy->actor, RED_HIT_FLASH, 255, 0, 8);
}

static void RememberAttacker(PlayState* play, Actor* attacker) {
    if (attacker == NULL || IsLinksOwnActor(attacker)) {
        return;
    }
    sRecentAttackerPower = GetAttackerPower(play, attacker);
    sRecentAttackFrame = play->gameplayFrames;
    sHasRecentAttacker = true;
}

static s32 PredictGanonHit(BossGanon2* ganon, const ColliderInfo* attack) {
    const s32 flags = attack->toucher.dmgFlags;
    const bool strongWeapon = (flags & GANON_STRONG_WEAPONS) != 0;
    if (ganon->unk_313) {
        const bool headHit = attack->atHitInfo == &ganon->unk_424.elements[GANON_HEAD_ELEMENT].info;
        return (headHit && strongWeapon) ? 2 : 0;
    }
    if (attack->atHitInfo != &ganon->unk_424.elements[GANON_BODY_ELEMENT].info) {
        return 0;
    }
    if (!strongWeapon) {
        return 1;
    }
    return (flags & GANON_MASTER_SWORD) ? 4 : 2;
}

// These actors subtract a fixed amount and check for death in the same update, so their health is set ahead of
// the hit; -1 means the actor reads its collider damage like any other.
static s32 PredictScriptedHit(LeveledEnemy* enemy, const ColliderInfo* attack, const Actor* attacker) {
    Actor* actor = enemy->actor;
    switch (actor->id) {
        case ACTOR_BOSS_GANON2:
            return PredictGanonHit((BossGanon2*)actor, attack);
        case ACTOR_EN_BA:
            return (actor->params < EN_BA_DEAD_BLOB) ? 1 : 0;
        case ACTOR_EN_CLEAR_TAG:
            return 1;
        case ACTOR_EN_VM:
            return (attacker != NULL && attacker->category == ACTORCAT_EXPLOSIVE) ? 1 : 0;
        default:
            return -1;
    }
}

static bool UsesColliderDamage(s16 actorId) {
    switch (actorId) {
        case ACTOR_BOSS_GOMA:
        case ACTOR_BOSS_DODONGO:
        case ACTOR_BOSS_GANONDROF:
        case ACTOR_BOSS_FD:
        case ACTOR_BOSS_FD2:
        case ACTOR_BOSS_MO:
        case ACTOR_BOSS_TW:
        case ACTOR_BOSS_GANON:
        case ACTOR_EN_GOMA:
            return false;
        default:
            return true;
    }
}

static void ResolveScriptedHit(LeveledEnemy* enemy, ColliderInfo* attack, Actor* attacker, s32 scriptedDamage) {
    if (scriptedDamage == 0 || enemy->compensated) {
        return;
    }
    const s32 units = ConvertHitToHealthUnits(enemy, GetAttackerPower(gPlayState, attacker), (f32)scriptedDamage);
    if (units == 0) {
        AbsorbHit(enemy, attack);
        return;
    }
    const s32 health = ReadEnemyHealth(enemy->actor);
    const s32 healthBeforeHit = MAX(health - units, 0) + scriptedDamage;
    enemy->compensation = (s16)(healthBeforeHit - health);
    enemy->compensationUnits = (u8)MIN(units, 255);
    enemy->compensated = true;
    WriteEnemyHealth(enemy->actor, healthBeforeHit);
    KeepHit(enemy, attack);
}

static void SettleCompensation(LeveledEnemy* enemy) {
    if (!enemy->compensated) {
        return;
    }
    enemy->compensated = false;
    if (ReadEnemyHealth(enemy->actor) != enemy->healthBeforeUpdate) {
        return;
    }
    WriteEnemyHealth(enemy->actor, enemy->healthBeforeUpdate - enemy->compensation);
    enemy->pendingUnits += enemy->compensationUnits;
}

static void ResolveCollisionDamage(Actor* victim, ColliderInfo* attack, float* damage) {
    PlayState* play = gPlayState;
    if (play == NULL || victim == NULL || attack == NULL || attack->atHit == NULL || attack->atHitInfo == NULL) {
        return;
    }
    Actor* attacker = attack->atHit->ac;
    if (victim->category == ACTORCAT_PLAYER) {
        RememberAttacker(play, attacker);
        return;
    }
    LeveledEnemy* enemy = FindEnemy(victim);
    if (enemy == NULL || (attack->atHit->acFlags & AC_HARD)) {
        return;
    }
    const s32 scriptedDamage = PredictScriptedHit(enemy, attack, attacker);
    if (scriptedDamage >= 0) {
        ResolveScriptedHit(enemy, attack, attacker, scriptedDamage);
        return;
    }
    if (!UsesColliderDamage(victim->id) || *damage <= 0.0f) {
        return;
    }
    const s32 units = ConvertHitToHealthUnits(enemy, GetAttackerPower(play, attacker), *damage);
    if (units > 0) {
        *damage = (float)units;
        KeepHit(enemy, attack);
        return;
    }
    *damage = 0.0f;
    AbsorbHit(enemy, attack);
}

// Bosses read a zero sword damage as a non-sword hit, so a weak hit borrows its unit from the next ones.
static void ResolveSwordDamage(PlayState* play, int32_t dmgFlags, uint8_t* damage) {
    LeveledEnemy* enemy = FindEnemy(sUpdatingActor);
    if (enemy == NULL || *damage == 0) {
        return;
    }
    s32 units = ConvertHitToHealthUnits(enemy, GetLinkPower(), *damage);
    if (units == 0) {
        units = 1;
        enemy->pendingUnits -= 1.0f;
    }
    enemy->swordUnits = (u8)MIN(enemy->swordUnits + units, 255);
    *damage = (uint8_t)MIN(units, 255);
}

// Health these actors lose outside any damage hook (bombs, hammer, arrows, deflected balls). None of them can die
// from it within that update, so the loss is rescaled afterwards.
static bool HasUnhookedHealthLoss(s16 actorId) {
    switch (actorId) {
        case ACTOR_BOSS_DODONGO:
        case ACTOR_BOSS_FD:
        case ACTOR_BOSS_FD2:
        case ACTOR_BOSS_GANON:
        case ACTOR_BOSS_GANONDROF:
        case ACTOR_BOSS_TW:
        case ACTOR_EN_GOMA:
            return true;
        default:
            return false;
    }
}

static void SnapshotBeforeUpdate(void* actorRef, bool* shouldUpdate) {
    sUpdatingActor = actorRef;
    LeveledEnemy* enemy = FindEnemy(actorRef);
    if (enemy == NULL) {
        return;
    }
    Actor* actor = enemy->actor;
    const s32 health = ReadEnemyHealth(actor);
    if (enemy->baseHealth == 0) {
        enemy->baseHealth = (s16)health;
    }
    enemy->healthBeforeUpdate = (s16)health;
    enemy->damageBeforeUpdate = GetAppliedColliderDamage(actor);
    enemy->swordUnits = 0;
}

static void ScaleUnhookedHealthLoss(LeveledEnemy* enemy) {
    const s32 health = ReadEnemyHealth(enemy->actor);
    const s32 lost = enemy->healthBeforeUpdate - health - enemy->damageBeforeUpdate - enemy->swordUnits;
    if (health <= 0 || lost <= 0) {
        return;
    }
    const s32 units = ConvertHitToHealthUnits(enemy, GetLinkPower(), (f32)lost);
    WriteEnemyHealth(enemy->actor, MAX(health + lost - units, 1));
}

static void ReconcileAfterUpdate(void* actorRef) {
    sUpdatingActor = NULL;
    LeveledEnemy* enemy = FindEnemy(actorRef);
    if (enemy == NULL) {
        return;
    }
    SettleCompensation(enemy);
    if (ReadEnemyHealth(enemy->actor) <= 0 && enemy->healthBeforeUpdate > 0) {
        enemy->reachedZeroHealth = true;
    }
    if (HasUnhookedHealthLoss(enemy->actor->id)) {
        ScaleUnhookedHealthLoss(enemy);
    }
}

static void StoreProgress(void) {
    sApi->StorageSet(MOD_NAME, EXPERIENCE_KEY, &sExperience, sizeof(sExperience));
    sApi->StorageSet(MOD_NAME, BONUS_HEALTH_KEY, &sBonusHealth, sizeof(sBonusHealth));
}

static void ReadStored(const char* key, void* value, uint32_t size) {
    if (sApi->StorageHas(MOD_NAME, key)) {
        sApi->StorageGet(MOD_NAME, key, value, size);
    }
}

static void SyncBonusHealth(bool healGain) {
    const s16 bonusHealth = GetBonusHealth(sLevel);
    const s16 change = bonusHealth - sBonusHealth;
    if (change == 0) {
        return;
    }
    gSaveContext.healthCapacity += change;
    gSaveContext.health = MIN(gSaveContext.health, gSaveContext.healthCapacity);
    if (healGain && change > 0) {
        gSaveContext.healthAccumulator += change;
    }
    sBonusHealth = bonusHealth;
    StoreProgress();
}

// Vanilla steps the capacity to 48 per level and refills to that; the leveled capacity replaces both targets.
static void SyncMagicCapacity(void) {
    if (!gSaveContext.isMagicAcquired || gSaveContext.magicLevel == 0) {
        return;
    }
    const s16 vanillaCapacity = gSaveContext.magicLevel * MAGIC_NORMAL_METER;
    const s16 capacity = gSaveContext.magicLevel * GetMagicUnits(sLevel);

    if (gSaveContext.magicState == MAGIC_STATE_STEP_CAPACITY) {
        if (gSaveContext.magicCapacity != vanillaCapacity && gSaveContext.magicCapacity < capacity) {
            return;
        }
        gSaveContext.magicState = MAGIC_STATE_FILL;
    }
    if (gSaveContext.magicState == MAGIC_STATE_FILL && gSaveContext.magicFillTarget == vanillaCapacity) {
        gSaveContext.magicFillTarget = capacity;
    }
    gSaveContext.magicCapacity = capacity;
    gSaveContext.magicFillTarget = MIN(gSaveContext.magicFillTarget, capacity);
    gSaveContext.magic = MIN(gSaveContext.magic, capacity);
}

static u32 HashBytes(u32 hash, const void* data, size_t size) {
    const u8* bytes = data;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * 16777619u;
    }
    return hash;
}

// Identifies a save by its contents, so a copied file inherits the level and an erased one loses it.
static u32 HashSaveMeta(const SaveFileMetaInfo* meta) {
    u32 hash = 2166136261u;
    hash = HashBytes(hash, meta->playerName, sizeof(meta->playerName));
    hash = HashBytes(hash, &meta->deaths, sizeof(meta->deaths));
    hash = HashBytes(hash, &meta->healthCapacity, sizeof(meta->healthCapacity));
    hash = HashBytes(hash, &meta->questItems, sizeof(meta->questItems));
    hash = HashBytes(hash, meta->inventoryItems, sizeof(meta->inventoryItems));
    hash = HashBytes(hash, &meta->equipment, sizeof(meta->equipment));
    hash = HashBytes(hash, &meta->upgrades, sizeof(meta->upgrades));
    hash = HashBytes(hash, &meta->rupees, sizeof(meta->rupees));
    return HashBytes(hash, &meta->gsTokens, sizeof(meta->gsTokens));
}

static void FormatFileCvar(char* name, size_t capacity, s32 file, const char* field) {
    snprintf(name, capacity, CVAR("File%d.%s"), file, field);
}

static void CacheFileLevel(s32 file, u8 level) {
    if (file < 0 || file >= FILE_SLOTS) {
        return;
    }
    char name[64];
    FormatFileCvar(name, sizeof(name), file, "Level");
    CVarSetInteger(name, level);
    FormatFileCvar(name, sizeof(name), file, "Hash");
    CVarSetInteger(name, (s32)HashSaveMeta(Save_GetSaveMetaInfo(file)));
    CVarSave();
}

static s32 FindCachedFileLevel(u32 hash) {
    char name[64];
    for (s32 file = 0; file < FILE_SLOTS; file++) {
        FormatFileCvar(name, sizeof(name), file, "Hash");
        if ((u32)CVarGetInteger(name, 0) != hash) {
            continue;
        }
        FormatFileCvar(name, sizeof(name), file, "Level");
        return CVarGetInteger(name, 0);
    }
    return 0;
}

// OnSaveFile runs on the save thread: only note the slot here, the cache is written from the game thread.
static void NoteSavedFile(int32_t fileNum, int32_t sectionID) {
    sSavedLevel = sLevel;
    sSavedFile = fileNum;
}

static void CachePendingSavedFile(void) {
    const s32 file = sSavedFile;
    if (file == NO_SAVED_FILE) {
        return;
    }
    sSavedFile = NO_SAVED_FILE;
    CacheFileLevel(file, sSavedLevel);
}

static void UpdateLinkProgress(void) {
    if (gPlayState == NULL) {
        return;
    }
    CachePendingSavedFile();
    SyncBonusHealth(false);
    SyncMagicCapacity();
}

static void StartLevelUpPopups(u8 previousLevel, bool gainedHearts) {
    static const f32 sPartVelocityX[LEVEL_UP_PARTS] = { 0.0f, -4.0f, 4.0f, -2.0f, 2.0f };
    const bool gained[LEVEL_UP_PARTS] = {
        true,
        GetLinkBasePower(sLevel) > GetLinkBasePower(previousLevel),
        GetLinkBaseCourage(sLevel) > GetLinkBaseCourage(previousLevel),
        gainedHearts,
        gSaveContext.isMagicAcquired && GetMagicUnits(sLevel) > GetMagicUnits(previousLevel),
    };
    for (s32 part = 0; part < LEVEL_UP_PARTS; part++) {
        if (!gained[part]) {
            continue;
        }
        FloatingPopup* popup = &sLevelUpPopups[part];
        StartPopup(popup, 0, 100, (part == LEVEL_UP_TEXT) ? -6.0f : -5.0f);
        popup->x = (part == LEVEL_UP_TEXT) ? 0.0f : 16.0f;
        popup->velocityX = sPartVelocityX[part];
    }
}

static void LevelUp(u8 level) {
    const u8 previousLevel = sLevel;
    const s16 previousBonusHealth = sBonusHealth;
    sLevel = level;
    SyncBonusHealth(true);
    if (CVarGetInteger(CVAR("HUD.LevelUp"), 1)) {
        StartLevelUpPopups(previousLevel, sBonusHealth > previousBonusHealth);
    }
    if (CVarGetInteger(CVAR("HUD.LevelUpSound"), 1)) {
        Audio_PlayFanfare(NA_BGM_ITEM_GET);
    }
}

static void GainExperience(u32 amount) {
    if (amount == 0 || sLevel >= MAX_LEVEL) {
        return;
    }
    sExperience = MIN(sExperience + amount, MAX_EXPERIENCE);
    StoreProgress();
    if (CVarGetInteger(CVAR("Popup.Experience"), 1)) {
        StartPopup(&sExperiencePopup, (u16)MIN(amount, MAX_SHOWN_NUMBER), 40, -6.0f);
    }
    sNextLevelTimer = NEXT_LEVEL_SHOW_FRAMES;

    const u8 level = GetLevelForExperience(sExperience);
    if (level > sLevel) {
        LevelUp(level);
    }
}

static void RewardDefeat(void* actorRef) {
    LeveledEnemy* enemy = FindEnemy(actorRef);
    if (enemy == NULL || enemy->rewarded) {
        return;
    }
    enemy->rewarded = true;
    GainExperience(ApplyExperienceRate(enemy->experience / enemy->rewardShare));
}

static void ForgetEnemy(void* actorRef) {
    LeveledEnemy* enemy = FindEnemy(actorRef);
    if (enemy == NULL) {
        return;
    }
    if (enemy->reachedZeroHealth && enemy->rewardShare == 1) {
        RewardDefeat(actorRef);
    }
    if (sUpdatingActor == actorRef) {
        sUpdatingActor = NULL;
    }
    *enemy = sEnemies[--sEnemyCount];
}

static void RewardToken(int16_t flagType, int16_t flag) {
    if (flagType != FLAG_GS_TOKEN) {
        return;
    }
    GainExperience(ComputeTokenExperience(gSaveContext.inventory.gsTokens));
}

static LeveledEnemy* FindNearestEnemy(PlayState* play, f32 range) {
    Player* player = GET_PLAYER(play);
    LeveledEnemy* nearest = NULL;
    f32 nearestDistance = range;
    for (s32 i = 0; i < sEnemyCount; i++) {
        Actor* actor = sEnemies[i].actor;
        const f32 distance = Math_Vec3f_DistXYZ(&actor->world.pos, &player->actor.world.pos);
        if (actor->colChkInfo.health > 0 && distance < nearestDistance) {
            nearest = &sEnemies[i];
            nearestDistance = distance;
        }
    }
    return nearest;
}

static bool FindLinksAttackerPower(PlayState* play, u8* power) {
    if (sHasRecentAttacker && play->gameplayFrames - sRecentAttackFrame <= ATTACKER_MEMORY_FRAMES) {
        *power = sRecentAttackerPower;
        return true;
    }
    LeveledEnemy* nearest = FindNearestEnemy(play, NEAREST_ATTACKER_RANGE);
    if (nearest == NULL) {
        return false;
    }
    *power = CLAMP(GetEnemyPower(nearest->level) + ENEMY_POWER_BONUS, 0, 255);
    return true;
}

// Health_ChangeBy runs this hook before its death check, so the corrected health decides whether Link dies.
static void ScaleDamageTaken(int16_t healthChange) {
    PlayState* play = gPlayState;
    if (healthChange >= 0 || play == NULL) {
        return;
    }
    const s32 vanillaDamage = -healthChange;
    f32 damage = (f32)vanillaDamage;
    u8 attackerPower;
    if (FindLinksAttackerPower(play, &attackerPower)) {
        damage = RoundDamage(ComputeDamageOnLink(damage, attackerPower, GetLinkCourage()), damage);
    }
    const s32 dealt = MAX((s32)(damage * CVarGetFloat(CVAR("DamageTaken"), 1.0f)), 1);

    gSaveContext.health = MIN(gSaveContext.health + vanillaDamage - dealt, gSaveContext.healthCapacity);
    sHasRecentAttacker = false;
    StartDamagePopup(&sLinkDamagePopup, (u16)MIN(dealt, MAX_SHOWN_NUMBER), CVAR("Popup.LinkDamage"));
}

static void ResetPopups(void) {
    memset(&sLinkDamagePopup, 0, sizeof(sLinkDamagePopup));
    memset(&sExperiencePopup, 0, sizeof(sExperiencePopup));
    memset(sLevelUpPopups, 0, sizeof(sLevelUpPopups));
}

static void LoadProgress(int32_t fileNum) {
    sExperience = 0;
    sBonusHealth = 0;
    sEnemyCount = 0;
    sUpdatingActor = NULL;
    sNextLevelTimer = 0;
    sHasRecentAttacker = false;
    ResetPopups();
    ReadStored(EXPERIENCE_KEY, &sExperience, sizeof(sExperience));
    ReadStored(BONUS_HEALTH_KEY, &sBonusHealth, sizeof(sBonusHealth));
    sExperience = MIN(sExperience, MAX_EXPERIENCE);
    sLevel = GetLevelForExperience(sExperience);
    CacheFileLevel(fileNum, sLevel);
}

static s32 FormatEnemyInfo(const LeveledEnemy* enemy, char* text, size_t capacity) {
    s32 length = 0;
    if (CVarGetInteger(CVAR("Navi.Level"), 1)) {
        length += snprintf(text + length, capacity - length,
                           " \x05"
                           "FLv%u",
                           enemy->level);
    }
    if (CVarGetInteger(CVAR("Navi.MaxHealth"), 1) && enemy->baseHealth > 0) {
        length += snprintf(text + length, capacity - length,
                           " \x05"
                           "AMaxHP %u",
                           GetDisplayedMaxHealth(enemy));
    }
    if (length > 0) {
        length += snprintf(text + length, capacity - length,
                           "\x05"
                           "@");
    }
    return MIN(length, (s32)capacity - 1);
}

static void LoadMessageIntoFont(PlayState* play, u16 textId) {
    Font* font = &play->msgCtx.font;
    Message_FindMessage(play, textId);
    font->msgLength = MIN(font->msgLength, sizeof(font->msgBuf));
    memcpy(font->msgBuf, (const char*)font->msgOffset, font->msgLength);
}

static void AppendEnemyInfo(uint16_t* textId, bool* loadFromMessageTable) {
    PlayState* play = gPlayState;
    if (play == NULL || *textId <= NAVI_ENEMY_TEXT_FIRST || *textId >= NAVI_ENEMY_TEXT_LAST ||
        gSaveContext.language == LANGUAGE_JPN) {
        return;
    }
    LeveledEnemy* enemy = FindEnemy(GET_PLAYER(play)->focusActor);
    char suffix[48];
    const s32 suffixLength = (enemy != NULL) ? FormatEnemyInfo(enemy, suffix, sizeof(suffix)) : 0;
    if (suffixLength <= 0) {
        return;
    }
    Font* font = &play->msgCtx.font;
    if (*loadFromMessageTable) {
        LoadMessageIntoFont(play, *textId);
    }
    char* newline = memchr(font->msgBuf, MESSAGE_NEWLINE, font->msgLength);
    if (newline == NULL || font->msgLength + suffixLength > sizeof(font->msgBuf)) {
        return;
    }
    const size_t insertAt = newline - font->msgBuf;
    memmove(newline + suffixLength, newline, font->msgLength - insertAt);
    memcpy(newline, suffix, suffixLength);
    font->msgLength += suffixLength;
    play->msgCtx.msgLength = font->msgLength;
    *loadFromMessageTable = false;
}

static bool ProjectToScreen(PlayState* play, Vec3f* world, f32* screenX, f32* screenY) {
    Vec3f projected;
    f32 inverseW;
    Actor_ProjectPos(play, world, &projected, &inverseW);
    if (projected.z <= 0.0f) {
        return false;
    }
    *screenX = SCREEN_WIDTH / 2 + (SCREEN_WIDTH / 2) * projected.x * inverseW;
    *screenY = SCREEN_HEIGHT / 2 - (SCREEN_HEIGHT / 2) * projected.y * inverseW;
    if (CVarGetInteger("gEnhancements.MirroredWorld", 0)) {
        *screenX = SCREEN_WIDTH - *screenX;
    }
    return true;
}

static void StepBouncingPopup(FloatingPopup* popup) {
    popup->x += popup->velocityX;
    popup->y += popup->velocityY;
    popup->velocityY += 1.15f;
    if (popup->y >= 0.0f) {
        popup->y = 0.0f;
        popup->velocityY = -popup->velocityY * 0.5f;
        if (popup->velocityY >= -4.0f) {
            popup->velocityY = 0.0f;
        }
    }
    popup->life--;
}

static void StepRisingPopup(FloatingPopup* popup, f32 drag) {
    popup->x += popup->velocityX;
    popup->y += popup->velocityY;
    if (popup->velocityY < 0.0f) {
        popup->velocityY = MIN(popup->velocityY + drag, 0.0f);
    }
    if (popup->velocityX < 0.0f) {
        popup->velocityX = MIN(popup->velocityX + 0.5f, 0.0f);
    } else if (popup->velocityX > 0.0f) {
        popup->velocityX = MAX(popup->velocityX - 0.5f, 0.0f);
    }
    popup->life--;
}

static u8 GetPopupAlpha(const FloatingPopup* popup) {
    return (u8)CLAMP(popup->life / 8.0f * 255.0f, 0.0f, 255.0f);
}

static Gfx* DrawRectangle(Gfx* gfx, s16 textureWidth, s16 textureHeight, s16 left, s16 top, s16 width, s16 height) {
    gSPWideTextureRectangle(gfx++, left * 4, top * 4, (left + width) * 4, (top + height) * 4, G_TX_RENDERTILE, 0, 0,
                            (textureWidth << 10) / width, (textureHeight << 10) / height);
    return gfx;
}

static Gfx* DrawI8(Gfx* gfx, const char* texture, s16 textureWidth, s16 textureHeight, s16 left, s16 top) {
    gDPLoadTextureBlock(gfx++, texture, G_IM_FMT_I, G_IM_SIZ_8b, textureWidth, textureHeight, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    return DrawRectangle(gfx, textureWidth, textureHeight, left, top, textureWidth, textureHeight);
}

static Gfx* DrawIA8(Gfx* gfx, const char* texture, s16 textureSize, s16 left, s16 top, s16 size) {
    gDPLoadTextureBlock(gfx++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, textureSize, textureSize, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    return DrawRectangle(gfx, textureSize, textureSize, left, top, size, size);
}

static Gfx* DrawFontLetter(Gfx* gfx, const char* texture, s16 left, s16 top, s16 size) {
    gDPLoadTextureBlock_4b(gfx++, texture, G_IM_FMT_I, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    return DrawRectangle(gfx, 16, 16, left, top, size, size);
}

static Gfx* DrawNextLabel(Gfx* gfx, s16 left, s16 top, s16 width, s16 height) {
    gDPLoadTextureBlock_4b(gfx++, sNextTexture, G_IM_FMT_IA, 48, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    return DrawRectangle(gfx, 48, 16, left, top, width, height);
}

static Gfx* DrawIcon(Gfx* gfx, const char* texture, s16 textureSize, s16 left, s16 top, s16 size) {
    gDPLoadTextureBlock(gfx++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, textureSize, textureSize, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    return DrawRectangle(gfx, textureSize, textureSize, left, top, size, size);
}

static Gfx* SetColor(Gfx* gfx, TextColor color, u8 alpha) {
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, color.r, color.g, color.b, alpha);
    return gfx;
}

static s16 MeasureNumber(const DigitFont* font, u32 value) {
    char text[12];
    return (s16)(snprintf(text, sizeof(text), "%u", value) * font->advance);
}

static Gfx* DrawDigits(Gfx* gfx, const DigitFont* font, u32 value, s16 left, s16 top) {
    char text[12];
    const s32 count = snprintf(text, sizeof(text), "%u", value);
    for (s32 i = 0; i < count; i++) {
        const char* texture = font->textures[text[i] - '0'];
        const s16 digitLeft = left + i * font->advance;
        if (font->intensity) {
            gfx = DrawI8(gfx, texture, font->width, font->height, digitLeft, top);
        } else {
            gfx = DrawIA8(gfx, texture, font->width, digitLeft, top, font->height);
        }
    }
    return gfx;
}

// Only the big digits get a drop shadow: on the 8x8 ammo digits any offset copy fills the glyph's holes.
static Gfx* DrawShadowedNumber(Gfx* gfx, const DigitFont* font, u32 value, s16 left, s16 top, TextColor color,
                               u8 alpha) {
    if (font->shadowed) {
        const TextColor black = { 0, 0, 0 };
        gfx = SetColor(gfx, black, alpha / 2);
        gfx = DrawDigits(gfx, font, value, left + 1, top + 1);
    }
    gfx = SetColor(gfx, color, alpha);
    return DrawDigits(gfx, font, value, left, top);
}

static Gfx* DrawFraction(Gfx* gfx, u32 current, u32 maximum, s16 left, s16 top, TextColor color, u8 alpha) {
    gfx = DrawShadowedNumber(gfx, &sSmallDigits, current, left, top, color, alpha);
    const s16 slashLeft = left + MeasureNumber(&sSmallDigits, current);
    gfx = SetColor(gfx, sWhite, alpha);
    gfx = DrawFontLetter(gfx, sSlashTexture, slashLeft, top - 1, 9);
    return DrawShadowedNumber(gfx, &sSmallDigits, maximum, slashLeft + 7, top, sMaximumGreen, alpha);
}

static Gfx* DrawEnemyDamagePopups(PlayState* play, Gfx* gfx) {
    for (s32 i = 0; i < sEnemyCount; i++) {
        LeveledEnemy* enemy = &sEnemies[i];
        FloatingPopup* popup = &enemy->damagePopup;
        f32 screenX;
        f32 screenY;
        if (popup->life <= 0) {
            continue;
        }
        StepBouncingPopup(popup);
        if (!ProjectToScreen(play, &enemy->actor->focus.pos, &screenX, &screenY)) {
            continue;
        }
        const s16 left = (s16)(screenX + popup->x) - MeasureNumber(&sBigDigits, popup->value) / 2;
        gfx = DrawShadowedNumber(gfx, &sBigDigits, popup->value, left, (s16)(screenY - 28 + popup->y), sWhite,
                                 GetPopupAlpha(popup));
    }
    return gfx;
}

static void ProjectLinkOrCenter(PlayState* play, Vec3f* world, f32* screenX, f32* screenY) {
    if (!ProjectToScreen(play, world, screenX, screenY)) {
        *screenX = SCREEN_WIDTH / 2;
        *screenY = 200.0f;
    }
}

static Gfx* DrawLinkNumberPopups(PlayState* play, Gfx* gfx) {
    Player* player = GET_PLAYER(play);
    f32 screenX;
    f32 screenY;
    if (sLinkDamagePopup.life > 0) {
        StepBouncingPopup(&sLinkDamagePopup);
        ProjectLinkOrCenter(play, &player->actor.focus.pos, &screenX, &screenY);
        const s16 left = (s16)(screenX + sLinkDamagePopup.x) - MeasureNumber(&sBigDigits, sLinkDamagePopup.value) / 2;
        gfx =
            DrawShadowedNumber(gfx, &sBigDigits, sLinkDamagePopup.value, left, (s16)(screenY - 28 + sLinkDamagePopup.y),
                               sLinkDamageOrange, GetPopupAlpha(&sLinkDamagePopup));
    }
    if (sExperiencePopup.life > 0) {
        StepRisingPopup(&sExperiencePopup, 0.4f);
        ProjectLinkOrCenter(play, &player->actor.world.pos, &screenX, &screenY);
        const s16 left = (s16)(screenX + sExperiencePopup.x) - MeasureNumber(&sBigDigits, sExperiencePopup.value) / 2;
        gfx =
            DrawShadowedNumber(gfx, &sBigDigits, sExperiencePopup.value, left, (s16)(screenY - 30 + sExperiencePopup.y),
                               sExperienceYellow, GetPopupAlpha(&sExperiencePopup));
    }
    return gfx;
}

static Gfx* DrawLevelUpText(Gfx* gfx, s16 left, s16 top) {
    for (u32 i = 0; i < ARRAY_COUNT(sLevelUpLetters); i++) {
        if (sLevelUpLetters[i] != NULL) {
            gfx = DrawFontLetter(gfx, sLevelUpLetters[i], left + i * 6, top, 10);
        }
    }
    return gfx;
}

static Gfx* DrawLevelUpPart(Gfx* gfx, s32 part, s16 left, s16 top) {
    switch (part) {
        case LEVEL_UP_TEXT:
            return DrawLevelUpText(gfx, left, top);
        case LEVEL_UP_POWER:
            return DrawIcon(gfx, sPowerIconTexture, 32, left, top, 12);
        case LEVEL_UP_COURAGE:
            return DrawIcon(gfx, sCourageIconTexture, 32, left, top, 12);
        case LEVEL_UP_HEARTS:
            return DrawIA8(gfx, sHeartTexture, 16, left, top, 12);
        case LEVEL_UP_MAGIC:
            return DrawIcon(gfx, sMagicIconTexture, 24, left, top, 12);
        default:
            return gfx;
    }
}

static Gfx* DrawLevelUpPopups(PlayState* play, Gfx* gfx) {
    f32 screenX;
    f32 screenY;
    ProjectLinkOrCenter(play, &GET_PLAYER(play)->actor.world.pos, &screenX, &screenY);
    for (s32 part = 0; part < LEVEL_UP_PARTS; part++) {
        FloatingPopup* popup = &sLevelUpPopups[part];
        if (popup->life <= 0) {
            continue;
        }
        StepRisingPopup(popup, 0.75f);
        gfx = SetColor(gfx, (part == LEVEL_UP_HEARTS) ? sHeartRed : sWhite, GetPopupAlpha(popup));
        gfx = DrawLevelUpPart(gfx, part, (s16)(screenX - 19 + popup->x), (s16)(screenY - 30 + popup->y));
    }
    return gfx;
}

static Gfx* DrawNextLevel(Gfx* gfx) {
    if (sNextLevelTimer <= 0) {
        return gfx;
    }
    sNextLevelTimer--;
    if (!CVarGetInteger(CVAR("HUD.NextLevel"), 1) || sLevel >= MAX_LEVEL) {
        return gfx;
    }
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawNextLabel(gfx, 10, 58, 48, 16);
    return DrawShadowedNumber(gfx, &sBigDigits, GetExperienceToNextLevel(), 48, 58, sWhite, 255);
}

static Gfx* DrawHealthNumbers(PlayState* play, Gfx* gfx) {
    const s32 mode = CVarGetInteger(CVAR("HUD.HealthNumbers"), NUMBERS_FULL);
    const u8 alpha = (u8)play->interfaceCtx.healthAlpha;
    if (mode == NUMBERS_NONE || alpha == 0) {
        return gfx;
    }
    const s32 lineLength = CVarGetInteger("gCosmetics.HUD.Hearts.LineLength", 10);
    const s32 hearts = gSaveContext.healthCapacity / HEART_HEALTH;
    const s32 heartsInFirstRow = (lineLength == 0) ? hearts : MIN(hearts, lineLength);
    const s16 left = 28 + getHealthMeterXOffset() + 10 * heartsInFirstRow;
    const s16 top = 22 + getHealthMeterYOffset();
    const TextColor color = HealthMeter_IsCritical() ? sCriticalOrange : sWhite;
    if (mode == NUMBERS_CURRENT) {
        return DrawShadowedNumber(gfx, &sSmallDigits, gSaveContext.health, left, top, color, alpha);
    }
    return DrawFraction(gfx, gSaveContext.health, gSaveContext.healthCapacity, left, top, color, alpha);
}

static s16 GetMagicBarTop(void) {
    const bool useMargins = CVarGetInteger("gCosmetics.HUD.MagicBar.UseMargins", 0);
    const s16 marginY = useMargins ? -Top_HUD_Margin : 0;
    const s32 lineLength = CVarGetInteger("gCosmetics.HUD.Hearts.LineLength", 10);
    const s32 extraRows = (lineLength == 0) ? 0 : (gSaveContext.healthCapacity - 1) / (HEART_HEALTH * lineLength);
    if (extraRows == 0) {
        return R_MAGIC_BAR_SMALL_Y + marginY;
    }
    return R_MAGIC_BAR_LARGE_Y + marginY + (R_MAGIC_BAR_LARGE_Y - R_MAGIC_BAR_SMALL_Y + 2) * (extraRows - 1);
}

static Gfx* DrawMagicNumbers(PlayState* play, Gfx* gfx) {
    const s32 mode = CVarGetInteger(CVAR("HUD.MagicNumbers"), NUMBERS_FULL);
    const u8 alpha = (u8)play->interfaceCtx.magicAlpha;
    const bool originalLocation = CVarGetInteger("gCosmetics.HUD.MagicBar.PosType", 0) == 0;
    if (mode == NUMBERS_NONE || alpha == 0 || gSaveContext.magicLevel == 0 || !originalLocation) {
        return gfx;
    }
    const s16 marginX = CVarGetInteger("gCosmetics.HUD.MagicBar.UseMargins", 0) ? Left_HUD_Margin : 0;
    const s16 fillLeft = OTRGetRectDimensionFromLeftEdge(R_MAGIC_FILL_X + marginX);
    const s16 center = fillLeft + gSaveContext.magicCapacity / 2;
    const s16 top = GetMagicBarTop() + 2;
    if (mode == NUMBERS_CURRENT) {
        const s16 left = center - MeasureNumber(&sSmallDigits, gSaveContext.magic) / 2;
        return DrawShadowedNumber(gfx, &sSmallDigits, gSaveContext.magic, left, top, sWhite, alpha);
    }
    const s16 width =
        MeasureNumber(&sSmallDigits, gSaveContext.magic) + 7 + MeasureNumber(&sSmallDigits, gSaveContext.magicCapacity);
    return DrawFraction(gfx, gSaveContext.magic, gSaveContext.magicCapacity, center - width / 2, top, sWhite, alpha);
}

static u16 GetSwordAttack(void) {
    const u8 multiplier = GetAttackHealthMultiplier();
    switch (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)) {
        case EQUIP_VALUE_SWORD_KOKIRI:
            return multiplier;
        case EQUIP_VALUE_SWORD_MASTER:
            return multiplier * 2;
        case EQUIP_VALUE_SWORD_BIGGORON:
            return IsGiantsKnifeBroken() ? multiplier : multiplier * 4;
        default:
            return 1;
    }
}

static TextColor GetModifierColor(s8 modifier) {
    if (modifier > 0) {
        return sMaximumGreen;
    }
    return (modifier < 0) ? sPenaltyRed : sWhite;
}

static Gfx* DrawPauseLevel(Gfx* gfx) {
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawFontLetter(gfx, sCapitalLTexture, 90, 68, 10);
    gfx = DrawFontLetter(gfx, sSmallVTexture, 93, 68, 10);
    return DrawShadowedNumber(gfx, &sBigDigits, sLevel, 98, 64, sWhite, 255);
}

static Gfx* DrawPauseVitals(Gfx* gfx) {
    const bool hasMagic = gSaveContext.magicCapacity > 0;
    const s16 healthTop = hasMagic ? 66 : 70;
    const TextColor healthColor = HealthMeter_IsCritical() ? sCriticalOrange : sWhite;

    gfx = SetColor(gfx, sHeartRed, 255);
    gfx = DrawIA8(gfx, sHeartTexture, 16, 116, healthTop, 8);
    gfx = DrawFraction(gfx, gSaveContext.health, gSaveContext.healthCapacity, 125, healthTop, healthColor, 255);
    if (!hasMagic) {
        return gfx;
    }
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawIcon(gfx, sMagicIconTexture, 24, 115, healthTop + 8, 9);
    return DrawFraction(gfx, gSaveContext.magic, gSaveContext.magicCapacity, 125, healthTop + 8, sWhite, 255);
}

static Gfx* DrawPauseCombatStats(Gfx* gfx) {
    const StatModifiers modifiers = GetEquipmentModifiers();
    const u8 power = GetLinkPower();

    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawIcon(gfx, sPowerIconTexture, 32, 90, 84, 9);
    gfx = DrawShadowedNumber(gfx, &sSmallDigits, power, 100, 85, GetModifierColor(modifiers.power), 255);
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawIcon(gfx, sCourageIconTexture, 32, 112, 84, 9);
    gfx = DrawShadowedNumber(gfx, &sSmallDigits, GetLinkCourage(), 122, 85, GetModifierColor(modifiers.courage), 255);
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawIcon(gfx, sAttackIconTexture, 32, 134, 84, 9);
    return DrawShadowedNumber(gfx, &sSmallDigits, (u16)ScaleAttack(GetSwordAttack(), power), 144, 85, sWhite, 255);
}

static Gfx* DrawPauseExperience(Gfx* gfx) {
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawIcon(gfx, sExperienceIconTexture, 32, 88, 170, 9);
    gfx = DrawShadowedNumber(gfx, &sSmallDigits, sExperience, 98, 171, sWhite, 255);
    gfx = SetColor(gfx, sWhite, 255);
    gfx = DrawNextLabel(gfx, 82, 178, 34, 11);
    return DrawShadowedNumber(gfx, &sSmallDigits, GetExperienceToNextLevel(), 116, 180, sWhite, 255);
}

static bool IsEquipmentPageIdle(PlayState* play) {
    PauseContext* pause = &play->pauseCtx;
    return pause->state == KALEIDO_STATE_OPEN && pause->pageIndex == PAUSE_EQUIP && pause->unk_1E4 == 0 &&
           pause->alpha == 255;
}

static Gfx* DrawPauseStats(PlayState* play, Gfx* gfx) {
    if (!CVarGetInteger(CVAR("HUD.PauseStats"), 1) || !IsEquipmentPageIdle(play)) {
        return gfx;
    }
    gfx = DrawPauseLevel(gfx);
    gfx = DrawPauseVitals(gfx);
    gfx = DrawPauseCombatStats(gfx);
    return DrawPauseExperience(gfx);
}

// The hook runs after the screen fade and the scene transition are drawn, so it must hide itself under them.
static bool IsWorldHudVisible(PlayState* play) {
    const bool transitioning = play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF;
    return !transitioning && play->interfaceCtx.unk_244 == 0 && play->interfaceCtx.healthAlpha > 0;
}

static void DrawLeveledHud(PlayState* play) {
    const bool paused = play->pauseCtx.state != 0;
    if (!paused && !IsWorldHudVisible(play)) {
        return;
    }
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    if (paused) {
        OVERLAY_DISP = DrawPauseStats(play, OVERLAY_DISP);
    } else {
        OVERLAY_DISP = DrawEnemyDamagePopups(play, OVERLAY_DISP);
        OVERLAY_DISP = DrawLinkNumberPopups(play, OVERLAY_DISP);
        OVERLAY_DISP = DrawLevelUpPopups(play, OVERLAY_DISP);
        OVERLAY_DISP = DrawNextLevel(OVERLAY_DISP);
    }
    OVERLAY_DISP = DrawHealthNumbers(play, OVERLAY_DISP);
    OVERLAY_DISP = DrawMagicNumbers(play, OVERLAY_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ClearPopupsOnSceneChange(int16_t sceneNum) {
    ResetPopups();
    sNextLevelTimer = 0;
}

// The file info box is drawn with eye (0, 0, 64) and a 60 degree view at depth -93.6.
static f32 GetFileSelectPixelsPerUnit(void) {
    return (SCREEN_HEIGHT / 2) / (FILE_SELECT_DEPTH * tanf(FILE_SELECT_HALF_FOV));
}

static Gfx* DrawFileLevel(FileChooseContext* fileChoose, Gfx* gfx, s32 file, s32 level) {
    static const TextColor sFileLevelYellow = { 255, 255, 100 };
    const f32 pixelsPerUnit = GetFileSelectPixelsPerUnit();
    const s16 nameRowY = 0x2C - WREG(38) * file + fileChoose->buttonYOffsets[file] + fileChoose->fileNamesY[file];
    const s16 centerX = (s16)(SCREEN_WIDTH / 2 + (fileChoose->windowPosX + FILE_LEVEL_X) * pixelsPerUnit);
    const s16 labelTop = (s16)(SCREEN_HEIGHT / 2 - (nameRowY + FILE_LEVEL_Y) * pixelsPerUnit);
    const u8 alpha = (u8)fileChoose->fileInfoAlpha[file];

    gfx = SetColor(gfx, sFileLevelYellow, alpha);
    gfx = DrawFontLetter(gfx, sCapitalLTexture, centerX - 7, labelTop, 10);
    gfx = DrawFontLetter(gfx, sSmallVTexture, centerX - 2, labelTop, 10);
    const s16 numberLeft = centerX - MeasureNumber(&sSmallDigits, level) / 2;
    return DrawShadowedNumber(gfx, &sSmallDigits, level, numberLeft, labelTop + 11, sFileLevelYellow, alpha);
}

// The screen fill fading to black goes on POLY_OPA, under the overlay, so the label must leave before it.
static bool IsFileSelectFadingOut(const FileChooseContext* fileChoose) {
    return fileChoose->menuMode == FS_MENU_MODE_SELECT && fileChoose->selectMode >= SM_FADE_OUT;
}

static void DrawFileSelectLevels(void* gameState) {
    FileChooseContext* fileChoose = gameState;
    CachePendingSavedFile();
    if (!CVarGetInteger(CVAR("HUD.FileSelectLevel"), 1) || IsFileSelectFadingOut(fileChoose)) {
        return;
    }
    Gfx_SetupDL_39Overlay(fileChoose->state.gfxCtx);
    OPEN_DISPS(fileChoose->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (s32 file = 0; file < FILE_SLOTS; file++) {
        const SaveFileMetaInfo* meta = Save_GetSaveMetaInfo(file);
        if (fileChoose->fileInfoAlpha[file] == 0 || !meta->valid) {
            continue;
        }
        const s32 level = FindCachedFileLevel(HashSaveMeta(meta));
        if (level > 0) {
            OVERLAY_DISP = DrawFileLevel(fileChoose, OVERLAY_DISP, file, level);
        }
    }
    CLOSE_DISPS(fileChoose->state.gfxCtx);
}

static SOHModMenuWidget MakeWidget(uint8_t column, SOHModMenuWidgetType type, const char* label, const char* cvar) {
    SOHModMenuWidget widget = { 0 };
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Leveled";
    widget.column = column;
    widget.type = type;
    widget.label = label;
    widget.cvar = cvar;
    return widget;
}

static void DrawHudHeading(void) {
    ModUI_Text("HUD");
}

static void DrawPopupHeading(void) {
    ModUI_Text("Floating numbers");
}

static void DrawNaviHeading(void) {
    ModUI_Text("Navi");
}

static void DrawLinkHeading(void) {
    ModUI_Text("Link");
}

static void DrawEnemyHeading(void) {
    ModUI_Text("Enemies");
}

static void AddHeading(uint8_t column, const char* label, SOHModMenuCallback draw) {
    SOHModMenuWidget widget = MakeWidget(column, SOH_MOD_MENU_CUSTOM, label, NULL);
    widget.onChange = draw;
    sApi->RegisterMenuWidget(&widget);
}

static void AddCheckbox(uint8_t column, const char* label, const char* cvar, const char* tooltip) {
    SOHModMenuWidget widget = MakeWidget(column, SOH_MOD_MENU_CHECKBOX, label, cvar);
    widget.tooltip = tooltip;
    widget.defaultInt = 1;
    sApi->RegisterMenuWidget(&widget);
}

static void AddFloatSlider(uint8_t column, const char* label, const char* cvar, f32 maximum) {
    SOHModMenuWidget widget = MakeWidget(column, SOH_MOD_MENU_FLOAT_SLIDER, label, cvar);
    widget.defaultFloat = 1.0f;
    widget.minFloat = 0.1f;
    widget.maxFloat = maximum;
    sApi->RegisterMenuWidget(&widget);
}

static void AddNumbersCombobox(uint8_t column, const char* label, const char* cvar) {
    static const char* const sModes[] = { "Full", "Current", "None" };
    SOHModMenuWidget widget = MakeWidget(column, SOH_MOD_MENU_COMBOBOX, label, cvar);
    widget.values = sModes;
    widget.valueCount = ARRAY_COUNT(sModes);
    widget.defaultInt = NUMBERS_FULL;
    sApi->RegisterMenuWidget(&widget);
}

static void RegisterHudMenu(void) {
    AddHeading(0, "Leveled HUD", DrawHudHeading);
    AddCheckbox(0, "EXP to next level", CVAR("HUD.NextLevel"), "Shows the EXP still needed after gaining EXP.");
    AddCheckbox(0, "Level-up popup", CVAR("HUD.LevelUp"), "Shows LEVEL UP and the stats that grew.");
    AddCheckbox(0, "Level-up fanfare", CVAR("HUD.LevelUpSound"), NULL);
    AddCheckbox(0, "Stats on the equipment page", CVAR("HUD.PauseStats"), NULL);
    AddCheckbox(0, "Level on the file select", CVAR("HUD.FileSelectLevel"), NULL);
    AddNumbersCombobox(0, "Health numbers", CVAR("HUD.HealthNumbers"));
    AddNumbersCombobox(0, "Magic numbers", CVAR("HUD.MagicNumbers"));
    AddHeading(0, "Leveled floating numbers", DrawPopupHeading);
    AddCheckbox(0, "Enemy damage", CVAR("Popup.EnemyDamage"), NULL);
    AddCheckbox(0, "Link's damage", CVAR("Popup.LinkDamage"), NULL);
    AddCheckbox(0, "EXP gained", CVAR("Popup.Experience"), NULL);
    AddHeading(0, "Leveled Navi", DrawNaviHeading);
    AddCheckbox(0, "Navi tells the enemy level", CVAR("Navi.Level"), NULL);
    AddCheckbox(0, "Navi tells the enemy max HP", CVAR("Navi.MaxHealth"), NULL);
}

static void RegisterBalanceMenu(void) {
    SOHModMenuWidget multiplier =
        MakeWidget(1, SOH_MOD_MENU_INT_SLIDER, "Enemy HP and Link's attack scale", CVAR("AttackHealthMultiplier"));
    multiplier.tooltip = "Only the size of the numbers: balance stays the same.";
    multiplier.defaultInt = 9;
    multiplier.minInt = 1;
    multiplier.maxInt = 32;
    sApi->RegisterMenuWidget(&multiplier);

    AddHeading(1, "Leveled Link", DrawLinkHeading);
    AddCheckbox(1, "Levels add hearts", CVAR("LevelHearts"), "One heart every 8 levels, up to 10.");
    AddCheckbox(1, "Levels set magic capacity", CVAR("LevelMagic"),
                "From 25% of vanilla at level 1 up to 150% at level 84.");
    AddCheckbox(1, "Equipment changes stats", CVAR("EquipmentStats"),
                "Tunics, boots, shields, swords and bracelets change power and courage.");
    AddFloatSlider(1, "Damage taken", CVAR("DamageTaken"), 8.0f);
    AddFloatSlider(1, "EXP rate", CVAR("ExperienceRate"), 10.0f);
    AddFloatSlider(1, "Skulltula token EXP rate", CVAR("TokenExperienceRate"), 10.0f);

    AddHeading(1, "Leveled enemies", DrawEnemyHeading);
    AddCheckbox(1, "Enemy level raises its attack", CVAR("EnemyAttackScales"), NULL);
    AddFloatSlider(1, "Enemy HP", CVAR("EnemyHealth"), 10.0f);
    AddFloatSlider(1, "Enemy level scale", CVAR("LevelScale"), 10.0f);
    SOHModMenuWidget castle =
        MakeWidget(1, SOH_MOD_MENU_CHECKBOX, "100 tokens: Ganon's Castle at level 99", CVAR("MaxedGanonsCastle"));
    sApi->RegisterMenuWidget(&castle);
}

static void RegisterMenu(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterMenuSidebar) || !SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        return;
    }
    sApi->RegisterMenuSidebar("Mods", "Leveled", "gMods.Menu.Page", 2);
    RegisterHudMenu();
    RegisterBalanceMenu();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, StorageSet) || !SOH_MOD_API_HAS(sApi, StorageHas)) {
        fprintf(stderr, "[leveled] The host lacks the storage API; the mod stays off\n");
        return;
    }
    RegisterMenu();
    SOH_REGISTER_HOOK(sApi, OnLoadFile, LoadProgress);
    SOH_REGISTER_HOOK(sApi, OnActorInit, TrackEnemy);
    SOH_REGISTER_HOOK(sApi, OnActorDestroy, ForgetEnemy);
    SOH_REGISTER_HOOK(sApi, ShouldActorUpdate, SnapshotBeforeUpdate);
    SOH_REGISTER_HOOK(sApi, OnActorUpdate, ReconcileAfterUpdate);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, ResolveCollisionDamage);
    SOH_REGISTER_HOOK(sApi, OnResolveSwordDamage, ResolveSwordDamage);
    SOH_REGISTER_HOOK(sApi, OnEnemyDefeat, RewardDefeat);
    SOH_REGISTER_HOOK(sApi, OnBossDefeat, RewardDefeat);
    SOH_REGISTER_HOOK(sApi, OnFlagSet, RewardToken);
    SOH_REGISTER_HOOK(sApi, OnPlayerHealthChange, ScaleDamageTaken);
    SOH_REGISTER_HOOK(sApi, OnInterfaceUpdate, UpdateLinkProgress);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawLeveledHud);
    SOH_REGISTER_HOOK(sApi, OnOpenText, AppendEnemyInfo);
    SOH_REGISTER_HOOK(sApi, OnSaveFile, NoteSavedFile);
    SOH_REGISTER_HOOK(sApi, OnFileChooseMain, DrawFileSelectLevels);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ClearPopupsOnSceneChange);
}
