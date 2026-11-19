#include <string.h>
#include <stdio.h>

#include "z64items.h"
#include "remains.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "mm_song_page.h"

#define MM_COLLECTABLES_PAGE (SOH_ITEM_PAGE_QUEST_VANILLA + 1)
#define MM_LAYOUT_KEY "nei.mm_collectables"
#define REMAINS_FIRST_SLOT 0

#define NOTE_A 0x02
#define NOTE_C_DOWN 0x05
#define NOTE_C_RIGHT 0x09
#define NOTE_C_LEFT 0x0B
#define NOTE_C_UP 0x0E

#define SONG_MAX_NOTES 8
#define GUEST_SONG_MAX 4

typedef struct {
    const char* key;
    const char* storeId;
    const char* iconPath;
    const char* namePath;
    const char* getItemModelPath;
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
    u8 cell;
    const OcarinaNote* playback;
} SongDefinition;

static const RemainsDefinition sRemains[REMAINS_COUNT] = {
    {
        "mm.remains.odolwa",
        "mm_remains_odolwa",
        "__OTR__icon_item_static_yar/gItemIconOdolwasRemainsTex",
        "__OTR__item_name_static/gItemNameOdolwasRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsOdolwaDL",
        "You got %rOdolwa's Remains%w! The&jungle warrior's mask still&keeps the beat of his dance.^"
        "Hold %y\x9F%w to run as he danced;&%y\xA3%w+%y\xA0%w calls his beetles.",
        "%rOdolwa's Remains&%wPress %y\xA1%w to wear it. Worn: %y\x9F%w runs,&%y\xA3%w+%y\xA0%w calls beetles.",
    },
    {
        "mm.remains.goht",
        "mm_remains_goht",
        "__OTR__icon_item_static_yar/gItemIconGohtsRemainsTex",
        "__OTR__item_name_static/gItemNameGohtsRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsGohtDL",
        "You got %rGoht's Remains%w! The&mechanical bull's mask is still&warm with its charge.^"
        "Hold %y\x9F%w to charge, %y\xA3%w+%y\x9F%w to&pound, hold %y\xA0%w for thunder.",
        "%rGoht's Remains&%wPress %y\xA1%w to wear it. Worn: %y\x9F%w&charges, %y\xA0%w calls thunder.",
    },
    {
        "mm.remains.gyorg",
        "mm_remains_gyorg",
        "__OTR__icon_item_static_yar/gItemIconGyorgsRemainsTex",
        "__OTR__item_name_static/gItemNameGyorgsRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsGyorgDL",
        "You got %rGyorg's Remains%w! The&masked fish still breathes&where Link cannot.^"
        "Underwater, %y\xA3%w calls its school&and holding %y\xA0%w stirs a whirlpool.",
        "%rGyorg's Remains&%wPress %y\xA1%w to wear it. Worn: endless&breath, fish and a whirlpool.",
    },
    {
        "mm.remains.twinmold",
        "mm_remains_twinmold",
        "__OTR__icon_item_static_yar/gItemIconTwinmoldsRemainsTex",
        "__OTR__item_name_static/gItemNameTwinmoldsRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsTwinmoldDL",
        "You got %rTwinmold's Remains%w!&Equip to %y\xA1%w to summon a shadow&wearing your mask. Costs 24 MP.^"
        "Wear it to open Orders: attack,&help, follow or wait. Your first&choice is included in summoning.^"
        "Press %y\xA1%w again for orders.&Changing mode costs 24 MP.&Choosing the same mode is free.^"
        "Help follows and lends strength.&Slash a cracked wall for a bomb.&Target a cucco to have it fetched.^"
        "Face him nearby and press %b\x9F%w.&He lifts you so you can aim.&Stand together for extra strength.^"
        "Aim a lift with the Stick.&%b\x9F%w launches; %g\xA0%w cancels.&Twin Link stays until you die.",
        "%rTwinmold's Remains&%wEquip to %y\xA1%w to wear it and&open Orders. Summon: 24 MP.&Later mode changes: 24 "
        "MP.",
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

static const ALIGN_ASSET(2) char sSonataName[] = "__OTR__item_name_static/gItemNameSonataOfAwakeningENGTex";
static const ALIGN_ASSET(2) char sLullabyName[] = "__OTR__item_name_static/gItemNameGoronLullabyENGTex";
static const ALIGN_ASSET(2) char sBossaNovaName[] = "__OTR__item_name_static/gItemNameNewWaveBossaNovaENGTex";
static const ALIGN_ASSET(2) char sElegyName[] = "__OTR__item_name_static/gItemNameElegyOfEmptynessENGTex";
static const ALIGN_ASSET(2) char sOathName[] = "__OTR__item_name_static/gItemNameOathToOrderENGTex";
static const ALIGN_ASSET(2) char sHealingName[] = "__OTR__item_name_static/gItemNameSongOfHealingENGTex";
static const ALIGN_ASSET(2) char sSoaringName[] = "__OTR__item_name_static/gItemNameSongOfSoaringENGTex";
static const ALIGN_ASSET(2) char sFugueName[] = "__OTR__textures/item_name_custom/gFugueOfHomeNameTex";
static const ALIGN_ASSET(2) char sBalladName[] = "__OTR__textures/item_name_custom/gBalladOfHeroNameTex";

#include "nei_song_playback.inc.c"
static const SongDefinition sSongs[] = {
    {
        "mm.song.sonata",
        sSonataIcon,
        sSonataName,
        7,
        { NOTE_C_UP, NOTE_C_LEFT, NOTE_C_UP, NOTE_C_LEFT, NOTE_A, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rSonata of Awakening%w! The song that woke a sleeping temple out of the swamp.",
        "%rSonata of Awakening&%w\xA2\xA1\xA2\xA1\x9F\xA1\x9F The ocarina answers&it.",
        6,
        NULL,
    },
    {
        "mm.song.goron_lullaby",
        sLullabyIcon,
        sLullabyName,
        8,
        { NOTE_A, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_A, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rGoron Lullaby%w! A father's song, long enough to put a mountain to sleep.",
        "%rGoron Lullaby&%w\x9F\xA1\xA1\x9F\xA1\xA1\xA1\x9F The ocarina answers&it.",
        7,
        NULL,
    },
    {
        "mm.song.new_wave",
        sBossaNovaIcon,
        sBossaNovaName,
        7,
        { NOTE_C_LEFT, NOTE_C_UP, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_RIGHT },
        "You learned the %rNew Wave Bossa Nova%w! The Indigo-Go's last song, the one the eggs of the sea sing "
        "back.",
        "%rNew Wave Bossa Nova&%w\xA1\xA2\xA1\xA1\xA1\xA1\xA1 The ocarina answers&it.",
        8,
        NULL,
    },
    {
        "mm.song.elegy",
        sElegyIcon,
        sElegyName,
        7,
        { NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP, NOTE_C_LEFT },
        "You learned the %rElegy of Emptiness%w! It leaves a hollow copy of you standing exactly where you stood.",
        "%rElegy of Emptiness&%w\xA1\xA1\xA1\xA1\xA1\xA2\xA1 The ocarina answers&it.",
        9,
        NULL,
    },
    {
        "mm.song.oath",
        sOathIcon,
        sOathName,
        6,
        { NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_A, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP },
        "You learned the %rOath to Order%w! The call that brings the four giants down from the edges of "
        "the world.",
        "%rOath to Order&%w\xA1\xA1\x9F\xA1\xA1\xA2 The ocarina answers it.",
        10,
        NULL,
    },
    {
        "mm.song.healing",
        sHealingIcon,
        sHealingName,
        6,
        { NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN },
        "You learned the %rSong of Healing%w! Sorrow goes into the melody, and a mask comes out.",
        "%rSong of Healing&%w\xA1\xA1\xA1\xA1\xA1\xA1 The ocarina answers it.",
        13,
        NULL,
    },
    {
        "mm.song.soaring",
        sSoaringIcon,
        sSoaringName,
        6,
        { NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_UP, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_UP },
        "You learned the %rSong of Soaring%w! Every owl that ever watched you is listening for it.",
        "%rSong of Soaring&%w\xA1\xA1\xA2\xA1\xA1\xA2 The ocarina answers it.",
        15,
        NULL,
    },
    {
        "nei.mm_song_fugue_of_home",
        sFugueIcon,
        sFugueName,
        6,
        { NOTE_A, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP, NOTE_C_RIGHT, NOTE_C_LEFT },
        "You learned the %rFugue of Home%w! A song that remembers the way back better than you do.",
        "%rFugue of Home&%w\x9F\xA6\xA8\xA5\xA8\xA7 The ocarina answers it.",
        14,
        sFuguePlayback,
    },
    {
        "nei.mm_song_ballad_of_hero",
        sBalladIcon,
        sBalladName,
        7,
        { NOTE_A, NOTE_C_DOWN, NOTE_C_UP, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT },
        "You learned the %rBallad of Hero%w! Someone wrote down what you did and set it to seven notes.",
        "%rBallad of Hero&%w\x9F\xA6\xA5\xA7\xA8\xA7\xA8 The ocarina answers it.",
        16,
        sBalladPlayback,
    },
};

#define SONG_COUNT ((s32)ARRAY_COUNT(sSongs))

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",
    "OnPlayerPostLimbDraw",
    "OnPlayerResolveLimbDraw",
    "OnPlayerResolveAnim",
    "OnPlayerResolveMotionScale",
    "OnPlayerActionHandler",
    "OnPlayerShieldBlocked",
    "OnActorDrawEnd",
    "OnActorUpdate",
    "OnPlayerFilterInput",
    "OnInterfaceDrawEnd",
    "OnGameFrameUpdate",
    "OnSceneInit",
    "OnOcarinaNote",
    "OnLoadGame",
    "OnKaleidoInput",
    "OnExitGame",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static SongDefinition sGuestSongs[GUEST_SONG_MAX];
static s32 sGuestSongCount;
static const SongDefinition* sPageSongs[SONG_COUNT + GUEST_SONG_MAX];
static s32 sPageSongCount;

static u8 sPlayedNotes[SONG_MAX_NOTES];
static s32 sPlayedCount;
static u8 sLastPlayedPitch = OCARINA_PITCH_NONE;

static s32 RemainsIndexForKey(const char* key) {
    for (s32 i = 0; i < REMAINS_COUNT; i++) {
        if (strcmp(sRemains[i].key, key) == 0) {
            return i;
        }
    }
    return REMAINS_NONE;
}

#include "twin_link.inc.c"

static void ToggleOdolwa(PlayState* play, Player* player) {
    Remains_Toggle(play, player, REMAINS_ODOLWA);
}

static void ToggleGoht(PlayState* play, Player* player) {
    Remains_Toggle(play, player, REMAINS_GOHT);
}

static void ToggleGyorg(PlayState* play, Player* player) {
    Remains_Toggle(play, player, REMAINS_GYORG);
}

static void ToggleTwinmold(PlayState* play, Player* player) {
    TwinUse(play, player);
}

static const SOHCustomItemInitFunc sRemainsToggle[REMAINS_COUNT] = {
    ToggleOdolwa,
    ToggleGoht,
    ToggleGyorg,
    ToggleTwinmold,
};

static void ForgetWornRemains(const char* key) {
    Remains_Forget(RemainsIndexForKey(key));
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
    memset(sPlayedNotes, OCARINA_PITCH_NONE, sizeof(sPlayedNotes));
    sPlayedCount = 0;
    sLastPlayedPitch = OCARINA_PITCH_NONE;
}

#include "collect_behaviors.inc.c"
#include "collect_background.inc.c"
#include "collect_pause.inc.c"

static void RecognizeSong(u8 note, f32 modulator, s8 bend) {
    // The hook also fires on release with an invalid note; dropping those keeps repeated notes distinct.
    if (gPlayState == NULL)
        return;
    if (note == OCARINA_PITCH_NONE) {
        sLastPlayedPitch = note;
        return;
    }
    if (note == sLastPlayedPitch)
        return;
    sLastPlayedPitch = note;
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
        Audio_PlaySfxGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        QueueRecognizedSong(i);
        ForgetPlayedNotes();
        return;
    }
}

static void StartFile(int32_t fileNum) {
    TwinLoad();
    Remains_ResetFile();
    ForgetPlayedNotes();
}

static const ALIGN_ASSET(2) char sSongNoteDL[] = "__OTR__objects/object_gi_melody/gGiSongNoteDL";
static const ALIGN_ASSET(2) char sSongColorDL[6][80] = {
    "__OTR__objects/object_gi_melody/gGiMinuetColorDL",   "__OTR__objects/object_gi_melody/gGiBoleroColorDL",
    "__OTR__objects/object_gi_melody/gGiSerenadeColorDL", "__OTR__objects/object_gi_melody/gGiRequiemColorDL",
    "__OTR__objects/object_gi_melody/gGiNocturneColorDL", "__OTR__objects/object_gi_melody/gGiPreludeColorDL"
};
static s32 SongIndexForKey(const char* key) {
    for (s32 i = 0; i < SONG_COUNT; i++) {
        if (strcmp(sSongs[i].key, key) == 0) {
            return i;
        }
    }
    return 0;
}

static void DrawSongGetItem(PlayState* play, GetItemEntry* entry) {
    const SOHCustomItemDefinition* item = sApi->FindCustomItemByRuntimeId(entry->tableId);
    s32 song = item != NULL ? SongIndexForKey(item->key) : 0;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSongColorDL[song % 6]);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSongNoteDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawRemainsGetItem(PlayState* play, GetItemEntry* entry) {
    const SOHCustomItemDefinition* item = sApi->FindCustomItemByRuntimeId(entry->tableId);
    s32 remains = item != NULL ? RemainsIndexForKey(item->key) : REMAINS_NONE;
    if (remains == REMAINS_NONE) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(.016f, .016f, .016f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRemains[remains].getItemModelPath);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void RegisterRemains(void) {
    for (s32 i = 0; i < REMAINS_COUNT; i++) {
        const RemainsDefinition* source = &sRemains[i];
        char option[128];
        snprintf(option, sizeof(option), "gMods.Store.mm_collectables.%s", source->storeId);
        if (!CVarGetInteger(option, 1)) {
            continue;
        }
        SOHCustomItemDefinition definition = Z64Items_Define(source->key, source->iconPath, source->namePath);

        Z64Items_SetButtons(&definition, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
        definition.flags |= SOH_CUSTOM_ITEM_INSTANT;
        if (i != REMAINS_TWINMOLD)
            definition.flags |= SOH_CUSTOM_ITEM_WEARABLE;
        Z64Items_SetPlacement(&definition, MM_COLLECTABLES_PAGE, (u8)(REMAINS_FIRST_SLOT + i), 0);
        Z64Items_SetTextbox(&definition, source->getItemText);
        Z64Items_SetPauseText(&definition, source->pauseText);
        definition.getItemEntry.drawFunc = DrawRemainsGetItem;
        Z64Items_SetAction(&definition, sRemainsToggle[i], NULL);
        definition.onRemove = i == REMAINS_TWINMOLD ? TwinForget : ForgetWornRemains;
        const SOHCustomItemRandomizer randomizer = {
            sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, source->key, source->key, "",
        };
        Z64Items_SetLogic(&definition, &randomizer);

        Z64Items_Register(sApi, &definition);
    }
}

static bool AddPageSong(const NeiMmSong* song) {
    if (song == NULL || song->size < sizeof(NeiMmSong) || song->key == NULL || song->noteCount == 0 ||
        song->noteCount > SONG_MAX_NOTES) {
        return false;
    }
    for (s32 i = 0; i < sPageSongCount; ++i) {
        if (strcmp(sPageSongs[i]->key, song->key) == 0) {
            return true;
        }
    }
    if (sGuestSongCount == GUEST_SONG_MAX) {
        return false;
    }
    SongDefinition* guest = &sGuestSongs[sGuestSongCount++];
    memset(guest, 0, sizeof(*guest));
    guest->key = song->key;
    guest->noteCount = song->noteCount;
    memcpy(guest->notes, song->notes, song->noteCount);
    guest->playback = song->playback;
    sPageSongs[sPageSongCount++] = guest;
    return true;
}

static void PublishPageSongs(void) {
    for (s32 i = 0; i < SONG_COUNT; ++i) {
        sPageSongs[sPageSongCount++] = &sSongs[i];
    }
    if (SOH_MOD_API_HAS(sApi, PublishService)) {
        sApi->PublishService("nei.mm_collectables.add_song", (void*)AddPageSong);
    }
}

static const char* ResolveSongIcon(const SongDefinition* song, s32 index) {
    static ALIGN_ASSET(2) char sDerivedIcons[ARRAY_COUNT(sSongs)][96];
    const char* name = strrchr(song->iconPath, '/');
    if (name == NULL || !SOH_MOD_API_HAS(sApi, HasResource)) {
        return song->iconPath;
    }
    snprintf(sDerivedIcons[index], sizeof(sDerivedIcons[index]), "__OTR__textures/mm_song_notes%s", name);
    return sApi->HasResource(sDerivedIcons[index]) ? sDerivedIcons[index] : song->iconPath;
}

static void RegisterSongs(void) {
    for (s32 i = 0; i < SONG_COUNT; i++) {
        const SongDefinition* source = &sSongs[i];
        SOHCustomItemDefinition definition = Z64Items_Define(source->key, ResolveSongIcon(source, i), source->namePath);

        Z64Items_SetPlacement(&definition, MM_COLLECTABLES_PAGE, source->cell, 0);
        definition.getItemEntry.drawFunc = DrawSongGetItem;
        Z64Items_SetTextbox(&definition, source->getItemText);
        Z64Items_SetPauseText(&definition, source->pauseText);
        const SOHCustomItemRandomizer randomizer = {
            sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, source->key, source->key, "",
        };
        Z64Items_SetLogic(&definition, &randomizer);

        Z64Items_Register(sApi, &definition);
    }
}

static void RegisterCollectablesLayout(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterLayoutVanilla)) {
        return;
    }
    static const SOHLayoutCell cells[] = {
        { 45, 62, 32, 32 },   { 78, 42, 32, 32 },    { 10, 42, 32, 32 },   { 45, 20, 32, 32 },   { 80, -9, 32, 32 },
        { 11, -9, 32, 32 },   { -109, -24, 16, 24 }, { -90, -24, 16, 24 }, { -71, -24, 16, 24 }, { -52, -24, 16, 24 },
        { -33, -24, 16, 24 }, { -14, -24, 16, 24 },  { -109, 2, 16, 24 },  { -90, 2, 16, 24 },   { -71, 2, 16, 24 },
        { -52, 2, 16, 24 },   { -33, 2, 16, 24 },    { -14, 2, 16, 24 },   { -103, 54, 32, 32 }, { 7, -44, 32, 32 },
        { 82, -44, 32, 32 },  { -110, 34, 24, 24 },  { -54, 58, 48, 48 },
    };
    const SOHLayoutPage page = { sizeof(SOHLayoutPage), MM_LAYOUT_KEY, "Majora's Mask", SOH_LAYOUT_COLLECTABLES, 4, 6,
                                 ARRAY_COUNT(cells),    cells };
    if (!sApi->RegisterLayoutPage(&page)) {
        return;
    }
    for (s32 i = 0; i < REMAINS_COUNT; ++i) {
        sApi->PlaceLayoutItem(sRemains[i].key, MM_LAYOUT_KEY, REMAINS_FIRST_SLOT + i);
    }
    for (s32 i = 0; i < SONG_COUNT; ++i) {
        sApi->PlaceLayoutItem(sSongs[i].key, MM_LAYOUT_KEY, sSongs[i].cell);
    }
    static const SOHLayoutVanilla shared[] = {
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

static void RegisterNotebook(void) {
    static const SOHCustomItemRandomizer randomizer = {
        .structSize = sizeof(SOHCustomItemRandomizer),
        .type = SOH_CUSTOM_ITEM_TYPE_ITEM,
        .poolCount = 0,
        .logicKey = "nei.combo.bombers_notebook",
        .hintKey = "nei.combo.bombers_notebook",
    };
    SOHCustomItemDefinition item =
        Z64Items_Define("nei.combo.bombers_notebook", "__OTR__icon_item_static_yar/gItemIconBombersNotebookTex", NULL);
    item.displayName = "Bomber's Notebook";
    item.pauseText = "%rBomber's Notebook&%wA record of promises in Termina. No controls in this world.";
    item.getItemText = "You got %rBomber's Notebook%w!&It records promises in Termina and unlocks no ability here.";
    item.preferredPage = SOH_ITEM_PAGE_QUEST_VANILLA + 6;
    item.preferredSlot = 0;
    item.randomizer = &randomizer;
    Z64Items_SetGetItemModel(&item, "__OTR__objects/object_gi_schedule/gGiBombersNotebookDL");
    Z64Items_Register(sApi, &item);
}

SOH_MOD_EXPORT void ModInit(void) {
    RegisterNotebook();
    ForgetPlayedNotes();
    RegisterRemains();
    RegisterSongs();
    PublishPageSongs();
    RegisterCollectablesLayout();
    RemainsAllies_Register(sApi);
    Remains_Init(sApi);
    RegisterCollectBehaviors();
    TwinRegister();
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawMmQuestBackground);
    RegisterCollectPause();
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, RecognizeSong);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, StartFile);
}
