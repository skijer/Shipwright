#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z64items.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/ModApi/VanillaItems/VanillaItems.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <ctype.h>
#include "soh/ModApi/ModMenu/ModUI.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"

#define KEY_SIZE 256
#define AUTO_CVAR(name) "gMods.ItemDropTester.Auto." name
#define AUTO_TEXT_SIZE 512
#define AUTO_PICKUP_TIMEOUT 200
#define AUTO_PRINT_TIMEOUT 100
#define AUTO_SETTLE_FRAMES 10
#define CUSTOM_DROP_HEIGHT 80.0f
#define MESSAGE_NEWLINE_CODE 0x01
#define MESSAGE_END_CODE 0x02
#define MESSAGE_ITEM_ICON_CODE 0x13
#define NO_MESSAGE_ICON -1
#define PLAY_EQUIPMENT_PAGE PAUSE_EQUIP
typedef enum { AUTO_IDLE, AUTO_QUEUED, AUTO_FALLING, AUTO_PLAYING, AUTO_SHOWN, AUTO_CLOSING } AutoPhase;
static const u8 sControlArguments[' '] = {
    [0x05] = 1, [0x06] = 1, [0x07] = 2, [0x0C] = 1, [0x0E] = 1, [0x11] = 2,
    [0x12] = 2, [0x13] = 1, [0x14] = 1, [0x15] = 3, [0x1E] = 1,
};
GetItemEntry ItemTable_RetrieveEntry(s16 tableId, s16 getItemId);
GetItemEntry GetItemMystery(void);
typedef struct {
    int32_t rg;
    const char* name;
    const char* key;
} NativeItem;
static const NativeItem sNative[] = {
#include "rando_catalog.inc.h"
};
static int32_t sPendingRg = -1;
static int32_t sCatalog;
static int32_t sSelectedRg = -1;
static const SOHModApi* sApi;
static bool sChecklistDirty;
static AutoPhase sAutoPhase;
static int32_t sAutoFrames;
static int32_t sAutoTextFrames;
static char sPending[KEY_SIZE];
static char sSelected[KEY_SIZE];
static char sFilter[128];
static const char* sStatus = "Click an item to queue a pickup above Link. Resume gameplay to spawn it.";
static const char* const sHooks[] = { "OnGameFrameUpdate", "OnGameStateMainStart" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, ARRAY_COUNT(sHooks) };

static void DrawNativeDrop(Actor* actor, PlayState* play) {
    EnItem00* item = (EnItem00*)actor;
    GetItemEntry entry = item->itemEntry;
    if (CVarGetInteger("gRandoEnhancements.MysteriousShuffle", 0))
        entry = GetItemMystery();
    Matrix_Scale(CVarGetFloat("gRandoEnhancements.TimeSavers.SkipGetItemAnimationScale", 10.0f),
                 CVarGetFloat("gRandoEnhancements.TimeSavers.SkipGetItemAnimationScale", 10.0f),
                 CVarGetFloat("gRandoEnhancements.TimeSavers.SkipGetItemAnimationScale", 10.0f), MTXMODE_APPLY);
    func_8002EBCC(actor, play, 0);
    func_8002ED80(actor, play, 0);
    EnItem00_CustomItemsParticles(actor, play, entry);
    GetItemEntry_Draw(play, entry);
}
static bool HasPendingDrop(void) {
    return sPending[0] != '\0' || sPendingRg >= 0;
}

static bool CanSpawnDrop(PlayState* play) {
    if (play->pauseCtx.state != 0 || play->msgCtx.msgMode != 0 || play->transitionTrigger != 0) {
        return false;
    }
    return !Player_InBlockingCsMode(play, GET_PLAYER(play));
}

static EnItem00* SpawnCustomDrop(PlayState* play, const Vec3f* feet, const char* key) {
    Vec3f pos = *feet;
    pos.y += CUSTOM_DROP_HEIGHT;
    EnItem00* drop = Item_DropCollectible2(play, &pos, ITEM00_SOH_CUSTOM);
    if (drop != NULL && !CustomItemRegistry_AttachDrop(&drop->actor, key)) {
        Actor_Kill(&drop->actor);
        return NULL;
    }
    return drop;
}

