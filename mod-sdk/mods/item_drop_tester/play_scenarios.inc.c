#define PLAY_PREFIX "play:"
#define PLAY_FRAMES 150
#define PLAY_PRESS_INTERVAL 24
#define PLAY_PAUSE_FRAME 5
#define PLAY_UNPAUSE_INTERVAL 30

typedef struct {
    const char* name;
    u16 heldButtons;
    u16 pressedButtons;
    bool opensPause;
} PlayAction;

static const PlayAction sPlayActions[] = {
    { "idle", 0, 0, false },      { "slash", 0, BTN_B, false }, { "charge", BTN_B, 0, false },
    { "guard", BTN_R, 0, false }, { "roll", 0, BTN_A, false },  { "pause", 0, 0, true },
};

static const PlayAction* sPlayAction;
static s32 sPlayFrames;
static bool sIsPlaying;

static const PlayAction* FindPlayAction(const char* name) {
    for (size_t i = 0; i < ARRAY_COUNT(sPlayActions); ++i) {
        if (strcmp(sPlayActions[i].name, name) == 0) {
            return &sPlayActions[i];
        }
    }
    return NULL;
}

static bool ApplyPlaySetup(const char* setup, const char* key) {
    if (strcmp(setup, "equip") == 0) {
        return sApi->SetEquipmentOwned(key, true) && sApi->WearEquipment(key);
    }
    if (strcmp(setup, "form") == 0) {
        return sApi->SetActiveForm(key);
    }
    return strcmp(setup, "none") == 0;
}

static bool StartPlayScenario(const char* request) {
    char setup[32];
    char key[96];
    char action[32];
    if (sscanf(request + strlen(PLAY_PREFIX), "%31[^:]:%95[^:]:%31s", setup, key, action) != 3) {
        return false;
    }
    sPlayAction = FindPlayAction(action);
    if (sPlayAction == NULL || !ApplyPlaySetup(setup, key)) {
        return false;
    }
    sPlayFrames = 0;
    sIsPlaying = true;
    return true;
}

static bool IsPlayRequest(const char* request) {
    return strncmp(request, PLAY_PREFIX, strlen(PLAY_PREFIX)) == 0;
}

static void PressPlayButton(PlayState* play, u16 button) {
    play->state.input[0].press.button |= button;
    play->state.input[0].cur.button |= button;
}

static void PressPlayButtons(void) {
    PlayState* play = gPlayState;
    if (play == NULL) {
        return;
    }
    if (!sIsPlaying) {
        if (play->pauseCtx.state != 0 && sPlayAction != NULL && ++sPlayFrames % PLAY_UNPAUSE_INTERVAL == 0) {
            PressPlayButton(play, BTN_START);
        }
        return;
    }
    play->state.input[0].cur.button |= sPlayAction->heldButtons;
    if (sPlayAction->pressedButtons != 0 && sPlayFrames % PLAY_PRESS_INTERVAL == 0) {
        PressPlayButton(play, sPlayAction->pressedButtons);
    }
    if (sPlayAction->opensPause && sPlayFrames == PLAY_PAUSE_FRAME) {
        play->pauseCtx.pageIndex = PLAY_EQUIPMENT_PAGE;
        PressPlayButton(play, BTN_START);
    }
}

static bool HasPlayFinished(void) {
    return ++sPlayFrames >= PLAY_FRAMES;
}
