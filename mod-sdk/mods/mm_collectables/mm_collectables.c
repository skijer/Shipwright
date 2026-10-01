// Majora's Mask Collectables: the four boss remains as wearable pieces, and the ten songs the
// ocarina of Ocarina of Time can be taught to answer.
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MM_COLLECTABLES_PAGE (SOH_ITEM_PAGE_QUEST_VANILLA + 1)
#define MM_LAYOUT_KEY "nei.mm_collectables"
#define REMAINS_FIRST_SLOT 0

#define SPRINT_SPEED_MULTIPLIER 2.0f
#define CHARGE_SPEED_MULTIPLIER 3.0f
#define SWIM_SPEED_MULTIPLIER 1.6f
#define CHARGE_MAGIC_INTERVAL 15
#define GIANT_DAMAGE_MULTIPLIER 2

#define STEP_SFX_INTERVAL 8
#define BG_ON_GROUND 1

// The ocarina hook reports the pitch the game gives the sequence player, not the note index.
#define NOTE_A 0x02
#define NOTE_C_DOWN 0x05
#define NOTE_C_RIGHT 0x09
#define NOTE_C_LEFT 0x0B
#define NOTE_C_UP 0x0E

#define SONG_MAX_NOTES 8

typedef enum {
    REMAINS_ODOLWA,
    REMAINS_GOHT,
    REMAINS_GYORG,
    REMAINS_TWINMOLD,
    REMAINS_COUNT,
} RemainsId;

typedef struct {
    const char* key;
    const char* iconPath;
    const char* namePath;
    const char* maskPath;
    f32 maskScale;
    f32 maskTranslateY;
    f32 maskTranslateZ;
    const char* getItemText;
    const char* pauseText;
} RemainsDefinition;

typedef struct {
    const char* key;
    const char* iconPath;
    const char* namePath;
    u8 noteCount;
    u8 notes[SONG_MAX_NOTES];
    const char* getItemText;
    const char* pauseText;
} SongDefinition;

static const ALIGN_ASSET(2) char sOdolwaIcon[] = "__OTR__textures/icon_item_custom/gItemIconOdolwasRemainsTex";
static const ALIGN_ASSET(2) char sGohtIcon[] = "__OTR__textures/icon_item_custom/gItemIconGohtsRemainsTex";
static const ALIGN_ASSET(2) char sGyorgIcon[] = "__OTR__textures/icon_item_custom/gItemIconGyorgsRemainsTex";
static const ALIGN_ASSET(2) char sTwinmoldIcon[] = "__OTR__textures/icon_item_custom/gItemIconTwinmoldsRemainsTex";

static const ALIGN_ASSET(2) char sOdolwaName[] = "__OTR__textures/item_name_custom/gOdolwasRemainsNameTex";
static const ALIGN_ASSET(2) char sGohtName[] = "__OTR__textures/item_name_custom/gGohtsRemainsNameTex";
static const ALIGN_ASSET(2) char sGyorgName[] = "__OTR__textures/item_name_custom/gGyorgsRemainsNameTex";
static const ALIGN_ASSET(2) char sTwinmoldName[] = "__OTR__textures/item_name_custom/gTwinmoldsRemainsNameTex";

// The only face-fitted remains geometry Majora's Mask has: what the moon children wear. It ships in
// mm.o2r, so every use is guarded and the piece simply stays invisible without it.
static const ALIGN_ASSET(2) char sOdolwaMask[] = "__OTR__objects/object_ob/gMoonChildOdolwasMaskDL";
static const ALIGN_ASSET(2) char sGohtMask[] = "__OTR__objects/object_ob/gMoonChildGohtsMaskDL";
static const ALIGN_ASSET(2) char sGyorgMask[] = "__OTR__objects/object_ob/gMoonChildGyorgsMaskDL";
static const ALIGN_ASSET(2) char sTwinmoldMask[] = "__OTR__objects/object_ob/gMoonChildTwinmoldsMaskDL";

