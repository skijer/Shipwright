#include <unordered_map>
#include <string>
#include <cstring>
#include "soh/ModApi/ModApi.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#define GLOBAL_H
#include "overlays/actors/ovl_En_Diving_Game/z_en_diving_game.h"
bool MmWearables_IsCircusLeaderWorn(void);
bool GameInteractor_Should(GIVanillaBehavior behavior, uint32_t result, ...);
GetItemEntry Randomizer_GetItemFromKnownCheck(RandomizerCheck check, GetItemID vanilla);
RandomizerInf Randomizer_GetRandomizerInfFromCheck(RandomizerCheck check);
}
static const SOHModApi* sTaxApi;
enum CLMTextId : uint16_t {
    CLM_TEXT_SYATEKI_CHILD_FIRST = 0x9310,
    CLM_TEXT_SYATEKI_CHILD_REPEAT = 0x9311,
    CLM_TEXT_SYATEKI_ADULT_FIRST = 0x9312,
    CLM_TEXT_SYATEKI_ADULT_REPEAT = 0x9313,
    CLM_TEXT_BOWLING_FIRST = 0x9314,
    CLM_TEXT_BOWLING_REPEAT = 0x9315,
    CLM_TEXT_INGO_CHILD = 0x9316,
    CLM_TEXT_INGO_ADULT_PRETALON = 0x9317,
    CLM_TEXT_INGO_ADULT_POSTTALON = 0x9318,
    CLM_TEXT_INGO_ALREADY = 0x9319,
    CLM_TEXT_TALON_FIRST = 0x931A,
    CLM_TEXT_TALON_ASLEEP = 0x931B,
    CLM_TEXT_MALON_BUY = 0x931C,
    CLM_TEXT_MALON_BROKE = 0x931D,
    CLM_TEXT_MALON_REPEAT = 0x931E,
    CLM_TEXT_HBA_FIRST = 0x931F,
    CLM_TEXT_HBA_REPEAT = 0x9320,
    CLM_TEXT_FISHING_FIRST = 0x9321,
    CLM_TEXT_FISHING_REPEAT = 0x9322,
    CLM_TEXT_TAKARA_FIRST = 0x9323,
    CLM_TEXT_TAKARA_REPEAT = 0x9324,
    CLM_TEXT_DIVING_FIRST = 0x9325,
    CLM_TEXT_DIVING_REPEAT = 0x9326,
};

#define GE1_TYPE_HORSEBACK_ARCHERY 0x45

#define CLM_BRIBE_SYATEKI_CHILD 5
#define CLM_BRIBE_SYATEKI_ADULT 10
#define CLM_BRIBE_BOWLING 20
#define CLM_BRIBE_HBA 10
#define CLM_BRIBE_FISHING 5
#define CLM_BRIBE_TAKARA 20
#define CLM_BRIBE_DIVING 15
#define CLM_MALON_COW_PRICE 100

static bool CLM_IsWorn() {
    return MmWearables_IsCircusLeaderWorn();
}

enum class CLMPhase : uint8_t {
    TextShowing,
    WaitingForClose,
    TextClosed,
    RewardOffered,
    Done,
};

typedef void (*ActorUpdateFunc)(Actor*, PlayState*);

struct CLMState {
    CLMPhase phase = CLMPhase::TextShowing;
    int16_t actorId = 0;
    s32 getItemId = 0;
    s16 bribeRupees = 0;
    bool firstTime = false;
    bool isChild = false;
    ActorUpdateFunc savedUpdate = nullptr;
};

static std::unordered_map<Actor*, CLMState> gStates;

static std::unordered_map<Actor*, EnDivingGameActionFunc> gDivingSafeActionFunc;

static RandomizerCheck CLM_ResolveRandoCheck(const CLMState& s) {
    switch (s.actorId) {
        case ACTOR_EN_SYATEKI_MAN:
            return s.isChild ? RC_MARKET_SHOOTING_GALLERY_REWARD : RC_KAK_SHOOTING_GALLERY_REWARD;
        case ACTOR_EN_BOM_BOWL_MAN:
            return Flags_GetItemGetInf(ITEMGETINF_11) ? RC_MARKET_BOMBCHU_BOWLING_SECOND_PRIZE
                                                      : RC_MARKET_BOMBCHU_BOWLING_FIRST_PRIZE;
        case ACTOR_EN_TA:
            return RC_LLR_TALONS_CHICKENS;
        case ACTOR_EN_GE1:
            return Flags_GetInfTable(INFTABLE_190) ? RC_GF_HBA_1500_POINTS : RC_GF_HBA_1000_POINTS;
        case ACTOR_FISHING:
            return s.isChild ? RC_LH_CHILD_FISHING : RC_LH_ADULT_FISHING;
        case ACTOR_EN_TAKARA_MAN:
            return RC_MARKET_TREASURE_CHEST_GAME_REWARD;
        case ACTOR_EN_DIVING_GAME:
            return RC_ZD_DIVING_MINIGAME;
        default:
            return RC_UNKNOWN_CHECK;
    }
}

