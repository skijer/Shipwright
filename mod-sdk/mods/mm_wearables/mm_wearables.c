// Majora's Mask wearables: seventeen masks that leave Link as Link.
// The four transformation masks, Keaton's Mask, Kafei's Mask and Garo's Mask live in their own mods.
// Together the mods fill Majora's 24-cell mask grid.

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "z64items.h"
#include "overlays/actors/ovl_En_Du/z_en_du.h"
extern void func_809FE4A4(EnDu* darunia, PlayState* play);
extern void MmTax_Init(const SOHModApi* api);

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define MASK_GRID_SIZE 24
#define MASK_PAGE 1
#define MASK_NONE (-1)

#define MASK_POSTMAN 0
#define MASK_ALL_NIGHT 1
#define MASK_BLAST 2
#define MASK_STONE 3
#define MASK_GREAT_FAIRY 4
#define MASK_BREMEN 7
#define MASK_BUNNY 8
#define MASK_DON_GERO 9
#define MASK_SCENTS 10
#define MASK_ROMANI 12
#define MASK_CIRCUS_LEADER 13
#define MASK_COUPLE 15
#define MASK_TRUTH 16
#define MASK_KAMARO 18
#define MASK_GIBDO 19
#define MASK_CAPTAIN 21
#define MASK_GIANT 22

// Every slot this mod fills. Everything per-mask is generated from it, so adding a mask is one line.
#define MM_WEARABLE_SLOTS(X)                                                                                     \
    X(MASK_POSTMAN) X(MASK_ALL_NIGHT) X(MASK_BLAST) X(MASK_STONE) X(MASK_GREAT_FAIRY) X(MASK_BREMEN)              \
    X(MASK_BUNNY) X(MASK_DON_GERO) X(MASK_SCENTS) X(MASK_ROMANI) X(MASK_CIRCUS_LEADER) X(MASK_COUPLE)             \
    X(MASK_TRUTH) X(MASK_KAMARO) X(MASK_GIBDO) X(MASK_CAPTAIN) X(MASK_GIANT)

#define MM_ICON_DIR "__OTR__icon_item_static_yar/"
#define MM_NAME_DIR "__OTR__item_name_static/"
#define MM_OBJECT_DIR "__OTR__objects/object_"
#define MM_ANIM_DIR "__OTR__misc/link_animetion/gPlayerAnim_"

// soh does not name the bgCheckFlags bits; z_player.c reads this one raw as 1.
#define BG_ON_GROUND 1

// An actor this far away is outside every range check in the game.
#define HIDDEN_DISTANCE 32000.0f

#define NIGHT_SKULLTULA_SCALE 0.02f

#define BLAST_COOLDOWN_FRAMES 310
#define BLAST_FADE_FRAMES 10
#define BOMB_SCALE 0.01f

#define BUNNY_SPEED_SCALE 1.5f
#define BUNNY_HOOD_FAST_AND_JUMP 2

#define COUPLE_HEALTH_INTERVAL 4
#define COUPLE_MAGIC_INTERVAL 7

#define GIANT_SCALE 0.025f
#define GIANT_DEFAULT_SCALE 0.01f
#define GIANT_MAGIC_INTERVAL 4
#define GIANT_FILL_START 56
#define GIANT_FILL_IN_STEP 45
#define GIANT_FILL_OUT_STEP 30

#define FROG_LOG_X 990.0f
#define FROG_LOG_Z (-1220.0f)
#define FROG_LOG_RADIUS_SQ (200.0f * 200.0f)
#define FROG_SONG_COUNT 7

#define CAPTAIN_SPAWN_INTERVAL 100
#define CAPTAIN_RETRY_INTERVAL 80
#define CAPTAIN_MAX_MINIONS 3
#define CAPTAIN_SPAWN_NEAR 200.0f
#define CAPTAIN_SPAWN_SPREAD 200.0f
#define CAPTAIN_GIANT_STALCHILD_PARAMS 10
// home.rot.z on a spawned minion: nothing vanilla writes it, so it marks our own.
#define CAPTAIN_MINION_TAG 0x7FFF

#define BREMEN_MARCH_SPEED 3.5f
#define BREMEN_TURN_RATE 0x07D0
#define BREMEN_CUCCO_AT_FRAME 120
#define BREMEN_CUCCO_COOLDOWN 400
// SPEED_MODE_CURVED of z_player.c, which has no public header either.
#define SPEED_MODE_CURVED 0.018f

#define COW_TALK_RADIUS 150.0f

#define KAMARO_DANCE_RADIUS 200.0f
#define KAMARO_DARUNIA_FRAMES 100

#define MASK_BUTTON_OWNER "skijer.mm_wearables"

#define SCENTS_IDLE_SPEED 0.5f

#define FAIRY_HAIR_MATRICES 6
#define FAIRY_HAIR_SEGMENT 0x0B
#define GREAT_FAIRY_REWARD_SWITCH 0x38

// A Majora player animation is the same raw format as an Ocarina one, but its root sits elsewhere.
#define MM_ANIM_VALUES_PER_FRAME 67
#define MM_ANIM_BASE_TRANSL_X (-57)

#define PERSISTENT_MASKS_CVAR "gEnhancements.PersistentMasks"
#define BUNNY_HOOD_CVAR "gEnhancements.MMBunnyHood"

typedef struct {
    const char* key;
    const char* iconPath;
    const char* namePath;
    // NULL for a mask that wears Ocarina's own model instead (see vanillaMask).
    const char* wornDL;
    const char* getItemDL;
    // The PLAYER_MASK_* slot this mask lends while worn, so every vanilla NPC check already works.
    uint8_t vanillaMask;
    const char* getItemText;
    const char* pauseText;
} WearableMask;

