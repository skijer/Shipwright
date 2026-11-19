
#include <stdbool.h>
#include <stdint.h>

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ModApi/ModApi.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define OWNER "mm_trap_types"
#define TRAP_SECTION "Enhancements"
#define TRAP_SIDEBAR "Extra Modes"
#define LAST_HOST_TRAP_WIDGET "Teleport Traps"
#define TRAP_HEADER "Majora's Mask Traps:"

#define TRAP_DELAY_FRAMES 3
#define JINX_FRAMES 1200
#define TIME_SKIP 4000
#define WALLET_DROP_HEIGHT 100.0f
#define WALLET_SCATTER_SPEED 5.0f
#define WALLET_DESPAWN_FRAMES 600

typedef struct {
    const char* label;
    const char* cvar;
    const char* tooltip;
    void (*apply)(PlayState* play);
} MmTrap;

typedef struct {
    int16_t value;
    int16_t drop;
} RupeeDrop;

static const RupeeDrop sRupeeDrops[] = {
    { 20, ITEM00_RUPEE_RED },
    { 5, ITEM00_RUPEE_BLUE },
    { 1, ITEM00_RUPEE_GREEN },
};

static const char* const sRequiredHooks[] = { "OnTrapRoll", "OnPlayerUpdate", "OnLoadGame" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
static const MmTrap* sRolledTrap;
static const MmTrap* sPendingTrap;
static int32_t sDelayFrames;
static int32_t sJinxFrames;
static bool sIsJinxBlocking;

static bool IsSwordOnB(void) {
    switch (gSaveContext.equips.buttonItems[0]) {
        case ITEM_SWORD_KOKIRI:
        case ITEM_SWORD_MASTER:
        case ITEM_SWORD_BGS:
        case ITEM_SWORD_KNIFE:
            return true;
        default:
            return false;
    }
}

static void SetJinxBlock(bool isBlocking) {
    if (isBlocking == sIsJinxBlocking) {
        return;
    }
    sIsJinxBlocking = isBlocking;
    if (isBlocking) {
        sApi->BlockPlayerInput(OWNER, BTN_B, false);
    } else {
        sApi->ReleasePlayerInput(OWNER);
    }
}

static void UpdateJinx(void) {
    if (sJinxFrames > 0) {
        sJinxFrames--;
    }
    SetJinxBlock(sJinxFrames > 0 && IsSwordOnB());
}

static void StartJinx(PlayState* play) {
    Audio_PlayActorSound2(&GET_PLAYER(play)->actor, NA_SE_EN_BUBLE_BITE);
    sJinxFrames = JINX_FRAMES;
}

static const RupeeDrop* GetLargestRupeeWithin(int16_t rupees) {
    for (uint32_t i = 0; i < ARRAY_COUNT(sRupeeDrops) - 1; i++) {
        if (rupees >= sRupeeDrops[i].value) {
            return &sRupeeDrops[i];
        }
    }
    return &sRupeeDrops[ARRAY_COUNT(sRupeeDrops) - 1];
}

static void ScatterWallet(PlayState* play) {
    Vec3f dropPos = GET_PLAYER(play)->actor.world.pos;
    dropPos.y += WALLET_DROP_HEIGHT;
    int16_t rupees = gSaveContext.rupees;
    while (rupees > 0) {
        const RupeeDrop* drop = GetLargestRupeeWithin(rupees);
        Rupees_ChangeBy(-drop->value);
        rupees -= drop->value;
        EnItem00* rupee = Item_DropCollectible(play, &dropPos, drop->drop);
        if (rupee == NULL) {
            continue;
        }
        rupee->actor.speedXZ = Rand_CenteredFloat(WALLET_SCATTER_SPEED);
        // unk_15A is EnItem00's despawn timer.
        rupee->unk_15A = WALLET_DESPAWN_FRAMES;
    }
}

static void SpawnLikeLike(PlayState* play) {
    Vec3f pos = GET_PLAYER(play)->actor.world.pos;
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_RR, pos.x, pos.y, pos.z, 0, 0, 0, 0);
}

static void SkipTime(PlayState* play) {
    gSaveContext.dayTime += TIME_SKIP;
    TransitionFade_SetColor(&play->transitionFade, 0x000000);
    iREG(50) = -1;
    Sfx_PlaySfxCentered(NA_SE_SY_WHITE_OUT_T);
}

static const MmTrap sTraps[] = {
    { "Jinx Traps", "gEnhancements.ExtraTraps.Jinx", "Link cannot use his sword for a minute.", StartJinx },
    { "Wallet Traps", "gEnhancements.ExtraTraps.Wallet", "Link's rupees scatter around him.", ScatterWallet },
    { "Like Like Traps", "gEnhancements.ExtraTraps.Enemy", "A Like Like spawns on top of Link.", SpawnLikeLike },
    { "Time Traps", "gEnhancements.ExtraTraps.Time", "Time jumps ahead about an hour and a half.", SkipTime },
};

static void QueueRolledTrap(void) {
    sPendingTrap = sRolledTrap;
    sDelayFrames = TRAP_DELAY_FRAMES;
}

static void OfferTrapTypes(uint32_t* candidateCount, void (**effect)(void)) {
    const MmTrap* enabledTraps[ARRAY_COUNT(sTraps)];
    uint32_t enabledCount = 0;
    for (uint32_t i = 0; i < ARRAY_COUNT(sTraps); i++) {
        if (CVarGetInteger(sTraps[i].cvar, 0)) {
            enabledTraps[enabledCount] = &sTraps[i];
            enabledCount++;
        }
    }
    if (enabledCount == 0) {
        return;
    }
    *candidateCount += enabledCount;
    uint32_t pick = Rand_Next() % *candidateCount;
    if (pick >= enabledCount) {
        return;
    }
    sRolledTrap = enabledTraps[pick];
    *effect = QueueRolledTrap;
}

static void UpdateTraps(void) {
    UpdateJinx();
    if (sPendingTrap == NULL) {
        return;
    }
    if (sDelayFrames > 0) {
        sDelayFrames--;
        return;
    }
    const MmTrap* trap = sPendingTrap;
    sPendingTrap = NULL;
    trap->apply(gPlayState);
}

static void ResetTraps(int32_t fileNum) {
    sPendingTrap = NULL;
    sJinxFrames = 0;
    SetJinxBlock(false);
}

static void RegisterTrapWidget(SOHModMenuWidgetType type, const char* label, const char* cvar, const char* tooltip,
                               const char* anchor) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };
    widget.section = TRAP_SECTION;
    widget.sidebar = TRAP_SIDEBAR;
    widget.type = type;
    widget.label = label;
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    sApi->RegisterMenuWidgetAt(&widget, anchor, true);
}

static void RegisterMenu(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterMenuWidgetAt)) {
        return;
    }
    RegisterTrapWidget(SOH_MOD_MENU_TEXT, TRAP_HEADER, NULL, NULL, LAST_HOST_TRAP_WIDGET);
    const char* anchor = TRAP_HEADER;
    for (uint32_t i = 0; i < ARRAY_COUNT(sTraps); i++) {
        RegisterTrapWidget(SOH_MOD_MENU_CHECKBOX, sTraps[i].label, sTraps[i].cvar, sTraps[i].tooltip, anchor);
        anchor = sTraps[i].label;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    RegisterMenu();
    sApi->RegisterHookByName("OnTrapRoll", (void*)OfferTrapTypes);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateTraps);
    SOH_REGISTER_HOOK(sApi, OnLoadGame, ResetTraps);
}