static bool CLM_TryDirectRandoDelivery(Actor* actor, PlayState* play, CLMState& s) {
    if (!IS_RANDO)
        return false;

    RandomizerCheck rc = CLM_ResolveRandoCheck(s);
    if (rc == RC_UNKNOWN_CHECK)
        return false;

    RandomizerInf flag = Randomizer_GetRandomizerInfFromCheck(rc);
    if (Flags_GetRandomizerInf(flag))
        return false;
    GetItemEntry entry = Randomizer_GetItemFromKnownCheck(rc, (GetItemID)s.getItemId);
    GiveItemEntryFromActor(actor, play, entry, 2000.0f, 1000.0f);
    return true;
}

struct CLMAdapter {
    int16_t actorId;
    bool (*shouldIntercept)(Actor* actor);
    uint16_t (*resolveTextId)(Actor* actor, CLMState& outState);
    bool (*grantReward)(Actor* actor, PlayState* play, CLMState& state);
    bool passVanillaUpdate;
};

static bool Syateki_ShouldIntercept(Actor* actor) {
    return true;
}

static uint16_t Syateki_ResolveTextId(Actor* actor, CLMState& s) {
    bool isChild = !LINK_IS_ADULT;
    bool already = isChild ? Flags_GetItemGetInf(ITEMGETINF_0D) : Flags_GetItemGetInf(ITEMGETINF_0E);
    s.isChild = isChild;
    s.firstTime = !already;
    s.bribeRupees = isChild ? CLM_BRIBE_SYATEKI_CHILD : CLM_BRIBE_SYATEKI_ADULT;

    if (isChild) {
        if (CUR_UPG_VALUE(UPG_BULLET_BAG) == 1)
            s.getItemId = GI_BULLET_BAG_40;
        else if (CUR_UPG_VALUE(UPG_BULLET_BAG) > 1)
            s.getItemId = GI_BULLET_BAG_50;
        else
            s.getItemId = GI_RUPEE_PURPLE;
        return already ? CLM_TEXT_SYATEKI_CHILD_REPEAT : CLM_TEXT_SYATEKI_CHILD_FIRST;
    } else {
        switch (CUR_UPG_VALUE(UPG_QUIVER)) {
            case 1:
                s.getItemId = GI_QUIVER_40;
                break;
            case 2:
                s.getItemId = GI_QUIVER_50;
                break;
            default:
                s.getItemId = GI_RUPEE_PURPLE;
                break;
        }
        return already ? CLM_TEXT_SYATEKI_ADULT_REPEAT : CLM_TEXT_SYATEKI_ADULT_FIRST;
    }
}

static bool Syateki_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime) {
        Rupees_ChangeBy(s.bribeRupees);
        return false;
    }
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static bool Bowling_ShouldIntercept(Actor* actor) {
    return true;
}

static uint16_t Bowling_ResolveTextId(Actor* actor, CLMState& s) {
    bool gotBag = Flags_GetItemGetInf(ITEMGETINF_11);
    bool gotHP = Flags_GetItemGetInf(ITEMGETINF_12);
    bool bothDone = gotBag && gotHP;
    s.firstTime = !bothDone;
    s.bribeRupees = CLM_BRIBE_BOWLING;

    if (!gotBag) {
        if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 0)
            s.getItemId = GI_BOMB_BAG_20;
        else if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 1)
            s.getItemId = GI_BOMB_BAG_30;
        else if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 2)
            s.getItemId = GI_BOMB_BAG_40;
        else
            s.getItemId = GI_RUPEE_PURPLE;
    } else if (!gotHP) {
        s.getItemId = GI_HEART_PIECE;
    } else {
        s.getItemId = 0;
    }

    return bothDone ? CLM_TEXT_BOWLING_REPEAT : CLM_TEXT_BOWLING_FIRST;
}

static bool Bowling_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime) {
        Rupees_ChangeBy(s.bribeRupees);
        return false;
    }
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static bool Ingo_ShouldIntercept(Actor* actor) {
    return true;
}