static EnItem00* SpawnNativeDrop(PlayState* play, Vec3f* feet, int32_t rg) {
    GetItemEntry entry = ItemTable_RetrieveEntry(MOD_RANDOMIZER, (s16)rg);
    if (CustomItemRegistry_IsCustomGetItem(&entry)) {
        const SOHCustomItemDefinition* replacement = sApi->FindCustomItemByRuntimeId(entry.tableId);
        if (replacement != NULL) {
            return SpawnCustomDrop(play, feet, replacement->key);
        }
    }
    if (entry.gi == 0) {
        return NULL;
    }
    EnItem00* drop = Item_DropCollectible2(play, feet, ITEM00_SOH_GIVE_ITEM_ENTRY_GI);
    if (drop != NULL) {
        drop->itemEntry = entry;
        drop->actor.draw = DrawNativeDrop;
    }
    return drop;
}

static void ClearPendingDrop(void) {
    sPending[0] = '\0';
    sPendingRg = -1;
}

static EnItem00* SpawnPendingDrop(PlayState* play) {
    Vec3f feet = GET_PLAYER(play)->actor.world.pos;
    EnItem00* drop =
        sPending[0] != '\0' ? SpawnCustomDrop(play, &feet, sPending) : SpawnNativeDrop(play, &feet, sPendingRg);
    ClearPendingDrop();
    return drop;
}

static bool QueueByName(const char* name) {
    if (sApi->FindCustomItem(name) != NULL) {
        snprintf(sPending, sizeof(sPending), "%s", name);
        return true;
    }
    for (size_t i = 0; i < ARRAY_COUNT(sNative); ++i) {
        if (!strcmp(name, sNative[i].name)) {
            sPendingRg = sNative[i].rg;
            return true;
        }
    }
    return false;
}

#include "play_scenarios.inc.c"

static void TakePlayRequest(const char* request) {
    const bool isStarted = StartPlayScenario(request);
    CVarSetString(AUTO_CVAR("Text"), "");
    CVarSetInteger(AUTO_CVAR("Icon"), NO_MESSAGE_ICON);
    CVarSetString(AUTO_CVAR("State"), isStarted ? "playing" : "unknown");
    sAutoPhase = isStarted ? AUTO_PLAYING : AUTO_IDLE;
    CVarSetString(AUTO_CVAR("Request"), "");
}

static void AdvancePlayScenario(void) {
    if (HasPlayFinished()) {
        CVarSetString(AUTO_CVAR("State"), "shown");
        sAutoPhase = AUTO_SHOWN;
    }
}

// The state is published before the request is cleared: a cleared request tells the driver the state is fresh.
static void TakeAutoRequest(void) {
    const char* request = CVarGetString(AUTO_CVAR("Request"), "");
    if (request[0] == '\0' || HasPendingDrop()) {
        return;
    }
    if (IsPlayRequest(request)) {
        TakePlayRequest(request);
        return;
    }
    const bool isKnown = QueueByName(request);
    CVarSetString(AUTO_CVAR("State"), isKnown ? "queued" : "unknown");
    sAutoPhase = isKnown ? AUTO_QUEUED : AUTO_IDLE;
    sAutoFrames = 0;
    CVarSetString(AUTO_CVAR("Request"), "");
}

static bool IsTextboxPrinted(PlayState* play) {
    const u8 state = Message_GetState(&play->msgCtx);
    return state != TEXT_STATE_NONE && state != TEXT_STATE_CLOSING && state != TEXT_STATE_DONE_FADING;
}

static void ClosePrintedTextbox(PlayState* play) {
    if (IsTextboxPrinted(play)) {
        Message_CloseTextbox(play);
    }
}

static void AbandonBlockedDrop(PlayState* play) {
    if (++sAutoFrames <= AUTO_PICKUP_TIMEOUT) {
        return;
    }
    ClearPendingDrop();
    char reason[AUTO_TEXT_SIZE];
    snprintf(reason, sizeof(reason), "pause %d, message %d, transition %d, cutscene %d", play->pauseCtx.state,
             play->msgCtx.msgMode, play->transitionTrigger, Player_InBlockingCsMode(play, GET_PLAYER(play)));
    CVarSetString(AUTO_CVAR("Text"), reason);
    CVarSetString(AUTO_CVAR("State"), "blocked");
    sAutoPhase = AUTO_IDLE;
}