static const WearableMask sMasks[MASK_GRID_SIZE] = {
    [MASK_POSTMAN] = { "skijer.mask_postman", MM_ICON_DIR "gItemIconPostmansHatTex",
                       MM_NAME_DIR "gItemNamePostmansHatENGTex",
                       MM_OBJECT_DIR "mask_posthat/object_mask_posthat_DL_000290",
                       MM_OBJECT_DIR "gi_mask12/gGiPostmanHatCapDL", PLAYER_MASK_NONE,
                       "You got the %rPostman's Hat%w!&The cap of a man who has never&been late in his life.",
                       "%rPostman's Hat&%wPress %y\xA1%w to wear it.&%y\xA0%w opens the mailbox map." },
    [MASK_ALL_NIGHT] = { "skijer.mask_all_night", MM_ICON_DIR "gItemIconAllNightMaskTex",
                         MM_NAME_DIR "gItemNameAllNightMaskENGTex",
                         MM_OBJECT_DIR "mask_yofukasi/object_mask_yofukasi_DL_000490",
                         MM_OBJECT_DIR "gi_mask06/gGiAllNightMaskFaceDL", PLAYER_MASK_NONE,
                         "You got the %rAll-Night Mask%w!&Nothing sleeps while you wear it,&not even what only "
                         "comes out at night.",
                         "%rAll-Night Mask&%wPress %y\xA1%w to wear it.&Night skulltulas come out by day." },
    [MASK_BLAST] = { "skijer.mask_blast", MM_ICON_DIR "gItemIconBlastMaskTex",
                     MM_NAME_DIR "gItemNameBlastMaskENGTex",
                     MM_OBJECT_DIR "mask_bakuretu/object_mask_bakuretu_DL_0005C0",
                     MM_OBJECT_DIR "gi_mask21/gGiBlastMaskDL", PLAYER_MASK_NONE,
                     "You got the %rBlast Mask%w!&A bomb you can wear. Press %y\xA0%w&and stand well back.",
                     "%rBlast Mask&%wPress %y\xA1%w to wear it.&Then %y\xA0%w detonates it." },
    [MASK_STONE] = { "skijer.mask_stone", MM_ICON_DIR "gItemIconStoneMaskTex",
                     MM_NAME_DIR "gItemNameStoneMaskENGTex", MM_OBJECT_DIR "mask_stone/object_mask_stone_DL_000820",
                     MM_OBJECT_DIR "gi_stonemask/gGiStoneMaskDL", PLAYER_MASK_NONE,
                     "You got the %rStone Mask%w!&It makes you as dull as a rock:&nobody will look twice at you.",
                     "%rStone Mask&%wPress %y\xA1%w to wear it.&Nothing can see or hear you." },
    [MASK_GREAT_FAIRY] = { "skijer.mask_great_fairy", MM_ICON_DIR "gItemIconGreatFairyMaskTex",
                           MM_NAME_DIR "gItemNameGreatFairysMaskENGTex",
                           MM_OBJECT_DIR "mask_bigelf/object_mask_bigelf_DL_0016F0",
                           MM_OBJECT_DIR "gi_mask14/gGiGreatFairyMaskFaceDL", PLAYER_MASK_NONE,
                           "You got the %rGreat Fairy's Mask%w!&Stray fairies come to it, and the&Great Fairies "
                           "answer it.",
                           "%rGreat Fairy's Mask&%wPress %y\xA1%w to wear it.&%y\xA0%w opens the fountain map;&in a "
                           "fountain, %y\x9F%w claims her gift." },
    [MASK_BREMEN] = { "skijer.mask_bremen", MM_ICON_DIR "gItemIconBremenMaskTex",
                      MM_NAME_DIR "gItemNameBremenMaskENGTex", MM_OBJECT_DIR "mask_bree/object_mask_bree_DL_0003C0",
                      MM_OBJECT_DIR "gi_mask20/gGiBremenMaskDL", PLAYER_MASK_NONE,
                      "You got the %rBremen Mask%w!&Hold %y\xA0%w and march. Whatever&falls in behind you is yours.",
                      "%rBremen Mask&%wPress %y\xA1%w to wear it.&Hold %y\xA0%w to march." },
    [MASK_BUNNY] = { "skijer.mask_bunny", "__OTR__textures/icon_item_static/gItemIconMaskBunnyHoodTex",
                     "__OTR__textures/item_name_static/gBunnyHoodItemNameENGTex", NULL,
                     "__OTR__objects/object_gi_rabit_mask/gGiBunnyHoodDL", PLAYER_MASK_BUNNY,
                     "You got the %rBunny Hood%w!&In Termina its ears do more&than flap: you run like a rabbit.",
                     "%rBunny Hood&%wPress %y\xA1%w to wear it.&Link runs half again as fast." },
    [MASK_DON_GERO] = { "skijer.mask_don_gero", MM_ICON_DIR "gItemIconDonGeroMaskTex",
                        MM_NAME_DIR "gItemNameDonGerosMaskENGTex", MM_OBJECT_DIR "mask_gero/gDonGeroMaskDL",
                        MM_OBJECT_DIR "gi_mask16/gGiDonGeroMaskFaceDL", PLAYER_MASK_NONE,
                        "You got %rDon Gero's Mask%w!&The frogs take their conductor&seriously. Find their log.",
                        "%rDon Gero's Mask&%wPress %y\xA1%w to wear it.&At the frogs' log, %y\xA1%w conducts." },
    [MASK_SCENTS] = { "skijer.mask_scents", MM_ICON_DIR "gItemIconMaskOfScentsTex",
                      MM_NAME_DIR "gItemNameMaskOfScentsENGTex",
                      MM_OBJECT_DIR "mask_bu_san/object_mask_bu_san_DL_000710",
                      MM_OBJECT_DIR "gi_mask22/gGiMaskOfScentsFaceDL", PLAYER_MASK_NONE,
                      "You got the %rMask of Scents%w!&A pig's nose, and a pig's nose&misses nothing.",
                      "%rMask of Scents&%wPress %y\xA1%w to wear it.&Stand still and Link sniffs the air." },
    [MASK_ROMANI] = { "skijer.mask_romani", MM_ICON_DIR "gItemIconRomaniMaskTex",
                      MM_NAME_DIR "gItemNameRomanisMaskENGTex",
                      MM_OBJECT_DIR "mask_romerny/object_mask_romerny_DL_0007A0",
                      MM_OBJECT_DIR "gi_mask10/gGiRomaniMaskCapDL", PLAYER_MASK_NONE,
                      "You got %rRomani's Mask%w!&Every cow in the land knows&the ranch it came from.",
                      "%rRomani's Mask&%wPress %y\xA1%w to wear it.&Cows give milk to %y\xA0%w, no song." },
    [MASK_CIRCUS_LEADER] = { "skijer.mask_circus_leader", MM_ICON_DIR "gItemIconCircusLeaderMaskTex",
                             MM_NAME_DIR "gItemNameCircusLeadersMaskENGTex",
                             MM_OBJECT_DIR "mask_zacho/object_mask_zacho_DL_000700",
                             MM_OBJECT_DIR "gi_mask11/gGiCircusLeaderMaskFaceDL", PLAYER_MASK_NONE,
                             "You got the %rCircus Leader's Mask%w!&A face so sad that everyone&who sees it weeps.",
                             "%rCircus Leader's Mask&%wPress %y\xA1%w to wear it." },
    [MASK_COUPLE] = { "skijer.mask_couple", MM_ICON_DIR "gItemIconCouplesMaskTex",
                      MM_NAME_DIR "gItemNameCouplesMaskENGTex",
                      MM_OBJECT_DIR "mask_meoto/object_mask_meoto_DL_0005A0",
                      MM_OBJECT_DIR "gi_mask13/gGiCouplesMaskFullDL", PLAYER_MASK_NONE,
                      "You got the %rCouple's Mask%w!&Two halves made whole. Wearing it&mends you as you walk.",
                      "%rCouple's Mask&%wPress %y\xA1%w to wear it.&Heals by day, restores magic by night." },
    [MASK_TRUTH] = { "skijer.mask_truth", "__OTR__textures/icon_item_static/gItemIconMaskTruthTex",
                     "__OTR__textures/item_name_static/gMaskofTruthItemNameENGTex", NULL,
                     "__OTR__objects/object_gi_truth_mask/gGiMaskOfTruthDL", PLAYER_MASK_TRUTH,
                     "You got the %rMask of Truth%w!&Stone and beast alike will&tell you what they know.",
                     "%rMask of Truth&%wPress %y\xA1%w to wear it.&Gossip Stones will speak." },
    [MASK_KAMARO] = { "skijer.mask_kamaro", MM_ICON_DIR "gItemIconKamaroMaskTex",
                      MM_NAME_DIR "gItemNameKamarosMaskENGTex",
                      MM_OBJECT_DIR "mask_dancer/object_mask_dancer_DL_000EF0",
                      MM_OBJECT_DIR "gi_mask17/gGiKamaroMaskDL", PLAYER_MASK_NONE,
                      "You got %rKamaro's Mask%w!&A dancer's last lesson. Hold %y\xA0%w&and pass it on.",
                      "%rKamaro's Mask&%wPress %y\xA1%w to wear it.&Hold %y\xA0%w to dance." },
    [MASK_GIBDO] = { "skijer.mask_gibdo", MM_ICON_DIR "gItemIconGibdoMaskTex",
                     MM_NAME_DIR "gItemNameGibdoMaskENGTex", MM_OBJECT_DIR "mask_gibudo/object_mask_gibudo_DL_000250",
                     MM_OBJECT_DIR "gi_mask15/gGiGibdoMaskDL", PLAYER_MASK_NONE,
                     "You got the %rGibdo Mask%w!&The dead take you for one of&their own and let you pass.",
                     "%rGibdo Mask&%wPress %y\xA1%w to wear it.&Redeads and Gibdos ignore you." },
    [MASK_CAPTAIN] = { "skijer.mask_captain", MM_ICON_DIR "gItemIconCaptainsHatTex",
                       MM_NAME_DIR "gItemNameCaptainsHatENGTex", MM_OBJECT_DIR "mask_skj/object_mask_skj_DL_0009F0",
                       MM_OBJECT_DIR "gi_mask18/gGiCaptainsHatBodyDL", PLAYER_MASK_NONE,
                       "You got the %rCaptain's Hat%w!&The bones of the field answer&to whoever wears it.",
                       "%rCaptain's Hat&%wPress %y\xA1%w to wear it.&Stalchildren serve instead of attack." },
    [MASK_GIANT] = { "skijer.mask_giant", MM_ICON_DIR "gItemIconGiantsMaskTex",
                     MM_NAME_DIR "gItemNameGiantsMaskENGTex",
                     MM_OBJECT_DIR "mask_kyojin/object_mask_kyojin_DL_000380",
                     MM_OBJECT_DIR "gi_mask23/gGiGiantMaskDL", PLAYER_MASK_NONE,
                     "You got the %rGiant's Mask%w!&Put it on and the world gets&small. It costs magic to stay big.",
                     "%rGiant's Mask&%wPress %y\xA1%w to grow.&Drains magic until it runs out." },
};

