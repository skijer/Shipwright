// Majora's Mask Collectables: the four boss remains as wearable pieces, and the ten songs the
// ocarina of Ocarina of Time can be taught to answer.
#include <string.h>

#include "z64items.h"
#include "remains.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"

#define MM_COLLECTABLES_PAGE (SOH_ITEM_PAGE_QUEST_VANILLA + 1)
#define MM_LAYOUT_KEY "nei.mm_collectables"
#define REMAINS_FIRST_SLOT 0

// The ocarina hook reports the pitch the game gives the sequence player, not the note index.
#define NOTE_A 0x02
#define NOTE_C_DOWN 0x05
#define NOTE_C_RIGHT 0x09
#define NOTE_C_LEFT 0x0B
#define NOTE_C_UP 0x0E

#define SONG_MAX_NOTES 8

typedef struct {
    const char* key;
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
} SongDefinition;

static const RemainsDefinition sRemains[REMAINS_COUNT] = {
    {
        "nei.mm_remains_odolwa",
        "__OTR__icon_item_static_yar/gItemIconOdolwasRemainsTex",
        "__OTR__item_name_static/gItemNameOdolwasRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsOdolwaDL",
        "You got %rOdolwa's Remains%w! The&jungle warrior's mask still&keeps the beat of his dance.^"
        "Hold %y\x9F%w to run as he danced;&%y\xA3%w+%y\xA0%w calls his beetles.",
        "%rOdolwa's Remains&%wPress %y\xA1%w to wear it. Worn: %y\x9F%w runs,&%y\xA3%w+%y\xA0%w calls beetles.",
    },
    {
        "nei.mm_remains_goht",
        "__OTR__icon_item_static_yar/gItemIconGohtsRemainsTex",
        "__OTR__item_name_static/gItemNameGohtsRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsGohtDL",
        "You got %rGoht's Remains%w! The&mechanical bull's mask is still&warm with its charge.^"
        "Hold %y\x9F%w to charge, %y\xA3%w+%y\x9F%w to&pound, hold %y\xA0%w for thunder.",
        "%rGoht's Remains&%wPress %y\xA1%w to wear it. Worn: %y\x9F%w&charges, %y\xA0%w calls thunder.",
    },
    {
        "nei.mm_remains_gyorg",
        "__OTR__icon_item_static_yar/gItemIconGyorgsRemainsTex",
        "__OTR__item_name_static/gItemNameGyorgsRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsGyorgDL",
        "You got %rGyorg's Remains%w! The&masked fish still breathes&where Link cannot.^"
        "Underwater, %y\xA3%w calls its school&and holding %y\xA0%w stirs a whirlpool.",
        "%rGyorg's Remains&%wPress %y\xA1%w to wear it. Worn: endless&breath, fish and a whirlpool.",
    },
    {
        "nei.mm_remains_twinmold",
        "__OTR__icon_item_static_yar/gItemIconTwinmoldsRemainsTex",
        "__OTR__item_name_static/gItemNameTwinmoldsRemainsENGTex",
        "__OTR__objects/object_bsmask/gRemainsTwinmoldDL",
        "You got %rTwinmold's Remains%w!&Equip to %y\xA1%w to summon a shadow&wearing your mask. Costs 24 MP.^"
        "Wear it to open Orders: attack,&help, follow or wait. Your first&choice is included in summoning.^"
        "Press %y\xA1%w again for orders.&Changing mode costs 24 MP.&Choosing the same mode is free.^"
        "Help follows and lends strength.&Slash a cracked wall for a bomb.&Target a cucco to have it fetched.^"
        "Face him nearby and press %b\x9F%w.&He lifts you so you can aim.&Stand together for extra strength.^"
        "Aim a lift with the Stick.&%b\x9F%w launches; %g\xA0%w cancels.&Twin Link stays until you die.",
        "%rTwinmold's Remains&%wEquip to %y\xA1%w to wear it and&open Orders. Summon: 24 MP.&Later mode changes: 24 MP.",
    },
};