static void AwaitAutoSpawn(PlayState* play) {
    if (CanSpawnDrop(play)) {
        return;
    }
    ClosePrintedTextbox(play);
    AbandonBlockedDrop(play);
}

static void ReportAutoSpawn(bool isSpawned) {
    sAutoFrames = 0;
    sAutoTextFrames = 0;
    CVarSetString(AUTO_CVAR("Text"), "");
    CVarSetInteger(AUTO_CVAR("Icon"), NO_MESSAGE_ICON);
    CVarSetString(AUTO_CVAR("State"), isSpawned ? "spawned" : "unspawnable");
    sAutoPhase = isSpawned ? AUTO_FALLING : AUTO_IDLE;
}

static void PublishMessage(PlayState* play) {
    const MessageContext* msgCtx = &play->msgCtx;
    const u8* raw = (const u8*)msgCtx->font.msgBuf;
    const s32 length = MIN((s32)msgCtx->msgLength, MESSAGE_BUF_SIZE - 1);
    char text[AUTO_TEXT_SIZE];
    size_t used = 0;
    int32_t icon = NO_MESSAGE_ICON;
    for (s32 i = 0; i < length && raw[i] != MESSAGE_END_CODE && used + 1 < sizeof(text); ++i) {
        const u8 symbol = raw[i];
        if (symbol == MESSAGE_ITEM_ICON_CODE) {
            icon = raw[i + 1];
        }
        if (symbol == MESSAGE_NEWLINE_CODE) {
            text[used++] = ' ';
        } else if (symbol < ' ') {
            i += sControlArguments[symbol];
        } else if (symbol <= '~') {
            text[used++] = (char)symbol;
        }
    }
    text[used] = '\0';
    CVarSetString(AUTO_CVAR("Text"), text);
    CVarSetInteger(AUTO_CVAR("Icon"), icon);
}

static void KillTesterDrops(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_MISC].head; actor != NULL; actor = actor->next) {
        const bool isTesterDrop = actor->id == ACTOR_EN_ITEM00 && (actor->params == ITEM00_SOH_CUSTOM ||
                                                                   actor->params == ITEM00_SOH_GIVE_ITEM_ENTRY_GI);
        if (isTesterDrop) {
            Actor_Kill(actor);
        }
    }
}

static void WatchAutoPickup(PlayState* play) {
    ++sAutoFrames;
    if (play->msgCtx.msgMode == 0) {
        if (sAutoFrames > AUTO_PICKUP_TIMEOUT) {
            CVarSetString(AUTO_CVAR("State"), "silent");
            sAutoPhase = AUTO_SHOWN;
        }
        return;
    }
    ++sAutoTextFrames;
    const bool isSettled = IsTextboxPrinted(play) && sAutoTextFrames >= AUTO_SETTLE_FRAMES;
    if (!isSettled && sAutoTextFrames < AUTO_PRINT_TIMEOUT) {
        return;
    }
    KillTesterDrops(play);
    PublishMessage(play);
    CVarSetString(AUTO_CVAR("State"), "shown");
    sAutoPhase = AUTO_SHOWN;
}

static void ReleaseAutoPickup(PlayState* play) {
    if (!CVarGetInteger(AUTO_CVAR("Release"), 0)) {
        return;
    }
    CVarSetInteger(AUTO_CVAR("Release"), 0);
    sIsPlaying = false;
    if (play->msgCtx.msgMode != 0) {
        Message_CloseTextbox(play);
    } else {
        KillTesterDrops(play);
    }
    sAutoFrames = 0;
    sAutoPhase = AUTO_CLOSING;
}

