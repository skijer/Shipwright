// Pause support stays in the mod. The engine's layout draws custom items but only knows OoT songs.
#include "textures/parameter_static/parameter_static.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

extern HOST_DATA OcarinaNote* sPlaybackSong;
#include "nei_song_playback.inc.c"

static OcarinaNote sPauseMelody[40];
static OcarinaNote* sPreviousPlayback;
static int sPauseSong = -1, sHoverSong = -1, sHoverFrames;
static int sAutoplaySong = -1, sAutoplayWait, sPlaybackFrames;
static bool sPlayingFromPause;

static int SelectedMmSong(PlayState* play) {
    PauseContext* pause = &play->pauseCtx;
    if (pause->state != 6 || pause->pageIndex != PAUSE_QUEST || pause->cursorSpecialPos != 0 ||
        pause->unk_1E4 != 0 || pause->cursorItem[PAUSE_QUEST] != ITEM_CUSTOM ||
        !ModLayout_IsActive(SOH_LAYOUT_COLLECTABLES)) return -1;
    const SOHCustomItemDefinition* item = CustomItemRegistry_GetPauseItem();
    if (item == NULL) return -1;
    for (int i = 0; i < SONG_COUNT; ++i)
        if (!strcmp(item->key, sSongs[i].key) && IsSongOwned(&sSongs[i])) return i;
    return -1;
}

static void StopPauseMelody(void) {
    if (sPauseSong < 0) return;
    // Leave another player's instrument and sequence alone if it has taken ownership.
    if (sPlaybackSong == &sPauseMelody[1]) {
        AudioOcarina_SetPlaybackSong(0, 0);
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sPlaybackSong = sPreviousPlayback;
    }
    sPauseSong = -1;
    sPlayingFromPause = false;
}

static void StartPauseMelody(int song, bool autoplay) {
    StopPauseMelody();
    memset(sPauseMelody, 0, sizeof(sPauseMelody));
    // The native player reads the previous note's attributes, including at position zero.
    // Leave a sentinel before the sequence and an explicit length-zero terminator after it.
    for (int i = 0; i < sSongs[song].noteCount; ++i) {
        OcarinaNote* note = &sPauseMelody[1 + i * 2];
        note->pitch = sSongs[song].notes[i];
        note->length = 18;
        note->volume = 100;
        note[1].pitch = OCARINA_PITCH_NONE;
        note[1].length = 3; // Separate repeated notes, including Command Melody's two A presses.
        note[1].volume = 100;
    }
    if (song == 7) memcpy(&sPauseMelody[1], sNeiPlayback0, sizeof(sNeiPlayback0));
    if (song == 8) memcpy(&sPauseMelody[1], sNeiPlayback2, sizeof(sNeiPlayback2));
    if (song == 9) memcpy(&sPauseMelody[1], sNeiPlayback1, sizeof(sNeiPlayback1));
    sPreviousPlayback = sPlaybackSong;
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
    // Initialize native playback counters with a valid song, then select mod-owned note data
    // while the instrument is off. No vanilla song table or saved scarecrow melody is changed.
    AudioOcarina_SetPlaybackSong(1, 1);
    sPlaybackSong = &sPauseMelody[1];
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
    sPauseSong = song;
    sPlayingFromPause = autoplay;
    sPlaybackFrames = 0;
}

static void CollectPauseInput(PlayState* play, Input* input, bool* handled) {
    if (*handled) return;
    int song = SelectedMmSong(play);
    if (song < 0 || !(input->press.button & BTN_A) ||
        (input->press.button & (BTN_START | BTN_B | BTN_R | BTN_Z | BTN_L)) ||
        ABS(input->rel.stick_x) > 30 || ABS(input->rel.stick_y) > 30 ||
        (input->cur.button & (BTN_DLEFT | BTN_DRIGHT | BTN_DUP | BTN_DDOWN))) return;
    StopPauseMelody();
    sAutoplaySong = song;
    sAutoplayWait = 120;
    // Use the normal pause-close path so it restores HUD, buttons, camera and audio itself.
    input->press.button &= ~BTN_A;
    input->press.button |= BTN_START;
}