static const ALIGN_ASSET(2) char sBlastCooldownDL[] =
    MM_OBJECT_DIR "mask_bakuretu/object_mask_bakuretu_DL_000440";
static const ALIGN_ASSET(2) char sBremenMarchAnim[] = MM_ANIM_DIR "clink_normal_okarina_walkB_Data";
static const ALIGN_ASSET(2) char sKamaroDanceAnim[] = MM_ANIM_DIR "alink_dance_loop_Data";
static const ALIGN_ASSET(2) char sScentsSniffAnim[] = MM_ANIM_DIR "cl_msbowait_Data";
static const ALIGN_ASSET(2) char sGiantSetMaskAnim[] = MM_ANIM_DIR "cl_setmask_Data";
static const ALIGN_ASSET(2) char sGiantSetMaskEndAnim[] = MM_ANIM_DIR "cl_setmaskend_Data";

// D_801C0BC0 / D_801C0BD0: what MM binds to segment 0x09 around the Blast Mask's two display lists.
static Gfx sBlastDefaultSeg9[] = {
    gsDPSetEnvColor(0, 0, 0, 255),
    gsSPEndDisplayList(),
};

static Gfx sBlastXluSeg9[] = {
    gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          G_RM_FOG_SHADE_A,
                      AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                          GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsSPEndDisplayList(),
};

typedef struct {
    uint16_t scene;
    uint8_t room;
    bool forChild;
    Vec3s pos;
    Vec3s rot;
    int16_t params;
} NightSkulltula;

// The gold skulltulas OoT only spawns after dark, which is exactly what the All-Night Mask denies.
static const NightSkulltula sNightSkulltulas[] = {
    { SCENE_GRAVEYARD, 1, true, { 156, 315, 795 }, { 16384, -32768, 0 }, -20096 },
    { SCENE_ZORAS_FOUNTAIN, 0, true, { -1891, 187, 1911 }, { 16384, 18022, 0 }, -19964 },
    { SCENE_GERUDOS_FORTRESS, 0, false, { 1598, 999, -2008 }, { 16384, -16384, 0 }, -19198 },
    { SCENE_GERUDOS_FORTRESS, 1, false, { 3377, 1734, -4935 }, { 16384, 0, 0 }, -19199 },
    { SCENE_KAKARIKO_VILLAGE, 0, false, { -18, 540, 1800 }, { 0, -32768, 0 }, -20160 },
    { SCENE_KAKARIKO_VILLAGE, 0, true, { -465, 377, -888 }, { 0, 28217, 0 }, -20222 },
    { SCENE_KAKARIKO_VILLAGE, 0, true, { 5, 686, -171 }, { 0, -32768, 0 }, -20220 },
    { SCENE_KAKARIKO_VILLAGE, 0, true, { 324, 270, 905 }, { 16384, 0, 0 }, -20216 },
    { SCENE_KAKARIKO_VILLAGE, 0, true, { -602, 120, 1120 }, { 16384, 0, 0 }, -20208 },
    { SCENE_LON_LON_RANCH, 0, true, { -2344, 180, 672 }, { 16384, 22938, 0 }, -29695 },
    { SCENE_LON_LON_RANCH, 0, true, { 808, 48, 326 }, { 16384, 0, 0 }, -29694 },
    { SCENE_LON_LON_RANCH, 0, true, { 997, 286, -2698 }, { 16384, -16384, 0 }, -29692 },
};