static void FinishAutoPickup(PlayState* play) {
    ClosePrintedTextbox(play);
    const bool isFree = CanSpawnDrop(play);
    if (!isFree && ++sAutoFrames <= AUTO_PICKUP_TIMEOUT) {
        return;
    }
    CVarSetString(AUTO_CVAR("State"), isFree ? "done" : "stuck");
    sAutoPhase = AUTO_IDLE;
}

static void AdvanceAuto(PlayState* play) {
    switch (sAutoPhase) {
        case AUTO_IDLE:
            TakeAutoRequest();
            break;
        case AUTO_QUEUED:
            AwaitAutoSpawn(play);
            break;
        case AUTO_FALLING:
            WatchAutoPickup(play);
            break;
        case AUTO_PLAYING:
            AdvancePlayScenario();
            break;
        case AUTO_SHOWN:
            ReleaseAutoPickup(play);
            break;
        case AUTO_CLOSING:
            FinishAutoPickup(play);
            break;
        default:
            sAutoPhase = AUTO_IDLE;
            break;
    }
}

#define SONG_CVAR(name) "gMods.ItemDropTester.Song." name
#define SONG_ANSWER_DELAY 20
#define SONG_TIMEOUT 600
#define OWL_MAP_CSACTION 1
typedef enum {
    SOARING_IDLE,
    SOARING_PLAYING,
    SOARING_AWAIT_MAP,
    SOARING_ON_MAP,
    SOARING_CONFIRMING,
    SOARING_AWAIT_TEXT,
    SOARING_CLOSING,
} SoaringTest;
static const u8 sSoaringNotes[] = { 0x05, 0x0B, 0x0E, 0x05, 0x0B, 0x0E };
static SoaringTest sSoaringTest;
static s32 sSoaringStep;
static s32 sSoaringFrames;
static char sSoaringAnswer[16];

static void PublishSoaringState(const char* state) {
    CVarSetString(SONG_CVAR("State"), state);
    sSoaringFrames = 0;
}

static void FinishSoaringTest(const char* state) {
    sSoaringTest = SOARING_IDLE;
    PublishSoaringState(state);
}

static bool IsOwlMapOpen(PlayState* play) {
    return GET_PLAYER(play)->csAction == OWL_MAP_CSACTION;
}

static bool PressAfterDelay(PlayState* play, u16 button) {
    if (++sSoaringFrames <= SONG_ANSWER_DELAY) {
        return false;
    }
    play->state.input[0].press.button |= button;
    play->state.input[0].cur.button |= button;
    sSoaringFrames = 0;
    return true;
}

static void PlayNextSoaringStep(void) {
    const s32 step = sSoaringStep++;
    const u8 note = step % 2 == 0 ? sSoaringNotes[step / 2] : OCARINA_PITCH_NONE;
    GameInteractor_ExecuteOnOcarinaNote(note, 0.0f, 0);
    if (sSoaringStep < 2 * (s32)ARRAY_COUNT(sSoaringNotes)) {
        return;
    }
    sSoaringTest = strcmp(sSoaringAnswer, "close") == 0 ? SOARING_AWAIT_TEXT : SOARING_AWAIT_MAP;
    PublishSoaringState("played");
}

static void TakeSoaringRequest(PlayState* play) {
    const char* request = CVarGetString(SONG_CVAR("Request"), "");
    if (strncmp(request, "soaring:", 8) != 0 || !CanSpawnDrop(play)) {
        return;
    }
    snprintf(sSoaringAnswer, sizeof(sSoaringAnswer), "%s", request + 8);
    sApi->SetCustomItemOwned("mm.song.soaring", true);
    sSoaringStep = 0;
    sSoaringTest = SOARING_PLAYING;
    PublishSoaringState("playing");
    CVarSetString(SONG_CVAR("Request"), "");
}

static void AwaitOwlMap(PlayState* play) {
    if (IsOwlMapOpen(play)) {
        sSoaringTest = SOARING_ON_MAP;
        PublishSoaringState("map");
    } else if (++sSoaringFrames > SONG_TIMEOUT) {
        FinishSoaringTest("no map");
    }
}

