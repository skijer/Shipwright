// Romani's Mask calls a cow the way Epona's Song does; wearing it at Link's house cow pours Chateau Romani.
#define CHATEAU_KEY "nei.chateau_romani"

const GetItemEntry* CustomItemRegistry_FindGetItemEntry(const char* key);
bool CustomItemRegistry_PrepareGetItem(Actor* actor, PlayState* play, GetItemEntry* entry);

static u8 sChateauCount;
static bool sChateauMagicActive;
static bool sChateauOwnsMagic;
static bool sCowCalled;
static u32 sCowCallFrame;
static u16 sCowIntroText = SOH_MOD_MESSAGE_INVALID;
static u16 sCowRefillText = SOH_MOD_MESSAGE_INVALID;

static bool IsHouseCowChateau(PlayState* play) {
    return play != NULL && play->sceneNum == SCENE_LINKS_HOUSE && IsWearing(MASK_ROMANI);
}

static void StoreChateauMagic(void) {
    sApi->StorageSet("nei.mm_wearables", "chateau_magic_owned", &sChateauOwnsMagic, sizeof(sChateauOwnsMagic));
}

static void EndChateauMagic(void) {
    if (sChateauOwnsMagic) {
        Flags_UnsetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER);
    }
    sChateauOwnsMagic = false;
    sChateauMagicActive = false;
    StoreChateauMagic();
}

static void ActivateChateauMagic(PlayState* play) {
    Health_ChangeBy(play, 0x140);
    if (Flags_GetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER) || CVarGetInteger("gCheats.InfiniteMagic", 0)) {
        return;
    }
    sChateauOwnsMagic = true;
    sChateauMagicActive = true;
    Flags_SetRandomizerInf(RAND_INF_HAS_INFINITE_MAGIC_METER);
    gSaveContext.magic = gSaveContext.magicCapacity;
    StoreChateauMagic();
}

static void TickChateauMagic(void) {
    if (!sChateauMagicActive) {
        return;
    }
    if (gSaveContext.health <= 0 || (gPlayState != NULL && gPlayState->gameOverCtx.state != GAMEOVER_INACTIVE)) {
        EndChateauMagic();
    }
}

static void RegisterCowMessages(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterMessage)) {
        return;
    }
    SOHModMessage message = { 0 };
    message.structSize = sizeof(message);
    message.textboxPosition = 2;
    message.autoFormat = true;
    message.key = "nei.romani.cow_intro";
    message.text = "This cow makes Chateau Romani! Have you brought an empty bottle?" "\x0B";
    sCowIntroText = sApi->RegisterMessage(&message);
    message.key = "nei.romani.cow_refill";
    message.text = "Let me fill your bottle with fresh Chateau Romani!" "\x0B";
    sCowRefillText = sApi->RegisterMessage(&message);
}

static void ReplaceCowText(u16* textId, bool* loadFromTable) {
    if (!IsHouseCowChateau(gPlayState)) {
        return;
    }
    u16 replacement = SOH_MOD_MESSAGE_INVALID;
    if (*textId == 0x2006) {
        replacement = sCowIntroText;
    } else if (*textId == 0x2007) {
        replacement = sCowRefillText;
    }
    if (replacement != SOH_MOD_MESSAGE_INVALID) {
        *textId = replacement;
        *loadFromTable = true;
    }
}

static bool CanCallCow(PlayState* play) {
    Player* player = GET_PLAYER(play);
    const u32 busy = PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE;

    return play->msgCtx.msgMode == MSGMODE_NONE && !(player->stateFlags1 & busy) && DREG(53) == 0;
}

// DREG(53) is what En_Cow's idle reads after Epona's Song. Every cow reads it once in the next frame;
// a call still standing after that found no idle cow, and would restart a conversation later.
static void WithdrawCowCall(void) {
    if (!sCowCalled || gPlayState == NULL || gPlayState->gameplayFrames - sCowCallFrame < 2) {
        return;
    }
    DREG(53) = 0;
    sCowCalled = false;
}

static void CallCow(Actor* cow, PlayState* play) {
    if (!IsWearing(MASK_ROMANI) || cow->xzDistToPlayer >= COW_TALK_RADIUS || !CanCallCow(play)) {
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return;
    }
    DREG(53) = 1;
    sCowCalled = true;
    sCowCallFrame = play->gameplayFrames;
}