// Every mask sits on Link's head with the same correction; only the per-mask base transform below
// differs. Measured on Majora's Mask's human Link, where the moon child's own values put the mask
// through the face.
#define MASK_ROT_X_DEGREES 180
#define MASK_ROT_Y_DEGREES (-90)
#define MASK_ROT_Z_DEGREES 15
#define MASK_OFFSET_Y (-510.0f)
#define MASK_OFFSET_Z 383.0f
#define MASK_SCALE_CORRECTION 1.04f

static const RemainsDefinition sRemains[REMAINS_COUNT] = {
    {
        "nei.mm_remains_odolwa",
        sOdolwaIcon,
        sOdolwaName,
        sOdolwaMask,
        0.50f,
        1400.0f,
        700.0f,
        "You got %rOdolwa's Remains%w!&The jungle warrior's mask still&keeps the beat of his dance.^"
        "Wear it and Link runs as Odolwa&danced: fast, and never rolling.",
        "%rOdolwa's Remains&%wPress %y\xA1%w to wear it.&Worn: Link sprints instead of rolling.",
    },
    {
        "nei.mm_remains_goht",
        sGohtIcon,
        sGohtName,
        sGohtMask,
        0.50f,
        1470.0f,
        900.0f,
        "You got %rGoht's Remains%w!&The mechanical bull's mask is&still warm with its charge.^"
        "Wear it and Link charges like Goht,&spending magic and fearing no fall.",
        "%rGoht's Remains&%wPress %y\xA1%w to wear it.&Worn: a magic-fed charge, and no fall damage.",
    },
    {
        "nei.mm_remains_gyorg",
        sGyorgIcon,
        sGyorgName,
        sGyorgMask,
        0.48f,
        1670.0f,
        900.0f,
        "You got %rGyorg's Remains%w!&The masked fish still breathes&where Link cannot.^"
        "Wear it and the water stops&counting: Link swims and never drowns.",
        "%rGyorg's Remains&%wPress %y\xA1%w to wear it.&Worn: endless breath and a faster swim.",
    },
    {
        "nei.mm_remains_twinmold",
        sTwinmoldIcon,
        sTwinmoldName,
        sTwinmoldMask,
        0.45f,
        1470.0f,
        900.0f,
        "You got %rTwinmold's Remains%w!&The giants of the desert left&their strength behind.^"
        "Wear it and every sword of Link's&strikes with a giant's arm.",
        "%rTwinmold's Remains&%wPress %y\xA1%w to wear it.&Worn: your sword hits twice as hard.",
    },
};

static const ALIGN_ASSET(2) char sSonataIcon[] = "__OTR__textures/icon_item_custom/gItemIconSonataOfAwakeningTex";
static const ALIGN_ASSET(2) char sLullabyIcon[] = "__OTR__textures/icon_item_custom/gItemIconGoronLullabyTex";
static const ALIGN_ASSET(2) char sBossaNovaIcon[] = "__OTR__textures/icon_item_custom/gItemIconNewWaveBossaNovaTex";
static const ALIGN_ASSET(2) char sElegyIcon[] = "__OTR__textures/icon_item_custom/gItemIconElegyOfEmptinessTex";
static const ALIGN_ASSET(2) char sOathIcon[] = "__OTR__textures/icon_item_custom/gItemIconOathToOrderTex";
static const ALIGN_ASSET(2) char sHealingIcon[] = "__OTR__textures/icon_item_custom/gItemIconSongOfHealingTex";
static const ALIGN_ASSET(2) char sSoaringIcon[] = "__OTR__textures/icon_item_custom/gItemIconSongOfSoaringTex";
static const ALIGN_ASSET(2) char sFugueIcon[] = "__OTR__textures/icon_item_custom/gItemIconFugueOfHomeTex";
static const ALIGN_ASSET(2) char sBalladIcon[] = "__OTR__textures/icon_item_custom/gItemIconBalladOfHeroTex";
static const ALIGN_ASSET(2) char sCommandIcon[] = "__OTR__textures/icon_item_custom/gItemIconCommandMelodyTex";