// z_en_fr.c sSongIndex: which eventChkInf[13] bit each of the seven frog songs claims.
static const uint8_t sFrogSongShifts[FROG_SONG_COUNT] = {
    EVENTCHKINF_SONGS_FOR_FROGS_ZL_SHIFT,     EVENTCHKINF_SONGS_FOR_FROGS_EPONA_SHIFT,
    EVENTCHKINF_SONGS_FOR_FROGS_SARIA_SHIFT,  EVENTCHKINF_SONGS_FOR_FROGS_SUNS_SHIFT,
    EVENTCHKINF_SONGS_FOR_FROGS_SOT_SHIFT,    EVENTCHKINF_SONGS_FOR_FROGS_STORMS_SHIFT,
    EVENTCHKINF_SONGS_FOR_FROGS_CHOIR_SHIFT,
};

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnPlayerPostLimbDraw",
    "OnPlayerResolveMotionScale",
    "OnActorUpdate",
    "OnActorResolvePlayerRelation",
    "OnActorPlaySfx",
    "OnSceneInit",
    "OnLoadGame",
    "OnOpenText",
    "OnInterfaceDrawEnd",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static int32_t sWorn = MASK_NONE;
static int8_t sResourcePresent[MASK_GRID_SIZE];
static bool sLentPersistentMasks;
static int32_t sPersistentMasksBefore;

static int32_t sBlastCooldown;
static int32_t sNightSkulltulasSpawned;
static int32_t sCoupleTimer;
static int32_t sCaptainTimer;
static int32_t sGiantMagicTimer;
static int32_t sGiantTransformTimer = -1;
static int16_t sGiantFillAlpha;
static bool sGiantHeldEndPose;
static bool sGiantScaleSnapped;
static bool sBremenMarching;
static int32_t sBremenMarchFrames;
static int32_t sBremenCuccoCooldown;
static bool sKamaroDancing;
static int32_t sKamaroDaruniaFrames;
static bool sScentsSniffing;
static bool sDonGeroOffering;
static int32_t sDonGeroReward;
static f32 sPoseFrame;

// Plain exported symbols of the game with no public header.
u8 ResourceMgr_FileExists(const char* resourcePath);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* player, PlayState* play);
void Player_AnimPlayLoop(PlayState* play, Player* player, LinkAnimationHeader* anim);
LinkAnimationHeader* Player_GetIdleAnim(Player* player);

// ---- resources ----

#define MM_ANIM_SLOTS 5
static LinkAnimationHeader sMmAnims[MM_ANIM_SLOTS];
static int16_t* sMmFrames[MM_ANIM_SLOTS];
static LinkAnimationHeader* sMarchAnim;
static LinkAnimationHeader* sDanceAnim;
static LinkAnimationHeader* sSniffAnim;
static LinkAnimationHeader* sSetMaskAnim;
static LinkAnimationHeader* sSetMaskEndAnim;
static bool sAnimsLoaded;

// A Majora animation walks its root where Ocarina's does not, so the base translation is forced back on a
// private copy — the resource itself is shared with the whole game.
static LinkAnimationHeader* LoadMmAnim(const char* path, int32_t slot) {
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }

    LinkAnimationHeader* source = ResourceMgr_LoadPlayerAnimAsHeader(path);
    if (source == NULL || source->common.frameCount <= 0) {
        return NULL;
    }

    size_t values = (size_t)source->common.frameCount * MM_ANIM_VALUES_PER_FRAME;
    int16_t* frames = (int16_t*)malloc(values * sizeof(int16_t));
    if (frames == NULL) {
        return NULL;
    }
    memcpy(frames, source->segment, values * sizeof(int16_t));
    for (int32_t frame = 0; frame < source->common.frameCount; frame++) {
        int16_t* root = &frames[frame * MM_ANIM_VALUES_PER_FRAME];

        root[0] = MM_ANIM_BASE_TRANSL_X;
        root[2] = 0;
    }

    free(sMmFrames[slot]);
    sMmFrames[slot] = frames;
    sMmAnims[slot].common.frameCount = source->common.frameCount;
    sMmAnims[slot].segment = frames;
    return &sMmAnims[slot];
}

static void LoadAnims(void) {
    if (sAnimsLoaded) {
        return;
    }
    sMarchAnim = LoadMmAnim(sBremenMarchAnim, 0);
    sDanceAnim = LoadMmAnim(sKamaroDanceAnim, 1);
    sSniffAnim = LoadMmAnim(sScentsSniffAnim, 2);
    sSetMaskAnim = LoadMmAnim(sGiantSetMaskAnim, 3);
    sSetMaskEndAnim = LoadMmAnim(sGiantSetMaskEndAnim, 4);
    sAnimsLoaded = sDanceAnim != NULL && sMarchAnim != NULL;
}

// Drawing a display list that no archive provides jumps into its own path string.
static bool HasWornModel(int32_t index) {
    const char* path = sMasks[index].wornDL;

    if (path == NULL) {
        return false;
    }
    if (sResourcePresent[index] == 0) {
        sResourcePresent[index] = ResourceMgr_FileExists(path) ? 1 : -1;
    }
    return sResourcePresent[index] > 0;
}

// ---- worn state ----

static bool IsWearing(int32_t index) {
    return sWorn == index;
}

static bool AnswersButtonB(void) {
    return IsWearing(MASK_BLAST) || IsWearing(MASK_BREMEN) || IsWearing(MASK_KAMARO) || IsWearing(MASK_POSTMAN) ||
           IsWearing(MASK_GREAT_FAIRY);
}

// A mask that answers B takes it from the player, so neither the sword nor a form's own B move fires under it.
static void ClaimButtonB(void) {
    if (AnswersButtonB()) {
        sApi->BlockPlayerInput(MASK_BUTTON_OWNER, BTN_B, false);
    } else {
        sApi->ReleasePlayerInput(MASK_BUTTON_OWNER);
    }
}

static void LendPersistentMasks(bool shouldLend) {
    if (shouldLend == sLentPersistentMasks) {
        return;
    }
    if (shouldLend) {
        sPersistentMasksBefore = CVarGetInteger(PERSISTENT_MASKS_CVAR, 0);
        CVarSetInteger(PERSISTENT_MASKS_CVAR, 1);
    } else {
        CVarSetInteger(PERSISTENT_MASKS_CVAR, sPersistentMasksBefore);
    }
    sLentPersistentMasks = shouldLend;
}

static void StopBremenMarch(PlayState* play, Player* player) {
    if (!sBremenMarching) {
        return;
    }
    sBremenMarching = false;
    sBremenMarchFrames = 0;
    sPoseFrame = 0.0f;
    LinkAnimation_PlayLoop(play, &player->skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_normal_wait);
}

// The dance owns Link's action func: an action left running under it would wait for a clip it no longer has.
static void KamaroDanceAction(Player* player, PlayState* play) {
    player->linearVelocity = 0.0f;
}

static void StopKamaroDance(PlayState* play, Player* player) {
    sKamaroDancing = false;
    sKamaroDaruniaFrames = 0;
    if (player->actionFunc != KamaroDanceAction) {
        return;
    }
    func_80839FFC(player, play);
    Player_AnimPlayLoop(play, player, Player_GetIdleAnim(player));
}