static void CollectPauseTick(void) {
    PlayState* play = gPlayState;
    if (play == NULL) { StopPauseMelody(); sAutoplaySong = -1; return; }
    if (sAutoplaySong >= 0) {
        if (--sAutoplayWait <= 0 || play->gameOverCtx.state != GAMEOVER_INACTIVE ||
            play->transitionTrigger != TRANS_TRIGGER_OFF) {
            sAutoplaySong = -1;
        } else if (play->pauseCtx.state == 0) {
            int song = sAutoplaySong;
            sAutoplaySong = -1;
            if (play->msgCtx.msgMode == MSGMODE_NONE && IsSongOwned(&sSongs[song])) {
                ForgetPlayedNotes();
                StartPauseMelody(song, true);
            }
        }
    }
    if (sPauseSong < 0) return;
    if (play->gameOverCtx.state != GAMEOVER_INACTIVE || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        (sPlayingFromPause ? (play->pauseCtx.state != 0 || play->msgCtx.msgMode != MSGMODE_NONE)
                           : SelectedMmSong(play) != sPauseSong) ||
        sPlaybackSong != &sPauseMelody[1] || ++sPlaybackFrames > 600) {
        StopPauseMelody();
        return;
    }
    if (sPlaybackFrames > 3 && AudioOcarina_GetPlaybackStaff()->state == 0) {
        int song = sPauseSong;
        bool autoplay = sPlayingFromPause;
        StopPauseMelody();
        if (autoplay) {
            // Deliver recognition on the game thread after playback, so existing song effects
            // and other NEI listeners (such as Command Melody) follow their normal path.
            for (int i = 0; i < sSongs[song].noteCount; ++i) {
                GameInteractor_ExecuteOnOcarinaNote(sSongs[song].notes[i], 1.0f, 0);
                GameInteractor_ExecuteOnOcarinaNote(OCARINA_PITCH_NONE, 1.0f, 0);
            }
        }
    }
}

static void CollectPauseReset(int32_t file) {
    StopPauseMelody();
    sAutoplaySong = sHoverSong = -1;
    sHoverFrames = 0;
}
static void CollectPauseScene(int16_t scene) { CollectPauseReset(0); }

static int PauseNoteButton(u8 pitch) {
    switch (pitch) {
        case NOTE_A: return 0;
        case NOTE_C_DOWN: return 1;
        case NOTE_C_RIGHT: return 2;
        case NOTE_C_LEFT: return 3;
        default: return 4;
    }
}