static const ALIGN_ASSET(2) char sSonataName[] = "__OTR__textures/item_name_custom/gSonataOfAwakeningNameTex";
static const ALIGN_ASSET(2) char sLullabyName[] = "__OTR__textures/item_name_custom/gGoronLullabyNameTex";
static const ALIGN_ASSET(2) char sBossaNovaName[] = "__OTR__textures/item_name_custom/gNewWaveBossaNovaNameTex";
static const ALIGN_ASSET(2) char sElegyName[] = "__OTR__textures/item_name_custom/gElegyOfEmptinessNameTex";
static const ALIGN_ASSET(2) char sOathName[] = "__OTR__textures/item_name_custom/gOathToOrderNameTex";
static const ALIGN_ASSET(2) char sHealingName[] = "__OTR__textures/item_name_custom/gSongOfHealingNameTex";
static const ALIGN_ASSET(2) char sSoaringName[] = "__OTR__textures/item_name_custom/gSongOfSoaringNameTex";
static const ALIGN_ASSET(2) char sFugueName[] = "__OTR__textures/item_name_custom/gFugueOfHomeNameTex";
static const ALIGN_ASSET(2) char sBalladName[] = "__OTR__textures/item_name_custom/gBalladOfHeroNameTex";
static const ALIGN_ASSET(2) char sCommandName[] = "__OTR__textures/item_name_custom/gCommandMelodyNameTex";