static uint16_t Ingo_ResolveTextId(Actor* actor, CLMState& s) {
    bool isChild = !LINK_IS_ADULT;
    bool alreadyHasEpona = Flags_GetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
    bool talonReturned = Flags_GetEventChkInf(EVENTCHKINF_TALON_RETURNED_FROM_CASTLE);
    s.isChild = isChild;
    s.firstTime = false;
    s.getItemId = 0;
    s.bribeRupees = 0;

    if (isChild)
        return CLM_TEXT_INGO_CHILD;
    if (alreadyHasEpona)
        return CLM_TEXT_INGO_ALREADY;

    s.firstTime = true;
    return talonReturned ? CLM_TEXT_INGO_ADULT_POSTTALON : CLM_TEXT_INGO_ADULT_PRETALON;
}

static bool Ingo_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (s.firstTime) {
        Flags_SetEventChkInf(EVENTCHKINF_EPONA_OBTAINED);
        Flags_SetEventChkInf(EVENTCHKINF_TALON_RETURNED_FROM_CASTLE);

        static const s16 kEponaExits[] = {
            ENTR_HYRULE_FIELD_11,
            ENTR_HYRULE_FIELD_12,
            ENTR_HYRULE_FIELD_13,
        };
        s16 pick = kEponaExits[Rand_Next() % 3];

        play->nextEntranceIndex = pick;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        play->transitionTrigger = TRANS_TRIGGER_START;
        gSaveContext.timerState = TIMER_STATE_OFF;
        // nextCutsceneIndex must stay below 0xFFF0 so the entrance cutscene table fires the Epona jump.
    }
    return false;
}

static bool Talon_ShouldIntercept(Actor* actor) {
    return !LINK_IS_ADULT && gPlayState->sceneNum == SCENE_LON_LON_BUILDINGS;
}

static uint16_t Talon_ResolveTextId(Actor* actor, CLMState& s) {
    bool already = Flags_GetItemGetInf(ITEMGETINF_TALON_BOTTLE);
    s.firstTime = !already;
    s.bribeRupees = 0;
    s.getItemId = already ? 0 : GI_MILK_BOTTLE;
    return already ? CLM_TEXT_TALON_ASLEEP : CLM_TEXT_TALON_FIRST;
}

static bool Talon_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime)
        return false;
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static bool Malon_ShouldIntercept(Actor* actor) {
    return LINK_IS_ADULT;
}

static uint16_t Malon_ResolveTextId(Actor* actor, CLMState& s) {
    bool already = Flags_GetEventChkInf(EVENTCHKINF_WON_COW_IN_MALONS_RACE);
    bool canAfford = gSaveContext.rupees >= CLM_MALON_COW_PRICE;
    s.bribeRupees = 0;
    if (already) {
        s.firstTime = false;
        s.getItemId = 0;
        return CLM_TEXT_MALON_REPEAT;
    }
    if (!canAfford) {
        s.firstTime = false;
        s.getItemId = 0;
        return CLM_TEXT_MALON_BROKE;
    }
    s.firstTime = true;
    s.getItemId = 0;
    return CLM_TEXT_MALON_BUY;
}

static bool Malon_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (s.firstTime) {
        Rupees_ChangeBy(-CLM_MALON_COW_PRICE);
        Flags_SetEventChkInf(EVENTCHKINF_WON_COW_IN_MALONS_RACE);
    }
    return false;
}

static bool HBA_ShouldIntercept(Actor* actor) {
    return (actor->params & 0xFF) == GE1_TYPE_HORSEBACK_ARCHERY;
}

static uint16_t HBA_ResolveTextId(Actor* actor, CLMState& s) {
    bool gotHP = Flags_GetInfTable(INFTABLE_190);
    bool gotQuiver = Flags_GetItemGetInf(ITEMGETINF_0F);
    bool bothDone = gotHP && gotQuiver;
    s.firstTime = !bothDone;
    s.bribeRupees = CLM_BRIBE_HBA;

    if (!gotHP) {
        s.getItemId = GI_HEART_PIECE;
    } else if (!gotQuiver) {
        switch (CUR_UPG_VALUE(UPG_QUIVER)) {
            case 1:
                s.getItemId = GI_QUIVER_40;
                break;
            case 2:
                s.getItemId = GI_QUIVER_50;
                break;
            default:
                s.getItemId = GI_RUPEE_PURPLE;
                break;
        }
    } else {
        s.getItemId = 0;
    }

    return bothDone ? CLM_TEXT_HBA_REPEAT : CLM_TEXT_HBA_FIRST;
}

static bool HBA_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime) {
        Rupees_ChangeBy(s.bribeRupees);
        return false;
    }
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static bool Fishing_ShouldIntercept(Actor* actor) {
    return actor->params == 1;
}