// The white of the transformation goes through the engine's own fill, which draws under the interface.
static void ApplyGiantFill(PlayState* play) {
    if (sGiantFillAlpha <= 0) {
        play->envCtx.fillScreen = false;
        return;
    }
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 255;
    play->envCtx.screenFillColor[1] = 255;
    play->envCtx.screenFillColor[2] = 255;
    play->envCtx.screenFillColor[3] = (u8)sGiantFillAlpha;
}

static void ClearGiant(Player* player) {
    sGiantMagicTimer = 0;
    sGiantTransformTimer = -1;
    sGiantFillAlpha = 0;
    sGiantHeldEndPose = false;
    sGiantScaleSnapped = false;
    player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
    Actor_SetScale(&player->actor, GIANT_DEFAULT_SCALE);
}

static void TakeOff(PlayState* play, Player* player) {
    if (sWorn == MASK_NONE) {
        return;
    }
    if (sMasks[sWorn].vanillaMask != PLAYER_MASK_NONE) {
        player->currentMask = PLAYER_MASK_NONE;
        LendPersistentMasks(false);
    }
    if (sWorn == MASK_GIANT) {
        ClearGiant(player);
        ApplyGiantFill(play);
    }
    StopBremenMarch(play, player);
    StopKamaroDance(play, player);
    sScentsSniffing = false;
    sCaptainTimer = 0;
    sDonGeroOffering = false;
    sWorn = MASK_NONE;
    ClaimButtonB();
}

static void ResetFairyHair(void);

static void PutOn(Player* player, int32_t index) {
    if (index == MASK_GREAT_FAIRY) ResetFairyHair();
    sWorn = index;
    sPoseFrame = 0.0f;

    if (sMasks[index].vanillaMask != PLAYER_MASK_NONE) {
        // Vanilla clears currentMask every frame the matching mask item is not on a button, and this one never
        // is: lending PersistentMasks is what keeps the slot until the mask comes off.
        LendPersistentMasks(true);
        player->currentMask = sMasks[index].vanillaMask;
    }
    if (index == MASK_GIANT) {
        sGiantMagicTimer = GIANT_MAGIC_INTERVAL;
        sGiantTransformTimer = 0;
        sGiantFillAlpha = 0;
        player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
        player->linearVelocity = 0.0f;
    }
    ClaimButtonB();
}

// Majora refuses the Giant's Mask on an empty meter, because its whole effect is magic.
static void ToggleMask(PlayState* play, Player* player, int32_t index) {
    bool wearingIt = IsWearing(index);

    if (!wearingIt && index == MASK_GIANT && gSaveContext.magic <= 0) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }

    TakeOff(play, player);
    if (!wearingIt) {
        PutOn(player, index);
    }
    Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
    player->stateFlags2 |= PLAYER_STATE2_FOOTSTEP;
}

#define DECLARE_WEAR_FUNC(index)                                        \
    static void Wear_##index(PlayState* play, Player* player) {         \
        ToggleMask(play, player, index);                                \
    }
MM_WEARABLE_SLOTS(DECLARE_WEAR_FUNC)
#undef DECLARE_WEAR_FUNC

// ---- drawing ----

// The Great Fairy's Mask reads six matrices off segment 0x0B for its hair; without them it walks a pointer
// into whatever the last mod left there.
#include "fairy_hair.inc.c"

static void ResetFairyHair(void) {
    sFairyHairInited = false;
    sHairLastPhysicsFrame = UINT32_MAX;
}

static void BindFairyHairMatrices(PlayState* play, Player* player) {
    Mtx* matrices = (Mtx*)Graph_Alloc(play->state.gfxCtx, FAIRY_HAIR_MATRICES * sizeof(Mtx));

    if (matrices == NULL) {
        return;
    }
    FairyHair_ComputeMatrices(play, player, matrices);

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, FAIRY_HAIR_SEGMENT, (uintptr_t)matrices);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Player_DrawBlastMask: the recovering mask crossfades a scrolling display list over the normal one.
static void DrawBlastMask(PlayState* play, const char* dlPath) {
    OPEN_DISPS(play->state.gfxCtx);

    if (sBlastCooldown > 0) {
        int32_t alpha = sBlastCooldown <= BLAST_FADE_FRAMES ? (sBlastCooldown * 255) / BLAST_FADE_FRAMES : 255;

        gSPSegment(POLY_OPA_DISP++, 0x08,
                   (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, play->gameplayFrames, play->gameplayFrames, 32,
                                               32, 1, play->gameplayFrames * 3,
                                               (u32) - (int32_t)(play->gameplayFrames * 2), 32, 32));
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, (u8)alpha);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBlastCooldownDL);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sBlastXluSeg9);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, (u8)(255 - alpha));
    } else {
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sBlastDefaultSeg9);
    }
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlPath);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Majora's mask display lists leave their own env colour and a lighting-less geometry mode behind, which
// would black out the chest limb drawn right after.
static void RestoreLimbState(PlayState* play, Player* player) {
    Color_RGB8 tunic = Player_GetTunicColor(player->currentTunic);

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, tunic.r, tunic.g, tunic.b, 0);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawWornMask(PlayState* play, Player* player, int32_t limbIndex) {
    if (limbIndex != PLAYER_LIMB_HEAD || sWorn == MASK_NONE || !HasWornModel(sWorn)) {
        return;
    }

    const char* dlPath = sMasks[sWorn].wornDL;

    if (sWorn == MASK_BLAST) {
        DrawBlastMask(play, dlPath);
    } else {
        if (sWorn == MASK_GREAT_FAIRY) {
            BindFairyHairMatrices(play, player);
        }
        OPEN_DISPS(play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dlPath);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    RestoreLimbState(play, player);
}

// ---- what other actors know about Link ----

static bool IsHiddenFromActor(Actor* actor) {
    if (IsWearing(MASK_STONE)) {
        return actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_NPC ||
               (actor->flags & ACTOR_FLAG_HOSTILE);
    }
    if (IsWearing(MASK_GIBDO)) {
        return actor->id == ACTOR_EN_RD;
    }
    if (IsWearing(MASK_CAPTAIN)) {
        return actor->id == ACTOR_EN_SKB || actor->id == ACTOR_EN_TEST;
    }
    return false;
}

// The single point where vanilla decides what an actor knows about Link this frame. The yaw has to point
// away from him too: fifty-four sites ask Actor_IsFacingPlayer and never look at the distance.
static void HidePlayerFromActor(Actor* actor, float* xzDist, float* yDist, int16_t* yawTowards) {
    if (!IsHiddenFromActor(actor)) {
        return;
    }
    *xzDist = HIDDEN_DISTANCE;
    *yDist = HIDDEN_DISTANCE;
    *yawTowards = actor->shape.rot.y + 0x8000;
}

// An enemy that cannot find Link has nothing to shout about.
static void SilenceHiddenActor(Actor* actor, int32_t kind, uint16_t* sfxId, bool* handled) {
    if (kind != SOH_ACTOR_SFX_GENERIC && kind != SOH_ACTOR_SFX_FLAGGED) {
        return;
    }
    if (actor->category != ACTORCAT_ENEMY && !(actor->flags & ACTOR_FLAG_HOSTILE)) {
        return;
    }
    if (IsHiddenFromActor(actor)) {
        *handled = true;
    }
}

// ---- per-mask behaviour ----

static void PlayPoseLoop(PlayState* play, Player* player, LinkAnimationHeader* anim) {
    LinkAnimation_PlayLoop(play, &player->skelAnime, anim);
    player->skelAnime.curFrame = sPoseFrame;
    AnimationContext_SetLoadFrame(play, anim, (int32_t)sPoseFrame, player->skelAnime.limbCount,
                                  player->skelAnime.jointTable);
    sPoseFrame += 1.0f;
    if (sPoseFrame >= player->skelAnime.animLength) {
        sPoseFrame = 0.0f;
    }
}

static bool IsPlayerBusy(Player* player) {
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM |
                                   PLAYER_STATE1_TALKING)) != 0;
}