static void AnswerOwlMap(PlayState* play) {
    const bool isCancel = strcmp(sSoaringAnswer, "cancel") == 0;
    if (!PressAfterDelay(play, isCancel ? BTN_B : BTN_A)) {
        return;
    }
    sSoaringTest = isCancel ? SOARING_CLOSING : SOARING_CONFIRMING;
}

static void ConfirmSoar(PlayState* play) {
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE) {
        if (++sSoaringFrames > SONG_TIMEOUT) {
            FinishSoaringTest("never asked");
        }
        return;
    }
    play->msgCtx.choiceIndex = 0;
    if (PressAfterDelay(play, BTN_A)) {
        FinishSoaringTest("answered");
    }
}

static void AwaitNoStatueText(PlayState* play) {
    if (play->msgCtx.msgMode != MSGMODE_NONE && !IsOwlMapOpen(play)) {
        sSoaringTest = SOARING_CLOSING;
        PublishSoaringState("refused");
    } else if (++sSoaringFrames > SONG_TIMEOUT) {
        FinishSoaringTest("no text");
    }
}

static void CloseSoaring(PlayState* play) {
    if (!IsOwlMapOpen(play) && play->msgCtx.msgMode == MSGMODE_NONE) {
        FinishSoaringTest("closed");
        return;
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        PressAfterDelay(play, BTN_A);
    }
}

static void AdvanceSongTest(void) {
    PlayState* play = gPlayState;
    if (play == NULL || gGameState != &play->state || !IS_RANDO || GET_PLAYER(play) == NULL) {
        return;
    }
    switch (sSoaringTest) {
        case SOARING_IDLE:
            TakeSoaringRequest(play);
            break;
        case SOARING_PLAYING:
            PlayNextSoaringStep();
            break;
        case SOARING_AWAIT_MAP:
            AwaitOwlMap(play);
            break;
        case SOARING_ON_MAP:
            AnswerOwlMap(play);
            break;
        case SOARING_CONFIRMING:
            ConfirmSoar(play);
            break;
        case SOARING_AWAIT_TEXT:
            AwaitNoStatueText(play);
            break;
        case SOARING_CLOSING:
            CloseSoaring(play);
            break;
        default:
            sSoaringTest = SOARING_IDLE;
            break;
    }
}

