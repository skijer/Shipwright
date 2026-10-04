// Night music for Hyrule Field. Port of marsh6487's HyruleFieldNightMusic (bc149477..fcc86528, soh/Enhancements/audio).
//
// The original is a state machine in the audio code. Here it is one: idle until Link is in Hyrule Field at night
// with the option on, then the chosen track starts on the main BGM player; when day comes, or Link leaves, or the
// option goes off, the scene's own sequence is started again. Anything else taking the main player (the battle
// theme, a boss, a cutscene) ends our ownership without a fight, and when the vanilla enemy handoff returns it
// resumes the track that was playing, which is ours.

#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "sequence.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define CVAR_ENABLED "gMods.HyruleFieldNight.Enabled"
#define CVAR_TRACK "gMods.HyruleFieldNight.Track"
#define FADE_FRAMES 30
// The engine's own macro (code_800EC960.c), which is not in a header.
#define StartSeq(player, fade, seq) \
    Audio_QueueSeqCmd(0x00000000 | ((u8)(player) << 24) | ((u8)(fade) << 0x10) | (u16)(seq))

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static bool sOwnsBgm;
static u16 sPlaying;

static bool IsBusy(PlayState* play) {
    return play->csCtx.state != CS_STATE_IDLE || func_800FA0B4(SEQ_PLAYER_FANFARE) != NA_BGM_DISABLED ||
           play->transitionTrigger != TRANS_TRIGGER_OFF;
}

static void Release(PlayState* play, bool restoreScene) {
    if (sOwnsBgm && restoreScene && func_800FA0B4(SEQ_PLAYER_BGM_MAIN) == sPlaying) {
        StartSeq(SEQ_PLAYER_BGM_MAIN, FADE_FRAMES, play->sequenceCtx.seqId);
    }
    sOwnsBgm = false;
    sPlaying = NA_BGM_DISABLED;
}

static void Tick(void) {
    PlayState* play = gPlayState;
    bool wanted;
    u16 track;

    if (play == NULL) {
        sOwnsBgm = false;
        return;
    }
    wanted = play->sceneNum == SCENE_HYRULE_FIELD && IS_NIGHT && CVarGetInteger(CVAR_ENABLED, 1) &&
             !IsBusy(play);
    track = (u16)CLAMP(CVarGetInteger(CVAR_TRACK, NA_BGM_KAKARIKO_ADULT), 0, 0x7F);

    if (sOwnsBgm) {
        u16 current = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);

        if (current != sPlaying) {
            // Something else has the player. Leave it alone; the enemy handoff gives the track back by itself.
            if (current != NA_BGM_ENEMY && current != NA_BGM_DISABLED) {
                sOwnsBgm = false;
            }
            return;
        }
        if (!wanted || track != sPlaying) {
            Release(play, play->sceneNum != SCENE_HYRULE_FIELD || !IS_NIGHT || !CVarGetInteger(CVAR_ENABLED, 1) ||
                              track != sPlaying);
        }
        return;
    }
    if (wanted && func_800FA0B4(SEQ_PLAYER_BGM_MAIN) != NA_BGM_ENEMY) {
        StartSeq(SEQ_PLAYER_BGM_MAIN, FADE_FRAMES, track);
        sPlaying = track;
        sOwnsBgm = true;
    }
}

static void RegisterMenu(void) {
    SOHModMenuWidget toggle = { sizeof(SOHModMenuWidget) };
    SOHModMenuWidget slider = { sizeof(SOHModMenuWidget) };

    toggle.section = "Audio";
    toggle.sidebar = "Music";
    toggle.type = SOH_MOD_MENU_CHECKBOX;
    toggle.label = "Hyrule Field Night Music";
    toggle.cvar = CVAR_ENABLED;
    toggle.tooltip = "Play a music track in Hyrule Field at night.";
    toggle.defaultInt = 1;
    sApi->RegisterMenuWidget(&toggle);

    slider.section = "Audio";
    slider.sidebar = "Music";
    slider.type = SOH_MOD_MENU_INT_SLIDER;
    slider.label = "Night Track (sequence id)";
    slider.cvar = CVAR_TRACK;
    slider.tooltip = "The sequence id played at night. 25 is Kakariko Village (adult).";
    slider.defaultInt = NA_BGM_KAKARIKO_ADULT;
    slider.minInt = 0;
    slider.maxInt = 0x7F;
    sApi->RegisterMenuWidget(&slider);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterMenu();
    }
}