static void DrawCollectPause(PlayState* play) {
    PauseContext* pause = &play->pauseCtx;
    if (pause->state != 6 || pause->pageIndex != PAUSE_QUEST || pause->cursorSpecialPos != 0 ||
        pause->unk_1E4 != 0 || !ModLayout_IsActive(SOH_LAYOUT_COLLECTABLES)) {
        sHoverSong = -1; sHoverFrames = 0; return;
    }
    int song = SelectedMmSong(play);
    if (song != sHoverSong) { sHoverSong = song; sHoverFrames = 0; }
    if (song >= 0 && ++sHoverFrames == 12 && sAutoplaySong < 0) StartPauseMelody(song, false);

    const SOHCustomItemDefinition* item = pause->cursorItem[PAUSE_QUEST] == ITEM_CUSTOM
                                            ? CustomItemRegistry_GetPauseItem() : NULL;
    const char* iconPath = NULL;
    if (item != NULL) {
        if (song < 0 && RemainsIndexForKey(item->key) < 0) return;
        iconPath = item->iconPath;
    } else if (pause->cursorItem[PAUSE_QUEST] < ARRAY_COUNT(gItemIcons)) {
        iconPath = (const char*)gItemIcons[pause->cursorItem[PAUSE_QUEST]];
        if (pause->cursorItem[PAUSE_QUEST] == ITEM_HEART_PIECE)
            iconPath = (const char*)gItemIcons[0x79 + ((gSaveContext.inventory.questItems >> 28) & 0xF)];
    }
    if (iconPath == NULL) return;

    // Locate this frame's actual icon quad. This also respects edits made in the layout editor.
    Vtx* icon = NULL;
    Gfx* cursorCommand = NULL;
    Gfx* end = play->state.gfxCtx->polyOpa.p;
    Vtx* lastQuad = NULL;
    Gfx combine = {0}, prim = {0}, env = {0};
    for (Gfx* cmd = play->state.gfxCtx->polyOpa.bufp; cmd < end; ++cmd) {
        unsigned op = (cmd->words.w0 >> 24) & 255;
        if (op == G_SETCOMBINE) combine = *cmd;
        if (op == G_SETPRIMCOLOR) prim = *cmd;
        if (op == G_SETENVCOLOR) env = *cmd;
        if (op == G_VTX || op == G_VTX_WIDE) {
            if (cmd->words.w1 == (uintptr_t)pause->cursorVtx) { cursorCommand = cmd; break; }
            lastQuad = (Vtx*)(uintptr_t)cmd->words.w1;
        }
        if (op == G_SETTIMG && (uintptr_t)cmd->words.w1 > UINT32_MAX) {
            char* path = (char*)(uintptr_t)cmd->words.w1;
            if (ResourceMgr_OTRSigCheck(path) && !strcmp(path, iconPath))
                icon = lastQuad;
        }
    }
    if (cursorCommand == NULL || icon == NULL) return;
    int x = icon[0].v.ob[0] - 4, y = icon[0].v.ob[1] + 4;
    int right = icon[1].v.ob[0] + 4 - 16, bottom = icon[2].v.ob[1] - 4 + 16;
    for (int corner = 0; corner < 4; ++corner) {
        int left = corner & 1 ? right : x;
        int top = corner & 2 ? bottom : y;
        for (int v = 0; v < 4; ++v) {
            pause->cursorVtx[corner * 4 + v].v.ob[0] = left + (v & 1 ? 16 : 0);
            pause->cursorVtx[corner * 4 + v].v.ob[1] = top - (v & 2 ? 16 : 0);
        }
    }
    if (song < 0) return;

    // Insert the staff before the cursor's vertex load, where the quest-page matrix is active.
    // Restore its combine/colors and vertex load before returning to the original display list.
    Gfx* list = Graph_Alloc(play->state.gfxCtx, sizeof(Gfx) * (SONG_MAX_NOTES * 12 + 12));
    Vtx* notes = Graph_Alloc(play->state.gfxCtx, sizeof(Vtx) * SONG_MAX_NOTES * 4);
    if (list == NULL || notes == NULL) return;
    Gfx* out = list;
    static const char* const textures[] = { gOcarinaBtnIconATex, gOcarinaBtnIconCDownTex,
        gOcarinaBtnIconCRightTex, gOcarinaBtnIconCLeftTex, gOcarinaBtnIconCUpTex };
    gDPPipeSync(out++);
    gDPSetCombineMode(out++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (int i = 0; i < sSongs[song].noteCount; ++i) {
        int button = PauseNoteButton(sSongs[song].notes[i]);
        int nx = -105 + i * 12, ny = -48 + button * 4 + pause->offsetY;
        for (int v = 0; v < 4; ++v) {
            Vtx* vertex = &notes[i * 4 + v];
            memset(vertex, 0, sizeof(*vertex));
            vertex->v.ob[0] = nx + (v & 1 ? 12 : 0);
            vertex->v.ob[1] = ny - (v & 2 ? 12 : 0);
            vertex->v.tc[0] = v & 1 ? 16 << 5 : 0;
            vertex->v.tc[1] = v & 2 ? 16 << 5 : 0;
            memset(vertex->v.cn, 255, 4);
        }
        gDPPipeSync(out++);
        if (button == 0) { gDPSetPrimColor(out++, 0, 0, 80, 150, 255, pause->alpha); }
        else { gDPSetPrimColor(out++, 0, 0, 255, 220, 40, pause->alpha); }
        gSPVertex(out++, (uintptr_t)&notes[i * 4], 4, 0);
        gDPLoadTextureBlock(out++, textures[button], G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
            G_TX_NOLOD, G_TX_NOLOD);
        gSP1Quadrangle(out++, 0, 2, 3, 1, 0);
    }
    gDPPipeSync(out++);
    *out++ = combine; *out++ = prim; *out++ = env;
    *out++ = *cursorCommand;
    gSPEndDisplayList(out++);
    gSPDisplayList(cursorCommand, list);
}

static void RegisterCollectPause(void) {
    SOH_REGISTER_HOOK(sApi, OnKaleidoInput, CollectPauseInput);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, CollectPauseTick);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawCollectPause);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, CollectPauseScene);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, CollectPauseReset);
    SOH_REGISTER_HOOK(sApi, OnExitGame, CollectPauseReset);
}