static uint16_t Fishing_ResolveTextId(Actor* actor, CLMState& s) {
    bool isChild = !LINK_IS_ADULT;
    s32 fishHS = HIGH_SCORE(HS_FISHING);
    bool already = isChild ? (fishHS & HS_FISH_PRIZE_CHILD) : (fishHS & HS_FISH_PRIZE_ADULT);
    s.isChild = isChild;
    s.firstTime = !already;
    s.bribeRupees = CLM_BRIBE_FISHING;
    s.getItemId = already ? 0 : (isChild ? GI_HEART_PIECE : GI_SCALE_GOLDEN);
    return already ? CLM_TEXT_FISHING_REPEAT : CLM_TEXT_FISHING_FIRST;
}

static bool Fishing_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime) {
        Rupees_ChangeBy(s.bribeRupees);
        return false;
    }
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static bool Takara_ShouldIntercept(Actor* actor) {
    return !LINK_IS_ADULT;
}

static uint16_t Takara_ResolveTextId(Actor* actor, CLMState& s) {
    s.firstTime = !Flags_GetItemGetInf(ITEMGETINF_1B);
    s.bribeRupees = CLM_BRIBE_TAKARA;
    s.getItemId = s.firstTime ? GI_HEART_PIECE : 0;
    return s.firstTime ? CLM_TEXT_TAKARA_FIRST : CLM_TEXT_TAKARA_REPEAT;
}

static bool Takara_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime) {
        Rupees_ChangeBy(s.bribeRupees);
        return false;
    }
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static bool Diving_ShouldIntercept(Actor* actor) {
    return true;
}

static uint16_t Diving_ResolveTextId(Actor* actor, CLMState& s) {
    bool already = Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_SILVER_SCALE);
    s.firstTime = !already;
    s.bribeRupees = CLM_BRIBE_DIVING;
    s.getItemId = already ? 0 : GI_SCALE_SILVER;
    return already ? CLM_TEXT_DIVING_REPEAT : CLM_TEXT_DIVING_FIRST;
}

static bool Diving_GrantReward(Actor* actor, PlayState* play, CLMState& s) {
    if (!s.firstTime) {
        Rupees_ChangeBy(s.bribeRupees);
        return false;
    }
    if (IS_RANDO) {
        return CLM_TryDirectRandoDelivery(actor, play, s);
    }
    Actor_OfferGetItem(actor, play, s.getItemId, 2000.0f, 1000.0f);
    return true;
}

static const CLMAdapter kAdapters[] = {
    { ACTOR_EN_SYATEKI_MAN, Syateki_ShouldIntercept, Syateki_ResolveTextId, Syateki_GrantReward, true },
    { ACTOR_EN_BOM_BOWL_MAN, Bowling_ShouldIntercept, Bowling_ResolveTextId, Bowling_GrantReward, false },
    { ACTOR_EN_IN, Ingo_ShouldIntercept, Ingo_ResolveTextId, Ingo_GrantReward, true },
    { ACTOR_EN_TA, Talon_ShouldIntercept, Talon_ResolveTextId, Talon_GrantReward, true },
    { ACTOR_EN_MA3, Malon_ShouldIntercept, Malon_ResolveTextId, Malon_GrantReward, true },
    { ACTOR_EN_GE1, HBA_ShouldIntercept, HBA_ResolveTextId, HBA_GrantReward, true },
    { ACTOR_FISHING, Fishing_ShouldIntercept, Fishing_ResolveTextId, Fishing_GrantReward, true },
    { ACTOR_EN_TAKARA_MAN, Takara_ShouldIntercept, Takara_ResolveTextId, Takara_GrantReward, true },
    // Diving Game: vanilla update must not run during CLM dialogue or its CS-locking talk branch softlocks.
    { ACTOR_EN_DIVING_GAME, Diving_ShouldIntercept, Diving_ResolveTextId, Diving_GrantReward, false },
};

static const CLMAdapter* FindAdapter(int16_t actorId) {
    for (const auto& a : kAdapters) {
        if (a.actorId == actorId)
            return &a;
    }
    return nullptr;
}