// The seven Majora's Mask melodies are the authoritative button sequences translated to Ocarina of
// Time's pitches. The last three have no melody of their own yet: theirs are provisional and picked
// so they collide with nothing vanilla teaches (see NOTES.md).
static const SongDefinition sSongs[] = {
    {
        "nei.mm_song_sonata",
        sSonataIcon,
        sSonataName,
        7,
        { NOTE_C_UP, NOTE_C_LEFT, NOTE_C_UP, NOTE_C_LEFT, NOTE_A, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rSonata of Awakening%w!&The song that woke a sleeping temple&out of the swamp.",
        "%rSonata of Awakening&%w\xA2\xA1\xA2\xA1\x9F\xA1\x9F&The ocarina answers it.",
    },
    {
        "nei.mm_song_goron_lullaby",
        sLullabyIcon,
        sLullabyName,
        8,
        { NOTE_A, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_A, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rGoron Lullaby%w!&A father's song, long enough to&put a mountain to sleep.",
        "%rGoron Lullaby&%w\x9F\xA1\xA1\x9F\xA1\xA1\xA1\x9F&The ocarina answers it.",
    },
    {
        "nei.mm_song_new_wave",
        sBossaNovaIcon,
        sBossaNovaName,
        7,
        { NOTE_C_LEFT, NOTE_C_UP, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_RIGHT },
        "You learned the %rNew Wave Bossa Nova%w!&The Indigo-Go's last song, the one&the eggs of the sea sing back.",
        "%rNew Wave Bossa Nova&%w\xA1\xA2\xA1\xA1\xA1\xA1\xA1&The ocarina answers it.",
    },
    {
        "nei.mm_song_elegy",
        sElegyIcon,
        sElegyName,
        7,
        { NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP, NOTE_C_LEFT },
        "You learned the %rElegy of Emptiness%w!&It leaves a hollow copy of you&standing exactly where you stood.",
        "%rElegy of Emptiness&%w\xA1\xA1\xA1\xA1\xA1\xA2\xA1&The ocarina answers it.",
    },
    {
        "nei.mm_song_oath",
        sOathIcon,
        sOathName,
        6,
        { NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_A, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP },
        "You learned the %rOath to Order%w!&The call that brings the four&giants down from the edges of the world.",
        "%rOath to Order&%w\xA1\xA1\x9F\xA1\xA1\xA2&The ocarina answers it.",
    },
    {
        "nei.mm_song_healing",
        sHealingIcon,
        sHealingName,
        6,
        { NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN },
        "You learned the %rSong of Healing%w!&Sorrow goes into the melody,&and a mask comes out.",
        "%rSong of Healing&%w\xA1\xA1\xA1\xA1\xA1\xA1&The ocarina answers it.",
    },
    {
        "nei.mm_song_soaring",
        sSoaringIcon,
        sSoaringName,
        6,
        { NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_UP, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_UP },
        "You learned the %rSong of Soaring%w!&Every owl that ever watched you&is listening for it.",
        "%rSong of Soaring&%w\xA1\xA1\xA2\xA1\xA1\xA2&The ocarina answers it.",
    },
    {
        "nei.mm_song_fugue_of_home",
        sFugueIcon,
        sFugueName,
        6,
        { NOTE_C_LEFT, NOTE_C_LEFT, NOTE_C_DOWN, NOTE_C_UP, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rFugue of Home%w!&A song that remembers the way back&better than you do.",
        "%rFugue of Home&%w\xA1\xA1\xA1\xA2\xA1\x9F&The ocarina answers it.",
    },
    {
        "nei.mm_song_ballad_of_hero",
        sBalladIcon,
        sBalladName,
        6,
        { NOTE_C_UP, NOTE_C_UP, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_A },
        "You learned the %rBallad of Hero%w!&Someone wrote down what you did&and set it to six notes.",
        "%rBallad of Hero&%w\xA2\xA2\xA1\xA1\xA1\x9F&The ocarina answers it.",
    },
    {
        "nei.mm_song_command_melody",
        sCommandIcon,
        sCommandName,
        6,
        { NOTE_A, NOTE_A, NOTE_C_UP, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_DOWN },
        "You learned the %rCommand Melody%w!&Play it and something else&takes its orders from you.",
        "%rCommand Melody&%w\x9F\x9F\xA2\xA1\xA1\xA1&The ocarina answers it.",
    },
};

#define SONG_COUNT ((s32)ARRAY_COUNT(sSongs))

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate", "OnPlayerPostLimbDraw",  "OnPlayerResolveMotionScale",
    "OnOcarinaNote",  "OnPlayerActionHandler", "OnResolveSwordDamage",
    "OnLoadGame",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static s32 sWornRemains = -1;
static s16 sStepSfxTimer;
static s16 sChargeMagicTimer;

static u8 sPlayedNotes[SONG_MAX_NOTES];
static s32 sPlayedCount;

static s16 DegreesToBinang(s32 degrees) {
    return (s16)((degrees * 0x10000) / 360);
}

static bool IsWorn(RemainsId remains) {
    return sWornRemains == (s32)remains;
}

static bool IsSprintHeld(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A);
}

// Odolwa's run and Goht's charge both live on A, so neither can afford to lose the button to a roll.
static bool OwnsTheAButton(void) {
    return IsWorn(REMAINS_ODOLWA) || IsWorn(REMAINS_GOHT);
}

static bool IsCharging(PlayState* play, Player* player) {
    return IsWorn(REMAINS_GOHT) && IsSprintHeld(play) && (player->actor.bgCheckFlags & BG_ON_GROUND);
}

static void DoffRemains(Player* player) {
    if (sWornRemains < 0) {
        return;
    }
    sWornRemains = -1;
    sChargeMagicTimer = 0;
    Player_PlaySfx(&player->actor, NA_SE_PL_TAKE_OUT_SHIELD);
}

static void DonRemains(Player* player, s32 remains) {
    sWornRemains = remains;
    sChargeMagicTimer = 0;
    player->currentMask = PLAYER_MASK_NONE;
    Player_PlaySfx(&player->actor, NA_SE_PL_CHANGE_ARMS);
}

static s32 RemainsIndexForKey(const char* key) {
    for (s32 i = 0; i < REMAINS_COUNT; i++) {
        if (strcmp(sRemains[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static void ToggleRemains(PlayState* play, Player* player, s32 remains) {
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        return;
    }
    if (IsWorn(remains)) {
        DoffRemains(player);
        return;
    }
    DonRemains(player, remains);
}

static void ToggleOdolwa(PlayState* play, Player* player) {
    ToggleRemains(play, player, REMAINS_ODOLWA);
}

static void ToggleGoht(PlayState* play, Player* player) {
    ToggleRemains(play, player, REMAINS_GOHT);
}

static void ToggleGyorg(PlayState* play, Player* player) {
    ToggleRemains(play, player, REMAINS_GYORG);
}

static void ToggleTwinmold(PlayState* play, Player* player) {
    ToggleRemains(play, player, REMAINS_TWINMOLD);
}

static const SOHCustomItemInitFunc sRemainsToggle[REMAINS_COUNT] = {
    ToggleOdolwa,
    ToggleGoht,
    ToggleGyorg,
    ToggleTwinmold,
};

static void ForgetWornRemains(const char* key) {
    if (RemainsIndexForKey(key) == sWornRemains) {
        sWornRemains = -1;
    }
}

static void SpendChargeMagic(void) {
    if (!gSaveContext.isMagicAcquired || (gSaveContext.magic <= 0)) {
        return;
    }
    if (++sChargeMagicTimer < CHARGE_MAGIC_INTERVAL) {
        return;
    }
    sChargeMagicTimer = 0;
    gSaveContext.magic--;
}

static void TickOdolwa(PlayState* play, Player* player) {
    if (!IsSprintHeld(play) || (player->actor.speedXZ <= 1.0f)) {
        return;
    }
    if (--sStepSfxTimer > 0) {
        return;
    }
    sStepSfxTimer = STEP_SFX_INTERVAL;
    Player_PlaySfx(&player->actor, NA_SE_PL_WALK_GROUND);
}

static void TickWornRemains(void) {
    if (gPlayState == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);

    // A native mask and a remains share Link's face: whichever was donned last keeps it.
    if ((sWornRemains >= 0) && (player->currentMask != PLAYER_MASK_NONE)) {
        sWornRemains = -1;
    }
    if (sWornRemains < 0) {
        return;
    }
    if (IsWorn(REMAINS_ODOLWA)) {
        TickOdolwa(gPlayState, player);
        return;
    }
    if (IsWorn(REMAINS_GOHT)) {
        if (IsCharging(gPlayState, player)) {
            SpendChargeMagic();
        }
        return;
    }
    if (IsWorn(REMAINS_GYORG) && (player->actor.yDistToWater > 0.0f)) {
        player->underwaterTimer = 0;
    }
}

static void DrawWornMask(PlayState* play, Player* player, int32_t limbIndex) {
    if ((limbIndex != PLAYER_LIMB_HEAD) || (sWornRemains < 0)) {
        return;
    }
    const RemainsDefinition* definition = &sRemains[sWornRemains];
    if (!SOH_MOD_API_HAS(sApi, HasResource) || !sApi->HasResource(definition->maskPath)) {
        return;
    }
    f32 scale = definition->maskScale * MASK_SCALE_CORRECTION;

    OPEN_DISPS(play->state.gfxCtx);

    // The current matrix is the head node, and the hat limb draws after us: leave it untouched.
    Matrix_Push();
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_RotateZYX(DegreesToBinang(MASK_ROT_X_DEGREES), DegreesToBinang(MASK_ROT_Y_DEGREES),
                     DegreesToBinang(MASK_ROT_Z_DEGREES), MTXMODE_APPLY);
    Matrix_Translate(0.0f, definition->maskTranslateY + MASK_OFFSET_Y, definition->maskTranslateZ + MASK_OFFSET_Z,
                     MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)definition->maskPath);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void ResolveMotionScale(Player* player, int32_t kind, f32* scale) {
    if ((sWornRemains < 0) || (gPlayState == NULL)) {
        return;
    }
    f32 multiplier = 1.0f;

    if (IsWorn(REMAINS_GYORG) && (player->actor.yDistToWater > 0.0f)) {
        multiplier = SWIM_SPEED_MULTIPLIER;
    } else if (IsCharging(gPlayState, player)) {
        multiplier = CHARGE_SPEED_MULTIPLIER;
    } else if (IsWorn(REMAINS_ODOLWA) && IsSprintHeld(gPlayState)) {
        multiplier = SPRINT_SPEED_MULTIPLIER;
    }
    if (multiplier == 1.0f) {
        return;
    }
    // A cap is a distance the engine measured against the wall ahead, so it scales like the speed does.
    if ((kind == SOH_PLAYER_MOTION_STICK_SPEED) || (kind == SOH_PLAYER_MOTION_SPEED_CAP)) {
        *scale *= multiplier;
    }
}

static void SuppressRoll(PlayState* play, Player* player, int32_t action, bool* consumed, bool* startedAction) {
    if (!OwnsTheAButton()) {
        return;
    }
    *consumed = true;
    *startedAction = false;
}

static void StrengthenSword(PlayState* play, int32_t dmgFlags, u8* damage) {
    if (!IsWorn(REMAINS_TWINMOLD)) {
        return;
    }
    *damage = (u8)(*damage * GIANT_DAMAGE_MULTIPLIER);
}

static void SurviveTheFall(bool* should, va_list args) {
    if (gPlayState == NULL || !IsCharging(gPlayState, GET_PLAYER(gPlayState))) {
        return;
    }
    *should = false;
}

static bool IsSongOwned(const SongDefinition* song) {
    return SOH_MOD_API_HAS(sApi, IsCustomItemOwned) && sApi->IsCustomItemOwned(song->key);
}

static bool MatchesTail(const SongDefinition* song) {
    if (sPlayedCount < song->noteCount) {
        return false;
    }
    for (s32 i = 0; i < song->noteCount; i++) {
        if (sPlayedNotes[SONG_MAX_NOTES - song->noteCount + i] != song->notes[i]) {
            return false;
        }
    }
    return true;
}

static void ForgetPlayedNotes(void) {
    memset(sPlayedNotes, OCARINA_NOTE_INVALID, sizeof(sPlayedNotes));
    sPlayedCount = 0;
}

static void RecognizeSong(u8 note, f32 modulator, s8 bend) {
    // The hook also fires on release, where the note is invalid: dropping those is what keeps a
    // melody's two identical notes in a row from collapsing into one.
    if ((gPlayState == NULL) || (note == OCARINA_NOTE_INVALID)) {
        return;
    }
    memmove(sPlayedNotes, sPlayedNotes + 1, SONG_MAX_NOTES - 1);
    sPlayedNotes[SONG_MAX_NOTES - 1] = note;
    if (sPlayedCount < SONG_MAX_NOTES) {
        sPlayedCount++;
    }

    for (s32 i = 0; i < SONG_COUNT; i++) {
        const SongDefinition* song = &sSongs[i];
        if (!IsSongOwned(song) || !MatchesTail(song)) {
            continue;
        }
        Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        ForgetPlayedNotes();
        return;
    }
}

static void StartFile(int32_t fileNum) {
    sWornRemains = -1;
    sChargeMagicTimer = 0;
    ForgetPlayedNotes();
}

static void RegisterRemains(void) {
    for (s32 i = 0; i < REMAINS_COUNT; i++) {
        const RemainsDefinition* source = &sRemains[i];
        SOHCustomItemDefinition definition = Z64Items_Define(source->key, source->iconPath, source->namePath);

        Z64Items_SetButtons(&definition, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
        definition.flags |= SOH_CUSTOM_ITEM_INSTANT;
        Z64Items_SetPlacement(&definition, MM_COLLECTABLES_PAGE, (u8)(REMAINS_FIRST_SLOT + i), 0);
        Z64Items_SetTextbox(&definition, source->getItemText);
        Z64Items_SetPauseText(&definition, source->pauseText);
        Z64Items_SetAction(&definition, sRemainsToggle[i], NULL);
        definition.onRemove = ForgetWornRemains;

        Z64Items_Register(sApi, &definition);
    }
}

static void RegisterSongs(void) {
    static const u8 cells[] = { 6, 7, 8, 9, 10, 13, 15, 14, 16, 12 };
    for (s32 i = 0; i < SONG_COUNT; i++) {
        const SongDefinition* source = &sSongs[i];
        SOHCustomItemDefinition definition = Z64Items_Define(source->key, source->iconPath, source->namePath);

        Z64Items_SetPlacement(&definition, MM_COLLECTABLES_PAGE, cells[i], 0);
        Z64Items_SetTextbox(&definition, source->getItemText);
        Z64Items_SetPauseText(&definition, source->pauseText);

        Z64Items_Register(sApi, &definition);
    }
}

static void RegisterCollectablesLayout(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterLayoutVanilla)) {
        return;
    }
    static const SOHLayoutCell cells[] = {
        { 45, 62, 32, 32 }, { 78, 42, 32, 32 }, { 10, 42, 32, 32 }, { 45, 20, 32, 32 },
        { 80, -9, 32, 32 }, { 11, -9, 32, 32 },
        { -109, -24, 16, 24 }, { -90, -24, 16, 24 }, { -71, -24, 16, 24 },
        { -52, -24, 16, 24 }, { -33, -24, 16, 24 }, { -14, -24, 16, 24 },
        { -109, 2, 16, 24 }, { -90, 2, 16, 24 }, { -71, 2, 16, 24 },
        { -52, 2, 16, 24 }, { -33, 2, 16, 24 }, { -14, 2, 16, 24 },
        { -103, 54, 32, 32 }, { 7, -44, 32, 32 }, { 82, -44, 32, 32 },
        { -110, 34, 24, 24 }, { -54, 58, 48, 48 },
    };
    const SOHLayoutPage page = { sizeof(SOHLayoutPage), MM_LAYOUT_KEY, "Majora's Mask", SOH_LAYOUT_COLLECTABLES,
                                 4, 6, ARRAY_COUNT(cells), cells };
    if (!sApi->RegisterLayoutPage(&page)) {
        return;
    }
    for (s32 i = 0; i < REMAINS_COUNT; ++i) {
        sApi->PlaceLayoutItem(sRemains[i].key, MM_LAYOUT_KEY, REMAINS_FIRST_SLOT + i);
    }
    static const u8 songCells[] = { 6, 7, 8, 9, 10, 13, 15, 14, 16, 12 };
    for (s32 i = 0; i < SONG_COUNT; ++i) {
        sApi->PlaceLayoutItem(sSongs[i].key, MM_LAYOUT_KEY, songCells[i]);
    }
    static const SOHLayoutVanilla shared[] = {
        { "nei.mm_collectables.saria", MM_LAYOUT_KEY, SOH_LAYOUT_COLLECTABLES, QUEST_SONG_SARIA, 11 },
        { "nei.mm_collectables.sun", MM_LAYOUT_KEY, SOH_LAYOUT_COLLECTABLES, QUEST_SONG_SUN, 17 },
        { "nei.mm_collectables.hearts", MM_LAYOUT_KEY, SOH_LAYOUT_COLLECTABLES, 24, 22 },
        { "nei.mm_collectables.strength", MM_LAYOUT_KEY, SOH_LAYOUT_EQUIPMENT, 8, 5 },
        { "nei.mm_collectables.scale", MM_LAYOUT_KEY, SOH_LAYOUT_EQUIPMENT, 12, 4 },
        { "nei.mm_collectables.quiver", MM_LAYOUT_KEY, SOH_LAYOUT_EQUIPMENT, 0, 19 },
        { "nei.mm_collectables.bombs", MM_LAYOUT_KEY, SOH_LAYOUT_EQUIPMENT, 4, 20 },
    };
    for (u32 i = 0; i < ARRAY_COUNT(shared); ++i) {
        sApi->RegisterLayoutVanilla(&shared[i]);
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    ForgetPlayedNotes();
    RegisterRemains();
    RegisterSongs();
    RegisterCollectablesLayout();
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickWornRemains);
    SOH_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawWornMask);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, ResolveMotionScale);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ROLL, SuppressRoll);
    SOH_REGISTER_HOOK(sApi, OnResolveSwordDamage, StrengthenSword);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, RecognizeSong);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, StartFile);
    sApi->RegisterVB(VB_RECIEVE_FALL_DAMAGE, SurviveTheFall);
}