static void SpawnNightSkulltulas(PlayState* play) {
    for (int32_t i = 0; i < ARRAY_COUNT(sNightSkulltulas); i++) {
        const NightSkulltula* gs = &sNightSkulltulas[i];

        if (gs->scene != play->sceneNum || gs->room != play->roomCtx.curRoom.num ||
            gs->forChild != (bool)LINK_IS_CHILD) {
            continue;
        }
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_SW, gs->pos.x, gs->pos.y, gs->pos.z, gs->rot.x, gs->rot.y,
                    gs->rot.z, gs->params);
    }
}

static void UpdateAllNightMask(PlayState* play) {
    if (!IS_DAY || sNightSkulltulasSpawned) {
        return;
    }
    sNightSkulltulasSpawned = 1;
    SpawnNightSkulltulas(play);
}

// En_Sw shrinks itself to nothing during the day; the mask holds it open.
static void KeepSkulltulaAwake(void* actorRef) {
    Actor* actor = (Actor*)actorRef;

    if (!IsWearing(MASK_ALL_NIGHT) || !IS_DAY) {
        return;
    }
    Actor_SetScale(actor, NIGHT_SKULLTULA_SCALE);
}

// The bomb runs its first update before any draw can place its collider, so the explosion sphere is set here.
static void DetonateBlastMask(PlayState* play, Player* player) {
    Vec3f at = player->actor.world.pos;
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, at.x, at.y, at.z, 0, 0, 0, BOMB_BODY);

    if (bomb == NULL) {
        return;
    }
    bomb->timer = 1;
    Actor_SetScale(&bomb->actor, BOMB_SCALE);
    bomb->explosionCollider.elements[0].dim.worldSphere.center.x = (int16_t)at.x;
    bomb->explosionCollider.elements[0].dim.worldSphere.center.y = (int16_t)at.y;
    bomb->explosionCollider.elements[0].dim.worldSphere.center.z = (int16_t)at.z;
    sBlastCooldown = BLAST_COOLDOWN_FRAMES;
}

static void UpdateBlastMask(PlayState* play, Player* player) {
    if (sBlastCooldown > 0) {
        sBlastCooldown--;
        return;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && !IsPlayerBusy(player)) {
        DetonateBlastMask(play, player);
    }
}

#include "mask_travel.inc.c"
#include "mask_effects.inc.c"

static void UpdateGreatFairyMask(PlayState* play, Player* player) {
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        OpenTravelMap(play, player, TRAVEL_FOUNTAIN);
    }
    if (play->sceneNum != SCENE_GREAT_FAIRYS_FOUNTAIN_MAGIC &&
        play->sceneNum != SCENE_GREAT_FAIRYS_FOUNTAIN_SPELLS) {
        return;
    }
    if (IsPlayerBusy(player) || !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return;
    }
    Flags_SetSwitch(play, GREAT_FAIRY_REWARD_SWITCH);
}

static void UpdateBremenMask(PlayState* play, Player* player) {
    if (sBremenCuccoCooldown > 0) {
        sBremenCuccoCooldown--;
    }
    if (IsPlayerBusy(player) || sMarchAnim == NULL) {
        StopBremenMarch(play, player);
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B) || (player->stateFlags1 & PLAYER_STATE1_DAMAGED)) {
        StopBremenMarch(play, player);
        return;
    }
    if (!sBremenMarching) {
        if (!(player->actor.bgCheckFlags & BG_ON_GROUND)) {
            return;
        }
        sBremenMarching = true;
        sBremenMarchFrames = 0;
        sPoseFrame = 0.0f;
    }

    sBremenMarchFrames++;
    if (sBremenMarchFrames == BREMEN_CUCCO_AT_FRAME && sBremenCuccoCooldown == 0) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_NIW, player->actor.world.pos.x, player->actor.world.pos.y,
                    player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, 0);
        sBremenCuccoCooldown = BREMEN_CUCCO_COOLDOWN;
    }

    PlayPoseLoop(play, player, sMarchAnim);

    int16_t yawTarget = player->yaw;
    f32 speedTarget;

    Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, SPEED_MODE_CURVED, play);
    Math_ScaledStepToS(&player->yaw, yawTarget, BREMEN_TURN_RATE);
    player->linearVelocity = BREMEN_MARCH_SPEED;
    player->actor.velocity.x = Math_SinS(player->yaw) * BREMEN_MARCH_SPEED;
    player->actor.velocity.z = Math_CosS(player->yaw) * BREMEN_MARCH_SPEED;
    player->actor.shape.rot.y = player->yaw;
}

static void OfferFrogReward(PlayState* play, Player* player) {
    if (Actor_HasParent(&player->actor, play)) {
        player->actor.parent = NULL;
        sDonGeroOffering = false;
        return;
    }
    Actor_OfferGetItem(&player->actor, play, sDonGeroReward, 30.0f, 100.0f);
}

// Conducting the choir claims every song the frogs still owe at once; the flag setter is what the randomizer
// listens to, so a shuffled seed delivers its own items through it.
static void ConductFrogChoir(PlayState* play, Player* player) {
    int32_t reward = GI_NONE;

    for (int32_t i = 0; i < FROG_SONG_COUNT; i++) {
        int32_t flag = (EVENTCHKINF_SONGS_FOR_FROGS_INDEX << 4) + sFrogSongShifts[i];

        if (Flags_GetEventChkInf(flag)) {
            continue;
        }
        Flags_SetEventChkInf(flag);
        reward = (i >= 5) ? GI_HEART_PIECE : (reward == GI_HEART_PIECE ? reward : GI_RUPEE_PURPLE);
    }

    if (reward == GI_NONE || IS_RANDO) {
        return;
    }
    sDonGeroReward = reward;
    sDonGeroOffering = true;
    Actor_OfferGetItem(&player->actor, play, reward, 30.0f, 100.0f);
}