static void CLM_PostAcceptItem(Actor* actor, CLMState& s) {
    if (IS_RANDO) {
        RandomizerCheck check = CLM_ResolveRandoCheck(s);
        if (check != RC_UNKNOWN_CHECK)
            Flags_SetRandomizerInf(Randomizer_GetRandomizerInfFromCheck(check));
    }
    switch (s.actorId) {
        case ACTOR_EN_SYATEKI_MAN:
            if (s.isChild) {
                Flags_SetItemGetInf(ITEMGETINF_0D);
            } else {
                Flags_SetItemGetInf(ITEMGETINF_0E);
            }
            break;
        case ACTOR_EN_BOM_BOWL_MAN:
            if (!Flags_GetItemGetInf(ITEMGETINF_11)) {
                Flags_SetItemGetInf(ITEMGETINF_11);
            } else {
                Flags_SetItemGetInf(ITEMGETINF_12);
            }
            break;
        case ACTOR_EN_TA:
            Flags_SetItemGetInf(ITEMGETINF_TALON_BOTTLE);
            break;
        case ACTOR_EN_MA3:
            Flags_SetEventChkInf(EVENTCHKINF_WON_COW_IN_MALONS_RACE);
            break;
        case ACTOR_EN_GE1:
            if (!Flags_GetInfTable(INFTABLE_190)) {
                Flags_SetInfTable(INFTABLE_190);
            } else {
                Flags_SetItemGetInf(ITEMGETINF_0F);
            }
            break;
        case ACTOR_FISHING:
            if (s.isChild) {
                HIGH_SCORE(HS_FISHING) |= HS_FISH_PRIZE_CHILD;
            } else {
                HIGH_SCORE(HS_FISHING) |= HS_FISH_PRIZE_ADULT;
            }
            break;
        case ACTOR_EN_TAKARA_MAN:
            Flags_SetItemGetInf(ITEMGETINF_1B);
            break;
        case ACTOR_EN_DIVING_GAME:
            Flags_SetEventChkInf(EVENTCHKINF_OBTAINED_SILVER_SCALE);
            break;
    }
}

static bool CLM_HasMorePrizes(const CLMState& s) {
    return (s.actorId == ACTOR_EN_BOM_BOWL_MAN &&
            (!Flags_GetItemGetInf(ITEMGETINF_11) || !Flags_GetItemGetInf(ITEMGETINF_12))) ||
           (s.actorId == ACTOR_EN_GE1 && (!Flags_GetInfTable(INFTABLE_190) || !Flags_GetItemGetInf(ITEMGETINF_0F)));
}

static void CLM_HijackedUpdate(Actor* actor, PlayState* play) {
    auto it = gStates.find(actor);
    if (it == gStates.end())
        return;
    auto& state = it->second;
    const CLMAdapter* adapter = FindAdapter(state.actorId);

    bool passVanilla = (adapter != nullptr) && adapter->passVanillaUpdate;

    switch (state.phase) {
        case CLMPhase::TextShowing: {
            if (passVanilla && state.savedUpdate != nullptr) {
                actor->flags &= ~ACTOR_FLAG_TALK;
                state.savedUpdate(actor, play);
            }
            u8 ms = Message_GetState(&play->msgCtx);
            if (ms == TEXT_STATE_EVENT && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);
                state.phase = CLMPhase::WaitingForClose;
            } else if (ms == TEXT_STATE_NONE && play->msgCtx.msgMode == MSGMODE_NONE) {
                state.phase = CLMPhase::TextClosed;
            }
            break;
        }
        case CLMPhase::WaitingForClose: {
            if (passVanilla && state.savedUpdate != nullptr) {
                actor->flags &= ~ACTOR_FLAG_TALK;
                state.savedUpdate(actor, play);
            }
            if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE && play->msgCtx.msgMode == MSGMODE_NONE) {
                state.phase = CLMPhase::TextClosed;
            }
            break;
        }
        case CLMPhase::TextClosed: {
            bool offeredItem = adapter ? adapter->grantReward(actor, play, state) : false;
            if (!offeredItem && state.firstTime) {
                CLM_PostAcceptItem(actor, state);
            }

            if (!offeredItem && state.firstTime && CLM_HasMorePrizes(state) && adapter != nullptr) {
                adapter->resolveTextId(actor, state);
                state.phase = CLMPhase::WaitingForClose;
            } else
                state.phase = offeredItem ? CLMPhase::RewardOffered : CLMPhase::Done;
            break;
        }
        case CLMPhase::RewardOffered: {
            if (Actor_HasParent(actor, play)) {
                CLM_PostAcceptItem(actor, state);
                actor->parent = NULL;
                bool more = CLM_HasMorePrizes(state);
                if (more && adapter != nullptr) {
                    adapter->resolveTextId(actor, state);
                    state.phase = CLMPhase::WaitingForClose;
                } else
                    state.phase = CLMPhase::Done;
            } else if (IS_RANDO) {
                CLM_TryDirectRandoDelivery(actor, play, state);
            } else {
                Actor_OfferGetItem(actor, play, state.getItemId, 2000.0f, 1000.0f);
            }
            break;
        }
        case CLMPhase::Done: {
            if (state.savedUpdate != nullptr) {
                actor->update = state.savedUpdate;
            }
            Player* player = GET_PLAYER(play);
            if (player != nullptr && player->csAction != 0) {
                player->csAction = 0;
                player->csActor = nullptr;
                player->cv.haltActorsDuringCsAction = false;
            }

            if (state.actorId == ACTOR_EN_DIVING_GAME) {
                auto safeIt = gDivingSafeActionFunc.find(actor);
                if (safeIt != gDivingSafeActionFunc.end()) {
                    EnDivingGame* dg = reinterpret_cast<EnDivingGame*>(actor);
                    dg->actionFunc = safeIt->second;
                    dg->state = ENDIVINGGAME_STATE_NOTPLAYING;
                    dg->phase = 0;
                    dg->unk_292 = 0;
                }
            }
            gStates.erase(it);
            break;
        }
    }
}