static const ALIGN_ASSET(2) char sSonataIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sLullabyIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sBossaNovaIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sElegyIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sOathIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sHealingIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sSoaringIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sFugueIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sBalladIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";
static const ALIGN_ASSET(2) char sCommandIcon[] = "__OTR__icon_item_static_yar/gItemIconSongNoteTex";

// Majora's Mask names its own seven songs; the last three are NEI's and keep their names.
static const ALIGN_ASSET(2) char sSonataName[] = "__OTR__item_name_static/gItemNameSonataOfAwakeningENGTex";
static const ALIGN_ASSET(2) char sLullabyName[] = "__OTR__item_name_static/gItemNameGoronLullabyENGTex";
static const ALIGN_ASSET(2) char sBossaNovaName[] = "__OTR__item_name_static/gItemNameNewWaveBossaNovaENGTex";
static const ALIGN_ASSET(2) char sElegyName[] = "__OTR__item_name_static/gItemNameElegyOfEmptynessENGTex";
static const ALIGN_ASSET(2) char sOathName[] = "__OTR__item_name_static/gItemNameOathToOrderENGTex";
static const ALIGN_ASSET(2) char sHealingName[] = "__OTR__item_name_static/gItemNameSongOfHealingENGTex";
static const ALIGN_ASSET(2) char sSoaringName[] = "__OTR__item_name_static/gItemNameSongOfSoaringENGTex";
static const ALIGN_ASSET(2) char sFugueName[] = "__OTR__textures/item_name_custom/gFugueOfHomeNameTex";
static const ALIGN_ASSET(2) char sBalladName[] = "__OTR__textures/item_name_custom/gBalladOfHeroNameTex";
static const ALIGN_ASSET(2) char sCommandName[] = "__OTR__textures/item_name_custom/gCommandMelodyNameTex";