static void UpdateDonGeroMask(PlayState* play, Player* player) {
    if (play->sceneNum != SCENE_ZORAS_RIVER) {
        sDonGeroOffering = false;
        return;
    }
    if (sDonGeroOffering) {
        OfferFrogReward(play, player);
        return;
    }

    f32 dx = player->actor.world.pos.x - FROG_LOG_X;
    f32 dz = player->actor.world.pos.z - FROG_LOG_Z;

    if ((dx * dx + dz * dz) > FROG_LOG_RADIUS_SQ || IsPlayerBusy(player)) {
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return;
    }
    ConductFrogChoir(play, player);
}

static void UpdateScentsMask(PlayState* play, Player* player) {
    bool idle = (player->linearVelocity < SCENTS_IDLE_SPEED) && (player->actor.bgCheckFlags & BG_ON_GROUND);

    if (!idle || IsPlayerBusy(player) || sSniffAnim == NULL) {
        sScentsSniffing = false;
        return;
    }
    if (!sScentsSniffing) {
        sScentsSniffing = true;
        sPoseFrame = 0.0f;
    }
    PlayPoseLoop(play, player, sSniffAnim);
}

static void UpdateCoupleMask(void) {
    sCoupleTimer--;
    if (sCoupleTimer > 0) {
        return;
    }
    if (IS_DAY) {
        sCoupleTimer = COUPLE_HEALTH_INTERVAL;
        if (gSaveContext.health < gSaveContext.healthCapacity) {
            gSaveContext.health++;
        }
        return;
    }
    sCoupleTimer = COUPLE_MAGIC_INTERVAL;
    if (gSaveContext.magic < gSaveContext.magicCapacity) {
        gSaveContext.magic++;
    }
}

static Actor* FindDarunia(PlayState* play) {
    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_NPC].head; it != NULL; it = it->next) {
        if (it->id == ACTOR_EN_DU && it->xzDistToPlayer < KAMARO_DANCE_RADIUS) {
            return it;
        }
    }
    return NULL;
}

// Dancing next to Darunia counts as Saria's Song: his own ocarina check takes it from here, so the cutscene is
// running before he waits for it to end and his reward is handed over as vanilla does.
static void DanceForDarunia(PlayState* play, Player* player) {
    if (play->sceneNum != SCENE_GORON_CITY || Flags_GetRandomizerInf(RAND_INF_DARUNIAS_JOY)) {
        return;
    }
    EnDu* darunia = (EnDu*)FindDarunia(play);
    if (darunia == NULL) {
        sKamaroDaruniaFrames = 0;
        return;
    }
    sKamaroDaruniaFrames++;
    if (sKamaroDaruniaFrames < KAMARO_DARUNIA_FRAMES) {
        return;
    }
    StopKamaroDance(play, player);
    play->msgCtx.ocarinaMode = OCARINA_MODE_03;
    darunia->actionFunc = func_809FE4A4;
}

static void UpdateKamaroMask(PlayState* play, Player* player) {
    Input* input = &play->state.input[0];
    bool canDance = sDanceAnim != NULL && !IsPlayerBusy(player) && player->csAction == 0;

    if (!sKamaroDancing) {
        bool isGrounded = (player->actor.bgCheckFlags & BG_ON_GROUND) != 0;
        if (!canDance || !isGrounded || !CHECK_BTN_ALL(input->press.button, BTN_B)) {
            return;
        }
        sKamaroDancing = true;
        sPoseFrame = 0.0f;
        Player_SetupAction(play, player, KamaroDanceAction, 0);
    }
    // Damage or a cutscene can take Link away from the dance without B being let go.
    if (!canDance || player->actionFunc != KamaroDanceAction || !CHECK_BTN_ALL(input->cur.button, BTN_B)) {
        StopKamaroDance(play, player);
        return;
    }
    PlayPoseLoop(play, player, sDanceAnim);
    DanceForDarunia(play, player);
}

static int32_t CountCaptainMinions(PlayState* play) {
    int32_t count = 0;

    for (Actor* it = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; it != NULL; it = it->next) {
        if ((it->id == ACTOR_EN_SKB || it->id == ACTOR_EN_TEST) && it->home.rot.z == CAPTAIN_MINION_TAG) {
            count++;
        }
    }
    return count;
}

static void UpdateCaptainsHat(PlayState* play, Player* player) {
    if (play->sceneNum != SCENE_HYRULE_FIELD || IS_DAY || IsPlayerBusy(player)) {
        sCaptainTimer = 0;
        return;
    }

    sCaptainTimer++;
    if (sCaptainTimer < CAPTAIN_SPAWN_INTERVAL) {
        return;
    }
    if (CountCaptainMinions(play) >= CAPTAIN_MAX_MINIONS) {
        sCaptainTimer = CAPTAIN_RETRY_INTERVAL;
        return;
    }

    int16_t angle = (int16_t)(Rand_ZeroOne() * 65536.0f);
    f32 distance = CAPTAIN_SPAWN_NEAR + Rand_ZeroOne() * CAPTAIN_SPAWN_SPREAD;
    f32 x = player->actor.world.pos.x + Math_SinS(angle) * distance;
    f32 z = player->actor.world.pos.z + Math_CosS(angle) * distance;
    int16_t id = LINK_IS_ADULT ? ACTOR_EN_TEST : ACTOR_EN_SKB;
    int16_t params = LINK_IS_ADULT ? 0 : CAPTAIN_GIANT_STALCHILD_PARAMS;
    Actor* minion =
        Actor_Spawn(&play->actorCtx, play, id, x, player->actor.world.pos.y, z, 0, angle, 0, params);

    if (minion != NULL) {
        minion->home.rot.z = CAPTAIN_MINION_TAG;
    }
    sCaptainTimer = 0;
}

// Player_Action_89: Link raises the mask to his face, the screen fills white, and the new size snaps in
// behind it.
static void UpdateGiantTransform(PlayState* play, Player* player) {
    sGiantTransformTimer++;
    player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
    player->linearVelocity = 0.0f;

    LinkAnimationHeader* anim = sGiantHeldEndPose ? sSetMaskEndAnim : sSetMaskAnim;
    if (anim != NULL) {
        PlayPoseLoop(play, player, anim);
        if (sPoseFrame == 0.0f && !sGiantHeldEndPose) {
            sGiantHeldEndPose = true;
        }
    }

    if (sGiantTransformTimer < GIANT_FILL_START) {
        Actor_SetScale(&player->actor, GIANT_DEFAULT_SCALE);
        return;
    }

    if (!sGiantScaleSnapped) {
        sGiantFillAlpha += GIANT_FILL_IN_STEP;
        if (sGiantFillAlpha < 255) {
            Actor_SetScale(&player->actor, GIANT_DEFAULT_SCALE);
            return;
        }
        sGiantFillAlpha = 255;
        sGiantScaleSnapped = true;
        Actor_SetScale(&player->actor, GIANT_SCALE);
        return;
    }

    sGiantFillAlpha -= GIANT_FILL_OUT_STEP;
    Actor_SetScale(&player->actor, GIANT_SCALE);
    if (sGiantFillAlpha > 0) {
        return;
    }
    sGiantFillAlpha = 0;
    sGiantTransformTimer = -1;
    sGiantHeldEndPose = false;
    sGiantScaleSnapped = false;
    player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
}