static void CLM_OnAnyTextOpens(uint16_t* textId, bool* loadFromMessageTable) {
    if (!CLM_IsWorn())
        return;

    PlayState* play = gPlayState;
    if (play == nullptr)
        return;

    Actor* talkActor = GET_PLAYER(play)->talkActor;
    if (talkActor == nullptr)
        return;

    const CLMAdapter* adapter = FindAdapter(talkActor->id);
    if (adapter == nullptr)
        return;
    if (!adapter->shouldIntercept(talkActor))
        return;

    if (gStates.find(talkActor) != gStates.end())
        return;

    auto& state = gStates[talkActor];
    state.phase = CLMPhase::TextShowing;
    state.actorId = talkActor->id;
    state.savedUpdate = talkActor->update;

    *textId = adapter->resolveTextId(talkActor, state);

    // Clearing ACTOR_FLAG_TALK keeps the vanilla talk-accept branch from locking the player.
    talkActor->flags &= ~ACTOR_FLAG_TALK;

    Player* player = GET_PLAYER(play);
    if (player != nullptr && player->csAction != 0) {
        player->csAction = 0;
        player->csActor = nullptr;
        player->cv.haltActorsDuringCsAction = false;
    }

    talkActor->update = CLM_HijackedUpdate;
}

static void CLM_LoadText(const char* body) {
    std::string text;
    size_t column = 0, line = 0;
    for (const char* p = body; *p;) {
        if (*p == '^') {
            text += '\x04';
            column = line = 0;
            ++p;
            continue;
        }
        if (*p == ' ') {
            const char* end = p + 1;
            while (*end && *end != ' ' && *end != '^')
                ++end;
            if (column + (end - p) > 32) {
                text += (++line == 4 ? '\x04' : '\x01');
                if (line == 4)
                    line = 0;
                column = 0;
                ++p;
                continue;
            }
        }
        text += *p++;
        ++column;
    }
    text += '\x0B';
    text += '\x02';
    Font* font = &gPlayState->msgCtx.font;
    size_t length = text.size() < sizeof(font->msgBuf) - 1 ? text.size() : sizeof(font->msgBuf) - 1;
    std::memcpy(font->msgBuf, text.data(), length);
    font->msgBuf[length] = 0;
    font->charTexBuf[0] = 0;
    gPlayState->msgCtx.msgLength = font->msgLength = length;
}
#define CLM_BUILD_MSG(name, body)                                    \
    static void name(uint16_t* textId, bool* loadFromMessageTable) { \
        CLM_LoadText(body);                                          \
        *loadFromMessageTable = false;                               \
    }

CLM_BUILD_MSG(BuildSyatekiChildFirst,
              "Oh! A-a royal inspector?!^"
              "Kid, you came for the weekly tally?^"
              "Business has been slow -- only rats and crows want to test their aim these days.^"
              "Here, take this as tribute. Tell His Majesty I'm behind on... paperwork.")
CLM_BUILD_MSG(BuildSyatekiChildRepeat, "Oh... back again, inspector?^"
                                       "Business is still slow, sir. Here, take a little something for your trouble...")
CLM_BUILD_MSG(BuildSyatekiAdultFirst,
              "Ah! The King's tax man! Welcome, welcome, good sir!^"
              "Great King Ganondorf's patrols have been so... thorough this moon.^"
              "Truly, a golden age for commerce!^"
              "Take this quiver -- no, PLEASE, I insist! A loyal subject is always eager to contribute!^"
              "L-long may the King reign!")
CLM_BUILD_MSG(BuildSyatekiAdultRepeat, "Please send the Great King my warmest regards, good sir!^"
                                       "Take this humble offering -- a loyal subject's duty!")

CLM_BUILD_MSG(BuildBowlingFirst, "Well, WELL... a tax collector? For the KING himself?^"
                                 "My, my -- those royal robes must hide a very... generous purse, don't they?^"
                                 "Come, sit closer. A man of your means deserves the VIP treatment.^"
                                 "Take this little prize -- on the house. Next visit, you bring me something shiny.")