// En_Cow re-offers GI_MILK every frame until Link takes it; the offer it already won is swapped in place,
// because GiveItemEntryFromActor's talk-range test rejects a GI_CUSTOM entry mid-conversation.
static void OfferChateauForMilk(Actor* cow, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!IsHouseCowChateau(play) || player->interactRangeActor != cow || player->getItemId != GI_MILK) {
        return;
    }
    const GetItemEntry* chateau = CustomItemRegistry_FindGetItemEntry(CHATEAU_KEY);
    if (chateau == NULL) {
        return;
    }
    GetItemEntry entry = *chateau;
    if (CustomItemRegistry_PrepareGetItem(cow, play, &entry)) {
        player->getItemEntry = entry;
        player->getItemId = entry.getItemId;
    }
}

static void UpdateCowForRomani(void* actorRef) {
    if (gPlayState == NULL) {
        return;
    }
    CallCow((Actor*)actorRef, gPlayState);
    OfferChateauForMilk((Actor*)actorRef, gPlayState);
}

static const ALIGN_ASSET(2) char sChateauIcon[] = "__OTR__icon_item_static_yar/gItemIconChateauRomaniTex";
static const ALIGN_ASSET(2) char sChateauName[] = "__OTR__item_name_static/gItemNameChateauRomaniENGTex";

static void DrawChateauBottle(PlayState* play, GetItemEntry* entry) {
    static const ALIGN_ASSET(2) char bottle[] = "__OTR__objects/object_gi_bottle_21/gGiChateauRomaniBottleEmptyDL";
    static const ALIGN_ASSET(2) char milk[] = "__OTR__objects/object_gi_bottle_21/gGiChateauRomaniBottleDL";
    DrawCompleteMask(play, bottle, milk);
}

static void StoreChateau(void) {
    sApi->StorageSet("nei.mm_wearables", "chateau", &sChateauCount, sizeof(sChateauCount));
}

static void LoadChateau(int32_t file) {
    sChateauMagicActive = false;
    sChateauOwnsMagic = false;
    sApi->StorageGet("nei.mm_wearables", "chateau_magic_owned", &sChateauOwnsMagic, sizeof(sChateauOwnsMagic));
    if (sChateauOwnsMagic) {
        EndChateauMagic();
    }
    sCowCalled = false;
    sChateauCount = 0;
    sApi->StorageGet("nei.mm_wearables", "chateau", &sChateauCount, sizeof(sChateauCount));
    sChateauCount = MIN(sChateauCount, 1);
}

// not_enough_bottles owns which bottle holds Chateau; without it the drink waits in its own slot.
static bool FillChateauBottle(PlayState* play) {
    typedef bool (*FillFunc)(PlayState*);
    FillFunc fill = SOH_MOD_API_HAS(sApi, FindService) ? (FillFunc)sApi->FindService("nei.bottles.fill_chateau") : NULL;
    return fill != NULL && play != NULL && fill(play);
}

static void ReceiveChateau(const char* key) {
    if (!FillChateauBottle(gPlayState)) {
        sChateauCount = 1;
        StoreChateau();
    }
}

static int32_t ChateauAmmo(const char* key) {
    return sChateauCount;
}

static bool CanDrinkChateau(Player* player, PlayState* play) {
    return sChateauCount > 0 && !IsPlayerBusy(player);
}

static void DrinkChateau(PlayState* play, Player* player) {
    sChateauCount = 0;
    StoreChateau();
    ActivateChateauMagic(play);
    Player_PlaySfx(&player->actor, NA_SE_VO_LI_DRINK);
}

static void RegisterChateau(void) {
    RegisterCowMessages();
    SOH_REGISTER_HOOK(sApi, OnOpenText, ReplaceCowText);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, TickChateauMagic);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, WithdrawCowCall);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_COW, UpdateCowForRomani);
    if (SOH_MOD_API_HAS(sApi, PublishService)) {
        sApi->PublishService("nei.chateau.drink", (void*)ActivateChateauMagic);
    }
    SOHCustomItemDefinition item = Z64Items_Define(CHATEAU_KEY, sChateauIcon, sChateauName);
    Z64Items_SetButtons(&item, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&item, 0, 23, 1);
    item.getItemEntry.drawFunc = DrawChateauBottle;
    Z64Items_SetTextbox(&item, "You got %rChateau Romani%w! Link's own cow supplies this special milk. It restores "
                               "health and grants infinite magic until you fall.");
    Z64Items_SetPauseText(&item, "%rChateau Romani&%wInfinite magic until death.&Wear %rRomani's Mask%w at the cow "
                                 "in Link's house.");
    Z64Items_SetAmmo(&item, ChateauAmmo);
    Z64Items_SetCanUse(&item, CanDrinkChateau);
    Z64Items_SetAction(&item, DrinkChateau, NULL);
    item.flags |= SOH_CUSTOM_ITEM_INSTANT;
    item.onReceive = ReceiveChateau;
    Z64Items_Register(sApi, &item);
    LoadChateau(0);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, LoadChateau);
}