// The seven Majora's Mask melodies are the authoritative button sequences translated to Ocarina of
// Time's pitches. The last three have no melody of their own yet: theirs are provisional and picked
// so they collide with nothing vanilla teaches.
static const SongDefinition sSongs[] = {
    {
        "nei.mm_song_sonata",
        sSonataIcon,
        sSonataName,
        7,
        { NOTE_C_UP, NOTE_C_LEFT, NOTE_C_UP, NOTE_C_LEFT, NOTE_A, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rSonata of Awakening%w! The song that woke a sleeping temple out of the swamp.",
        "%rSonata of Awakening&%w\xA2\xA1\xA2\xA1\x9F\xA1\x9F The ocarina answers&it.",
    },
    {
        "nei.mm_song_goron_lullaby",
        sLullabyIcon,
        sLullabyName,
        8,
        { NOTE_A, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_A, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rGoron Lullaby%w! A father's song, long enough to put a mountain to sleep.",
        "%rGoron Lullaby&%w\x9F\xA1\xA1\x9F\xA1\xA1\xA1\x9F The ocarina answers&it.",
    },
    {
        "nei.mm_song_new_wave",
        sBossaNovaIcon,
        sBossaNovaName,
        7,
        { NOTE_C_LEFT, NOTE_C_UP, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_RIGHT },
        "You learned the %rNew Wave Bossa Nova%w! The Indigo-Go's last song, the one the eggs of the sea sing "
        "back.",
        "%rNew Wave Bossa Nova&%w\xA1\xA2\xA1\xA1\xA1\xA1\xA1 The ocarina answers&it.",
    },
    {
        "nei.mm_song_elegy",
        sElegyIcon,
        sElegyName,
        7,
        { NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP, NOTE_C_LEFT },
        "You learned the %rElegy of Emptiness%w! It leaves a hollow copy of you standing exactly where you stood.",
        "%rElegy of Emptiness&%w\xA1\xA1\xA1\xA1\xA1\xA2\xA1 The ocarina answers&it.",
    },
    {
        "nei.mm_song_oath",
        sOathIcon,
        sOathName,
        6,
        { NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_A, NOTE_C_DOWN, NOTE_C_RIGHT, NOTE_C_UP },
        "You learned the %rOath to Order%w! The call that brings the four giants down from the edges of "
        "the world.",
        "%rOath to Order&%w\xA1\xA1\x9F\xA1\xA1\xA2 The ocarina answers it.",
    },
    {
        "nei.mm_song_healing",
        sHealingIcon,
        sHealingName,
        6,
        { NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_RIGHT, NOTE_C_DOWN },
        "You learned the %rSong of Healing%w! Sorrow goes into the melody, and a mask comes out.",
        "%rSong of Healing&%w\xA1\xA1\xA1\xA1\xA1\xA1 The ocarina answers it.",
    },
    {
        "nei.mm_song_soaring",
        sSoaringIcon,
        sSoaringName,
        6,
        { NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_UP, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_C_UP },
        "You learned the %rSong of Soaring%w! Every owl that ever watched you is listening for it.",
        "%rSong of Soaring&%w\xA1\xA1\xA2\xA1\xA1\xA2 The ocarina answers it.",
    },
    {
        "nei.mm_song_fugue_of_home",
        sFugueIcon,
        sFugueName,
        6,
        { NOTE_C_LEFT, NOTE_C_LEFT, NOTE_C_DOWN, NOTE_C_UP, NOTE_C_RIGHT, NOTE_A },
        "You learned the %rFugue of Home%w! A song that remembers the way back better than you do.",
        "%rFugue of Home&%w\xA1\xA1\xA1\xA2\xA1\x9F The ocarina answers it.",
    },
    {
        "nei.mm_song_ballad_of_hero",
        sBalladIcon,
        sBalladName,
        6,
        { NOTE_C_UP, NOTE_C_UP, NOTE_C_RIGHT, NOTE_C_DOWN, NOTE_C_LEFT, NOTE_A },
        "You learned the %rBallad of Hero%w! Someone wrote down what you did and set it to six notes.",
        "%rBallad of Hero&%w\xA2\xA2\xA1\xA1\xA1\x9F The ocarina answers it.",
    },
    {
        "nei.mm_song_command_melody",
        sCommandIcon,
        sCommandName,
        6,
        { NOTE_A, NOTE_A, NOTE_C_UP, NOTE_C_RIGHT, NOTE_C_LEFT, NOTE_C_DOWN },
        "You learned the %rCommand Melody%w! Play it and something else takes its orders from you.",
        "%rCommand Melody&%w\x9F\x9F\xA2\xA1\xA1\xA1 The ocarina answers it.",
    },
};

#define SONG_COUNT ((s32)ARRAY_COUNT(sSongs))

static const char* const sRequiredHooks[] = {
    "OnPlayerUpdate",        "OnPlayerPostLimbDraw",       "OnPlayerResolveLimbDraw",
    "OnPlayerResolveAnim",   "OnPlayerResolveMotionScale", "OnPlayerActionHandler",
    "OnPlayerShieldBlocked", "OnActorDrawEnd", "OnActorUpdate",
    "OnPlayerFilterInput", "OnInterfaceDrawEnd", "OnGameFrameUpdate", "OnSceneInit", "OnOcarinaNote",         "OnLoadGame",
};
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static u8 sPlayedNotes[SONG_MAX_NOTES];
static s32 sPlayedCount;

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
}

#include "collect_behaviors.inc.c"
#include "collect_background.inc.c"

static void RecognizeSong(u8 note, f32 modulator, s8 bend) {
    // The hook also fires on release, where the note is invalid: dropping those is what keeps a
    // melody's two identical notes in a row from collapsing into one.
    if ((gPlayState == NULL) || (note == OCARINA_PITCH_NONE)) {
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

static const ALIGN_ASSET(2) char sSongNoteDL[]="__OTR__objects/object_gi_melody/gGiSongNoteDL";
static const ALIGN_ASSET(2) char sSongColorDL[6][80]={
 "__OTR__objects/object_gi_melody/gGiMinuetColorDL","__OTR__objects/object_gi_melody/gGiBoleroColorDL",
 "__OTR__objects/object_gi_melody/gGiSerenadeColorDL","__OTR__objects/object_gi_melody/gGiRequiemColorDL",
 "__OTR__objects/object_gi_melody/gGiNocturneColorDL","__OTR__objects/object_gi_melody/gGiPreludeColorDL"
};
static void DrawSongGetItem(PlayState* play,GetItemEntry* entry) {
 const SOHCustomItemDefinition* item=sApi->FindCustomItemByRuntimeId(entry->tableId);int song=0;
 if(item!=NULL)for(int i=0;i<SONG_COUNT;++i)if(strcmp(item->key,sSongs[i].key)==0)song=i;
 OPEN_DISPS(play->state.gfxCtx);
 Gfx_SetupDL_25Xlu(play->state.gfxCtx);
 gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
 gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sSongColorDL[song%6]);
 gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sSongNoteDL);
 CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawRemainsGetItem(PlayState* play,GetItemEntry* entry) {
 const SOHCustomItemDefinition* item=sApi->FindCustomItemByRuntimeId(entry->tableId);
 int remains=item!=NULL?RemainsIndexForKey(item->key):-1;if(remains<0)return;
 OPEN_DISPS(play->state.gfxCtx);
 Gfx_SetupDL_25Opa(play->state.gfxCtx);
 Matrix_Scale(.016f,.016f,.016f,MTXMODE_APPLY);
 gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
 gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sRemains[remains].getItemModelPath);
 CLOSE_DISPS(play->state.gfxCtx);
}
static void RegisterRemains(void) {
    for (s32 i = 0; i < REMAINS_COUNT; i++) {
        const RemainsDefinition* source = &sRemains[i];
        SOHCustomItemDefinition definition = Z64Items_Define(source->key, source->iconPath, source->namePath);

        Z64Items_SetButtons(&definition, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
        definition.flags |= SOH_CUSTOM_ITEM_INSTANT;
        if (i != REMAINS_TWINMOLD) definition.flags |= SOH_CUSTOM_ITEM_WEARABLE;
        Z64Items_SetPlacement(&definition, MM_COLLECTABLES_PAGE, (u8)(REMAINS_FIRST_SLOT + i), 0);
        Z64Items_SetTextbox(&definition, source->getItemText);
        Z64Items_SetPauseText(&definition, source->pauseText);
        definition.getItemEntry.drawFunc=DrawRemainsGetItem;
        Z64Items_SetAction(&definition, sRemainsToggle[i], NULL);
        definition.onRemove = i == REMAINS_TWINMOLD ? TwinForget : ForgetWornRemains;
        const SOHCustomItemRandomizer randomizer = {
            sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, source->key, source->key, "",
        };
        Z64Items_SetLogic(&definition, &randomizer);

        Z64Items_Register(sApi, &definition);
    }
}

static void RegisterSongs(void) {
    static const u8 cells[] = { 6, 7, 8, 9, 10, 13, 15, 14, 16, 12 };
    for (s32 i = 0; i < SONG_COUNT; i++) {
        const SongDefinition* source = &sSongs[i];
        SOHCustomItemDefinition definition = Z64Items_Define(source->key, source->iconPath, source->namePath);

        Z64Items_SetPlacement(&definition, MM_COLLECTABLES_PAGE, cells[i], 0);
        definition.getItemEntry.drawFunc=DrawSongGetItem;
        Z64Items_SetTextbox(&definition, source->getItemText);
        Z64Items_SetPauseText(&definition, source->pauseText);
        // An item, not a song: the song pool is sized for OoT's twelve and would leave these without a spot.
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
    RemainsAllies_Register(sApi);
    Remains_Init(sApi);
    RegisterCollectBehaviors();
    TwinRegister();
    SOH_REGISTER_HOOK(sApi,OnInterfaceDrawEnd,DrawMmQuestBackground);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, RecognizeSong);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, StartFile);
}