CLM_BUILD_MSG(BuildBowlingRepeat, "Back again, handsome? Still no jewelry? Tsk, tsk...^"
                                  "Here, take a few rupees and run along now. I'm a busy woman.")

CLM_BUILD_MSG(BuildIngoChild, "That face...^"
                              "It's so familiar to me, in fact, it looks like me, but with a great depression.^"
                              "Like someone who has had dreams, but couldn't reach them because...^"
                              "Kid, take that off, please!^"
                              "I can't focus on work thinking about that!")
CLM_BUILD_MSG(BuildIngoAdultPreTalon, "That-!^"
                                      "That face, it's... it's me! But...^"
                                      "...No, it's my inner self.^"
                                      "I see it so clearly, I thought taking this ranch would bring me joy, but...^"
                                      "Why do I still feel...sad?^"
                                      "...Talon, he was a lazy bum, but, he was also a friend.^"
                                      "Yes, I can see it all so clearly.^"
                                      "Kid, you have shown me the error of my ways, now I must make things right.^"
                                      "I can't offer much, but I will allow you to take the red horse.^"
                                      "Actually, I was training it for Ganondorf, but Malon spoke highly of you.^"
                                      "I entrust you to take good care of her.^"
                                      "And if Talon comes back, he can have the ranch, I accept my role.")
CLM_BUILD_MSG(BuildIngoAdultPostTalon, "That mask...^"
                                       "Yes, that was me mere moments ago.^"
                                       "Talon made sure I learned my lesson, and not just through words.^"
                                       "Though, that's not the entire truth...^"
                                       "I was feeling like that mask even before he came back.^"
                                       "I can't understand the feeling, but, I must push past it.^"
                                       "You don't have Malon's song, do you?^"
                                       "She has taken a liking to you, kid, so...^"
                                       "With their permission, you can have her horse.^"
                                       "You'd just have to find the horse yourself if you lose her.")
CLM_BUILD_MSG(BuildIngoAlready, "...yes, the horse is yours, kid. Take her.")

CLM_BUILD_MSG(
    BuildTalonFirst,
    "Hur hur hur... a tax man? For the cuccos?^"
    "Well I'll be -- they're finally regulating poultry. 'Bout time...^"
    "Tell ya what, pardner. Take this bottle o' Lon Lon milk. Call it a... henhouse health fee, or somethin'.^"
    "Now excuse me, I was in the middle of a fine nap...")
CLM_BUILD_MSG(BuildTalonAsleep, "...zzz... zzz... hur hur... cuccos...")

CLM_BUILD_MSG(BuildMalonBuy, "Pffft -- hahahahaha!^"
                             "Fairy boy? Is that YOU under there?!^"
                             "A tax collector? With those skinny arms? Oh goddesses, I'm gonna cry laughing...^"
                             "Okay, okay -- tell you what, 'tax man.' Just for the laughs...^"
                             "...How about I sell you a cow? Yeah, you heard me. A real live cow.^"
                             "100 rupees. And no haggling with that face.^"
                             "Heeheehee. Pleasure doing business with you, 'officer.'")
CLM_BUILD_MSG(BuildMalonBroke, "Pffft -- hahahaha!^"
                               "Fairy boy, is that YOU?^"
                               "A 'tax collector' who's BROKE? Oh that's the funniest thing I've heard all year.^"
                               "Come back when you've actually got coin, eh?")
CLM_BUILD_MSG(BuildMalonRepeat, "Already sold you one, fairy boy! Now shoo!")

CLM_BUILD_MSG(BuildHBAFirst, "HOW did you get past the guards?!^"
                             "No men are allowed in this fortress -- ESPECIALLY not HYLIAN ones!^"
                             "...Of course. Another tax collector. Lord Ganondorf bleeds his OWN people dry now.^"
                             "His 'archery tariff' is strangling the training program. We used to rule this desert -- "
                             "now we're just accountants for his crown.^"
                             "FINE. Take the prize. And tell him every sister here curses his name.")
CLM_BUILD_MSG(BuildHBARepeat, "Still here? Walk out before I change my mind.^"
                              "Take a few rupees. Don't come back.")

CLM_BUILD_MSG(BuildFishingFirst, "Eh? A tax man? Out here? You walked ALL this way in those boots?^"
                                 "Listen, between you and me, the pond's been pretty dry since the King changed...^"
                                 "Take this and say you inspected. Nothin's biting anyway.")
CLM_BUILD_MSG(BuildFishingRepeat, "Still nothin' bitin'. Beat it.^"
                                  "Here -- a few coins. Tell the King I'm cooperatin'.")