static void UpdateGiantsMask(PlayState* play, Player* player) {
    if (sGiantTransformTimer >= 0) {
        UpdateGiantTransform(play, player);
        ApplyGiantFill(play);
        return;
    }

    // Everything else in the game resets the player scale, so the giant size is re-asserted every frame.
    Actor_SetScale(&player->actor, GIANT_SCALE);

    if (sGiantMagicTimer > 0) {
        sGiantMagicTimer--;
    } else {
        sGiantMagicTimer = GIANT_MAGIC_INTERVAL;
        if (gSaveContext.magic > 0) {
            gSaveContext.magic--;
        }
    }

    if (gSaveContext.magic > 0) {
        return;
    }
    gSaveContext.magic = 0;
    TakeOff(play, player);
    Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
}

static void UpdateWornMask(void) {
    PlayState* play = gPlayState;

    if (play == NULL) return;
    Player* player = GET_PLAYER(play);
    if (!sAnimsLoaded && (play->gameplayFrames % 60) == 0) LoadAnims();
    if (UpdateTravelMap(play)) return;
    UpdatePostmanMailboxes(play, player);
    if (sTravelMode != TRAVEL_NONE || sWorn == MASK_NONE) return;

    // The lent mask slot is what makes every vanilla reaction work, and a load or a cutscene can drop it.
    if (sMasks[sWorn].vanillaMask != PLAYER_MASK_NONE) {
        player->currentMask = sMasks[sWorn].vanillaMask;
    }

    switch (sWorn) {
        case MASK_ALL_NIGHT:
            UpdateAllNightMask(play);
            break;
        case MASK_BLAST:
            UpdateBlastMask(play, player);
            break;
        case MASK_GREAT_FAIRY:
            UpdateGreatFairyMask(play, player);
            break;
        case MASK_BREMEN:
            UpdateBremenMask(play, player);
            break;
        case MASK_DON_GERO:
            UpdateDonGeroMask(play, player);
            break;
        case MASK_SCENTS:
            UpdateScentsMask(play, player);
            break;
        case MASK_COUPLE:
            UpdateCoupleMask();
            break;
        case MASK_KAMARO:
            UpdateKamaroMask(play, player);
            break;
        case MASK_CAPTAIN:
            UpdateCaptainsHat(play, player);
            break;
        case MASK_GIANT:
            UpdateGiantsMask(play, player);
            break;
        default:
            break;
    }
}

// ---- reactions the masks borrow from vanilla ----

// The vanilla enhancement already grants this boost when it is on, so the mask only adds what is missing.
static void ScaleBunnyHoodSpeed(Player* player, int32_t kind, float* scale) {
    if (kind != SOH_PLAYER_MOTION_STICK_SPEED || !IsWearing(MASK_BUNNY)) {
        return;
    }
    if (CVarGetInteger(BUNNY_HOOD_CVAR, 0) == BUNNY_HOOD_FAST_AND_JUMP) {
        return;
    }
    *scale *= BUNNY_SPEED_SCALE;
}

// Romani's Mask asks a cow for milk the way Epona's Song does: DREG(53) is the flag the cow itself watches.
static void MilkCowForRomani(void* actorRef) {
    Actor* cow = (Actor*)actorRef;
    PlayState* play = gPlayState;

    if (!IsWearing(MASK_ROMANI) || play == NULL || cow->xzDistToPlayer >= COW_TALK_RADIUS) {
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return;
    }
    DREG(53) = 1;
}

static void KeepRedeadsCalm(bool* should, va_list args) {
    if (IsWearing(MASK_GIBDO) || IsWearing(MASK_CAPTAIN)) {
        *should = false;
    }
}

// ---- session state ----

bool MmWearables_IsCircusLeaderWorn(void) { return IsWearing(MASK_CIRCUS_LEADER); }

static void ForgetScene(int16_t sceneNum) {
    sTruthLastStone = NULL;
    sMailboxSpawned = 0;
    CloseTravelMap();
    sFairyHairInited = false;
    sHairLastPhysicsFrame = UINT32_MAX;
    sNightSkulltulasSpawned = 0;
    sCaptainTimer = 0;
    sDonGeroOffering = false;
    sBremenMarching = false;
    sBremenMarchFrames = 0;
    sKamaroDancing = false;
    sKamaroDaruniaFrames = 0;
    sScentsSniffing = false;
}

static void ForgetFile(int32_t fileNum) {
    LendPersistentMasks(false);
    sWorn = MASK_NONE;
    ClaimButtonB();
    sBlastCooldown = 0;
    sGiantTransformTimer = -1;
    sGiantFillAlpha = 0;
    sGiantHeldEndPose = false;
    sGiantScaleSnapped = false;
    ForgetScene(0);
}

// ---- registration ----

static void RegisterMask(int32_t index, SOHCustomItemInitFunc wear) {
    const WearableMask* mask = &sMasks[index];
    SOHCustomItemDefinition definition = Z64Items_Define(mask->key, mask->iconPath, mask->namePath);

    Z64Items_SetButtons(&definition, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    definition.flags |= SOH_CUSTOM_ITEM_INSTANT | SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&definition, MASK_PAGE, (uint8_t)index, 0);
    Z64Items_SetGetItemModel(&definition, mask->getItemDL);
    Z64Items_SetTextbox(&definition, mask->getItemText);
    Z64Items_SetPauseText(&definition, mask->pauseText);
    Z64Items_SetAction(&definition, wear, NULL);
    Z64Items_Register(sApi, &definition);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    LoadAnims();
    MmTax_Init(sApi);
    RegisterMaskEffects();
    RegisterMaskTravel();

#define REGISTER_WEARABLE(index) RegisterMask(index, Wear_##index);
    MM_WEARABLE_SLOTS(REGISTER_WEARABLE)
#undef REGISTER_WEARABLE

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateWornMask);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawWornMask);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ScaleBunnyHoodSpeed);
    SOH_REGISTER_HOOK(sApi, OnActorResolvePlayerRelation, HidePlayerFromActor);
    SOH_REGISTER_HOOK(sApi, OnActorPlaySfx, SilenceHiddenActor);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, ForgetFile);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_SW, KeepSkulltulaAwake);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_COW, MilkCowForRomani);

    sApi->RegisterVB(VB_REDEAD_GIBDO_FREEZE_LINK, KeepRedeadsCalm);
}