static void Tick(void) {
    if (sChecklistDirty) {
        CVarSave();
        sChecklistDirty = false;
    }
    PlayState* play = gPlayState;
    if (play == NULL || !IS_RANDO) {
        return;
    }
    AdvanceAuto(play);
    if (!HasPendingDrop() || !CanSpawnDrop(play)) {
        return;
    }
    const bool isSpawned = SpawnPendingDrop(play) != NULL;
    sStatus = isSpawned ? "Pickup spawned. Collect it to test the real rando acquisition."
                        : "No collectible entry for this reward, or actor allocation failed.";
    if (sAutoPhase == AUTO_QUEUED) {
        ReportAutoSpawn(isSpawned);
    }
}
static bool Matches(const char* text) {
    if (!sFilter[0])
        return true;
    if (text == NULL)
        return false;
    for (; *text; ++text) {
        const char* a = text;
        const char* b = sFilter;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (!*b)
            return true;
    }
    return false;
}
static void QueueCustom(const char* key) {
    snprintf(sPending, sizeof(sPending), "%s", key);
    snprintf(sSelected, sizeof(sSelected), "%s", key);
    sPendingRg = sSelectedRg = -1;
    sStatus = "Queued. Close the menu and resume gameplay.";
}
static bool HasVariants(const char* key) {
    for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
        const SOHCustomItemDefinition* child = sApi->GetCustomItemAt(i);
        const SOHCustomPickupVariant* v = child ? sApi->GetPickupVariant(child->key) : NULL;
        if (v != NULL && !strcmp(v->itemKey, key))
            return true;
    }
    return false;
}
static void CustomRow(const SOHCustomItemDefinition* item) {
    if (item == NULL || sApi->GetPickupVariant(item->key) != NULL)
        return;
    bool match = Matches(item->key);
    bool variants = HasVariants(item->key);
    if (!match && variants)
        for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
            const SOHCustomItemDefinition* child = sApi->GetCustomItemAt(i);
            const SOHCustomPickupVariant* v = child ? sApi->GetPickupVariant(child->key) : NULL;
            if (v && !strcmp(v->itemKey, item->key) && (Matches(v->variantKey) || Matches(v->key)))
                match = true;
        }
    if (!match)
        return;
    const int32_t rg = sApi->GetRandoItem(item->key);
    if (rg <= 0 && !variants)
        return;
    ModUI_PushID(item->key);
    if (ModUI_Selectable(item->key, !strcmp(sSelected, item->key), 0, 0, 0)) {
        snprintf(sSelected, sizeof(sSelected), "%s", item->key);
        sSelectedRg = -1;
    }
    if (rg > 0 && ModUI_Button("Drop", 0, 0))
        QueueCustom(item->key);
    for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i) {
        const SOHCustomItemDefinition* child = sApi->GetCustomItemAt(i);
        const SOHCustomPickupVariant* v = child ? sApi->GetPickupVariant(child->key) : NULL;
        if (v == NULL || strcmp(v->itemKey, item->key) || sApi->GetRandoItem(v->key) <= 0)
            continue;
        ModUI_PushID(v->key);
        if (ModUI_Button(v->variantKey, 0, 0))
            QueueCustom(v->key);
        ModUI_PopID();
        ModUI_SameLine(0, -1);
    }
    if (variants)
        ModUI_NewLine();
    ModUI_Separator();
    ModUI_PopID();
}
static void DrawChecklist(const char* key) {
    static const char* const labels[] = { "Pickup",    "Icon",  "Acquisition model", "Held model", "Use",
                                          "Save/load", "Anchor" };
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    ModUI_Text("Checklist (manual results, saved locally):");
    for (int i = 0; i < 7; ++i) {
        char cvar[KEY_SIZE + 64];
        snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", key, fields[i]);
        bool passed = CVarGetInteger(cvar, 0) != 0;
        if (ModUI_Checkbox(labels[i], &passed)) {
            CVarSetInteger(cvar, passed);
            sChecklistDirty = true;
        }
    }
}
static void CopyChecklist(void) {
    const uint32_t count = sApi->GetCustomItemCount();
    if (count > 10000)
        return;
    const size_t capacity = ((size_t)count + sizeof(sNative) / sizeof(sNative[0])) * (KEY_SIZE + 160) + 256;
    char* text = malloc(capacity);
    if (text == NULL)
        return;
    size_t used = snprintf(text, capacity,
                           "# Ship/OoT rando item checklist\n\n| Item / subtype | Pickup | Icon | Acquisition | Held | "
                           "Use | Save/load | Anchor |\n|---|---|---|---|---|---|---|---|\n");
    static const char* const fields[] = { "pickup", "icon", "acquisition", "held", "use", "save", "anchor" };
    for (uint32_t i = 0; i < count; ++i) {
        const SOHCustomItemDefinition* item = sApi->GetCustomItemAt(i);
        if (item == NULL || item->key == NULL || strlen(item->key) >= KEY_SIZE)
            continue;
        used += snprintf(text + used, capacity - used, "| %s |", item->key);
        for (int f = 0; f < 7; ++f) {
            char cvar[KEY_SIZE + 64];
            snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", item->key, fields[f]);
            used += snprintf(text + used, capacity - used, " %s |", CVarGetInteger(cvar, 0) ? "OK" : "pending");
        }
        used += snprintf(text + used, capacity - used, "\n");
    }
    for (size_t i = 0; i < sizeof(sNative) / sizeof(sNative[0]); ++i) {
        used += snprintf(text + used, capacity - used, "| %s |", sNative[i].name);
        for (int f = 0; f < 7; ++f) {
            char cvar[KEY_SIZE + 64];
            snprintf(cvar, sizeof(cvar), "gMods.ItemDropTester.Check.%s.%s", sNative[i].key, fields[f]);
            used += snprintf(text + used, capacity - used, " %s |", CVarGetInteger(cvar, 0) ? "OK" : "pending");
        }
        used += snprintf(text + used, capacity - used, "\n");
    }
    ModUI_SetClipboardText(text);
    free(text);
    sStatus = "Markdown checklist copied to clipboard.";
}
static void DrawMenu(void) {
    static const char* const modes[] = { "Custom items and variants", "Vanilla / built-in rando rewards" };
    ModUI_TextWrapped("Real rando pickups above Link. Progressives use your current inventory. Ownership is granted "
                      "only when collected; no seed checks are marked collected.");
    ModUI_TextWrapped(sStatus);
    if (ModUI_Button("Copy full checklist (Markdown)", 0, 0))
        CopyChecklist();
    ModUI_Combo("Catalog", &sCatalog, modes, 2);
    ModUI_InputTextWithHint("Filter", "Name, key or subtype", sFilter, sizeof(sFilter), 0);
    if (ModUI_Button("Cancel queued drop", 0, 0)) {
        sPending[0] = '\0';
        sPendingRg = -1;
    }
    if (gPlayState == NULL || !IS_RANDO)
        ModUI_TextWrapped("Load a randomizer save to test the real rando acquisition flow.");
    bool visible = ModUI_BeginChild("DropCatalog", 0, 320, true, 0);
    if (visible) {
        ModUI_BeginDisabled(gPlayState == NULL || !IS_RANDO || sPending[0] || sPendingRg >= 0);
        if (sCatalog == 0) {
            for (uint32_t i = 0; i < sApi->GetCustomItemCount(); ++i)
                CustomRow(sApi->GetCustomItemAt(i));
        } else {
            for (size_t i = 0; i < sizeof(sNative) / sizeof(sNative[0]); ++i) {
                const NativeItem* item = &sNative[i];
                if (!Matches(item->name) && !Matches(item->key))
                    continue;
                ModUI_PushID(item->key);
                if (ModUI_Selectable(item->name, sSelectedRg == item->rg, 0, 0, 0)) {
                    sSelectedRg = item->rg;
                    snprintf(sSelected, sizeof(sSelected), "%s", item->key);
                }
                if (ModUI_Button("Drop", 0, 0)) {
                    sPendingRg = sSelectedRg = item->rg;
                    snprintf(sSelected, sizeof(sSelected), "%s", item->key);
                    sStatus = "Queued. Close the menu and resume gameplay.";
                }
                const char* replacement = VanillaItems_GetCustomKeyForRandoItem(item->rg);
                if (replacement != NULL) {
                    ModUI_Text("Active mod replacement:");
                    ModUI_Text(replacement);
                }
                ModUI_PopID();
            }
        }
        ModUI_EndDisabled();
    }
    ModUI_EndChild();
    if (sSelected[0]) {
        ModUI_Text(sSelected);
        DrawChecklist(sSelected);
    }
    const SOHCustomItemDefinition* selected = sSelectedRg < 0 ? sApi->FindCustomItem(sSelected) : NULL;
    if (selected != NULL) {
        const SOHCustomPickupVariant* subtype = sApi->GetPickupVariant(selected->key);
        if (subtype != NULL) {
            ModUI_Text(subtype->itemKey);
            ModUI_Text(subtype->variantKey);
        }
        ModUI_Text("Icon:");
        ModUI_Text(selected->iconPath ? selected->iconPath : "(none)");
        ModUI_Text("Acquisition model:");
        ModUI_Text(selected->getItemModelPath        ? selected->getItemModelPath
                   : selected->getItemEntry.drawFunc ? "Custom draw callback"
                                                     : "Native draw ID / icon fallback");
    }
}
SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}
SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, GetPickupVariant))
        return;
    sApi->RegisterMenuSidebar("Mods", "Item Drop Tester", "gMods.Menu.Page", 1);
    SOHModMenuWidget widget = { 0 };
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Item Drop Tester";
    widget.type = SOH_MOD_MENU_CUSTOM;
    widget.label = "Item Drop Tester";
    widget.onChange = DrawMenu;
    sApi->RegisterMenuWidget(&widget);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, Tick);
    SOH_REGISTER_HOOK(sApi, OnGameStateMainStart, PressPlayButtons);
    SOH_REGISTER_HOOK(sApi, OnGameStateMainStart, AdvanceSongTest);
}