CLM_BUILD_MSG(BuildTakaraFirst,
              "Shhh! Shhh! Keep it DOWN! A tax collector?! Here?!^"
              "Look, officially I run a 'children's entertainment venue.' Off the books it's... less family-friendly.^"
              "Here -- the prize from my best chest. Consider the paperwork filed. And you never saw me, understood?")
CLM_BUILD_MSG(BuildTakaraRepeat, "Still clean! Nothin' to audit! Take some coin and GO!")

CLM_BUILD_MSG(
    BuildDivingFirst,
    "A Hyrulean tax collector? In Zora waters?^"
    "...Our treaty with the surface throne is clear -- Zora's Domain pays in fish, not in rupees.^"
    "But the surface kings always want more coin, don't they?^"
    "Take this scale. Tell your king the fountain was inspected and found compliant. Tell him NOTHING of the rest.")
CLM_BUILD_MSG(BuildDivingRepeat, "Still compliant. Leave the fountain be.^"
                                 "A small token for your troubles, collector.")

static void CLM_OnDivingActorUpdate(void* actorRef) {
    EnDivingGame* dg = static_cast<EnDivingGame*>(actorRef);
    Player* player = (gPlayState != nullptr) ? GET_PLAYER(gPlayState) : nullptr;
    if (player == nullptr)
        return;
    if (player->stateFlags1 & PLAYER_STATE1_TALKING)
        return;
    if (gStates.find(&dg->actor) != gStates.end())
        return;
    if (dg->state != ENDIVINGGAME_STATE_NOTPLAYING)
        return;
    if (dg->actor.flags & ACTOR_FLAG_TALK)
        return;

    gDivingSafeActionFunc[&dg->actor] = dg->actionFunc;
}

static void CLM_OnSceneInit(int16_t sceneNum) {
    if (!gStates.empty()) {
        gStates.clear();
    }
    gDivingSafeActionFunc.clear();
}

extern "C" void MmTax_Init(const SOHModApi* api) {
    sTaxApi = api;

    SOH_REGISTER_HOOK(api, OnOpenText, CLM_OnAnyTextOpens);
    SOH_REGISTER_HOOK(api, OnSceneInit, CLM_OnSceneInit);
    SOH_REGISTER_HOOK_FOR_ID(api, OnActorUpdate, ACTOR_EN_DIVING_GAME, CLM_OnDivingActorUpdate);

    auto reg = [](uint16_t id, void (*fn)(uint16_t*, bool*)) {
        sTaxApi->RegisterHookForIdByName("OnOpenText", id, (void*)fn);
    };

    reg(CLM_TEXT_SYATEKI_CHILD_FIRST, BuildSyatekiChildFirst);
    reg(CLM_TEXT_SYATEKI_CHILD_REPEAT, BuildSyatekiChildRepeat);
    reg(CLM_TEXT_SYATEKI_ADULT_FIRST, BuildSyatekiAdultFirst);
    reg(CLM_TEXT_SYATEKI_ADULT_REPEAT, BuildSyatekiAdultRepeat);
    reg(CLM_TEXT_BOWLING_FIRST, BuildBowlingFirst);
    reg(CLM_TEXT_BOWLING_REPEAT, BuildBowlingRepeat);
    reg(CLM_TEXT_INGO_CHILD, BuildIngoChild);
    reg(CLM_TEXT_INGO_ADULT_PRETALON, BuildIngoAdultPreTalon);
    reg(CLM_TEXT_INGO_ADULT_POSTTALON, BuildIngoAdultPostTalon);
    reg(CLM_TEXT_INGO_ALREADY, BuildIngoAlready);
    reg(CLM_TEXT_TALON_FIRST, BuildTalonFirst);
    reg(CLM_TEXT_TALON_ASLEEP, BuildTalonAsleep);
    reg(CLM_TEXT_MALON_BUY, BuildMalonBuy);
    reg(CLM_TEXT_MALON_BROKE, BuildMalonBroke);
    reg(CLM_TEXT_MALON_REPEAT, BuildMalonRepeat);
    reg(CLM_TEXT_HBA_FIRST, BuildHBAFirst);
    reg(CLM_TEXT_HBA_REPEAT, BuildHBARepeat);
    reg(CLM_TEXT_FISHING_FIRST, BuildFishingFirst);
    reg(CLM_TEXT_FISHING_REPEAT, BuildFishingRepeat);
    reg(CLM_TEXT_TAKARA_FIRST, BuildTakaraFirst);
    reg(CLM_TEXT_TAKARA_REPEAT, BuildTakaraRepeat);
    reg(CLM_TEXT_DIVING_FIRST, BuildDivingFirst);
    reg(CLM_TEXT_DIVING_REPEAT, BuildDivingRepeat);
}
