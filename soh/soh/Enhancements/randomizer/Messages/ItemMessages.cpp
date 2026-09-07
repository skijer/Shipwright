/**
 * This file handles custom messages relating to Items,
 * such as Get Item messages for non-vanilla items,
 * Vanilla/MQ hints when collecting Maps, Ice Trap messages,
 * etc.
 */
#include <soh/OTRGlobals.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/Traps.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/ShipInit.hpp"
#include <soh/ResourceManagerHelpers.h>
#include "soh/Enhancements/randomizer/randomizerTypes.h"

#include <cstdarg>
#include <algorithm>

extern "C" {
#include "variables.h"
#include "macros.h"
#include "functions.h"
#include "z64item.h"
extern PlayState* gPlayState;
extern u8 gLanternCatchPending; // item_lantern.c — fire type pending message display
}

// Forward declaration for custom item messages from randomizer.cpp
struct CustomItemMessageEntry {
    s16 rgId;
    ItemID itemId;
    const char* english;
    const char* german;
    const char* french;
};
extern const CustomItemMessageEntry* GetCustomItemMessage(s16 rgId);

void BuildTriforcePieceMessage(CustomMessage& msg) {
    auto rando = OTRGlobals::Instance->gRandomizer;
    uint8_t current = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected + 1;
    // if any settings are off, 0 them out here as a precaution
    uint8_t bridge = rando->GetRandoSettingValue(RSK_RAINBOW_BRIDGE) == RO_BRIDGE_TRIFORCE_PIECES
                         ? rando->GetRandoSettingValue(RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT)
                         : 0;
    uint8_t wincon = rando->GetRandoSettingValue(RSK_WINCON) == RO_WINCON_TRIFORCE_PIECES
                         ? rando->GetRandoSettingValue(RSK_WINCON_TRIFORCE_COUNT)
                         : 0;
    uint8_t GBK = rando->GetRandoSettingValue(RSK_GANONS_BOSS_KEY) == RO_GANON_BOSS_KEY_TRIFORCE_PIECES
                      ? rando->GetRandoSettingValue(RSK_GBK_TRIFORCE_COUNT)
                      : 0;
    uint8_t soul = rando->GetRandoSettingValue(RSK_GANONS_SOUL) == RO_GANONS_SOUL_TRIFORCE_PIECES
                       ? rando->GetRandoSettingValue(RSK_GANONS_SOUL_TRIFORCE_COUNT)
                       : 0;

    // If we reach wincon, we win!
    if (current == wincon) {
        msg = { "You completed the %yTriforce of Courage%w! %gGG%w!",
                "Das %yTriforce des Mutes%w! Du hast alle Splitter gefunden. %gGut gemacht%w!",
                "Vous avez complété la %yTriforce du Courage%w! %gFélicitations%w!" };
        // otherwise prioritise the different triggers
    } else if (current == bridge) {
        msg = { "You made your wish to the %yTriforce%w! %rTh%ye R%gai%cnb%bow %pBr%rid%yge %gha%cs r%bai%psed%w!",
                TODO_TRANSLATE, TODO_TRANSLATE };
    } else if (current == GBK) {
        msg = { "You completed the %yTriforce of Power%w! %rThe Key to Evil is yours%w!", TODO_TRANSLATE,
                TODO_TRANSLATE };
    } else if (current == soul) {
        msg = { "You completed the %yTriforce of Wisdom%w! %bGanon's soul is reclaimed%w!", TODO_TRANSLATE,
                TODO_TRANSLATE };
        // if everything is zero, then there's no goal...
    } else if (bridge + wincon + GBK + soul == 0) {
        msg = { "You found a %yTriforce Piece%w! But it's %puseless%w...", TODO_TRANSLATE, TODO_TRANSLATE };
    } else {
        // if nothing is complete, we need to check is we have more than we need
        uint8_t highest = std::max({ current, bridge, wincon, GBK, soul });
        if (highest == current) {
            // RANDOTODO TODO_TRANSLATE you could maybe make this sound cleaner because InsertNumber allows for dynamic
            // plurals
            msg = { "You found a spare %yTriforce Piece%w! You only needed %c[[d]]%w, but you have %g[[current]]%w!",
                    "Ein übriger %yTriforce-Splitter%w! Du hast nun %g[[current]]%w von %c[[d]]%w nötigen gefunden.",
                    "Vous avez trouvé un %yFragment de Triforce%w en plus! Vous n'aviez besoin que de %c[[d]]%w, "
                    "mais vous en avez %g[[current]]%w en tout!" };
            msg.InsertNumber(std::max({ bridge, wincon, GBK, soul }));
        } else {
            // find the next goal by setting everything below current (including failed conditions set to 0 before)
            // to a high number, then looking for the lowest.
            // if we have the exact amount, it will be caught by the first check, so no worries there
            if (bridge < current) {
                bridge = 255;
            }
            if (GBK < current) {
                GBK = 255;
            }
            if (soul < current) {
                soul = 255;
            }
            if (wincon < current) {
                wincon = 255;
            }
            uint8_t next = std::min({ bridge, GBK, soul, wincon });

            uint8_t remaining = next - current;
            float percentageCollected = (float)current / (float)next;

            if (percentageCollected <= 0.25) {
                msg = { "You found a %yTriforce Piece%w! %g[[current]]%w down, %c[[d]]%w more and you [[condition]]! "
                        "It's a start!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (percentageCollected <= 0.5) {
                msg = { "You found a %yTriforce Piece%w! that makes %g[[current]]%w, %c[[d]]%w to go until you "
                        "[[condition]]! Progress!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (percentageCollected <= 0.75) {
                msg = { "You found a %yTriforce Piece%w! You have %g[[current]]%w and need %c[[d]]%w more and you "
                        "[[condition]]! Over half-way there!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (percentageCollected < 1.0) {
                msg = { "You found a %yTriforce Piece%w! %g[[current]]%w down, %c[[d]]%w left until you [[condition]]! "
                        "Almost done!",
                        TODO_TRANSLATE, TODO_TRANSLATE };
            }

            // default condition is soul
            CustomMessage condition = { "%brelease Ganons Soul%w", TODO_TRANSLATE, TODO_TRANSLATE };
            if (next == wincon) {
                condition = { "%gWin the game%w", TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (next == bridge) {
                condition = { "%csummon the Rainbow Bridge%w", TODO_TRANSLATE, TODO_TRANSLATE };
            } else if (next == GBK) {
                condition = { "%rfind the key to Ganondorf's Lair%w", TODO_TRANSLATE, TODO_TRANSLATE };
            }
            msg.Replace("[[condition]]", condition);
            msg.InsertNumber(remaining);
        }
    }
    msg.Replace("[[current]]", std::to_string(current));
    msg.AutoFormat(ITEM_CUSTOM);
}

void BuildTriforceMessage(CustomMessage& msg) {
    msg = { "You completed the %yTriforce of&Courage%w! %gGG%w!",
            "Das %yTriforce des Mutes%w! Du hast&alle Splitter gefunden. %gGut gemacht%w!",
            "Vous avez complété la %yTriforce&du Courage%w! %gFélicitations%w!" };
    msg.Format(ITEM_CUSTOM);
}

void BuildCustomItemMessage(Player* player, CustomMessage& msg) {
    int16_t rgid;
    if (player->getItemEntry.objectId != OBJECT_INVALID) {
        rgid = player->getItemEntry.getItemId;
    } else {
        rgid = player->getItemId;
    }

    // Check if this is a custom item with a detailed message
    const CustomItemMessageEntry* customMsg = GetCustomItemMessage(rgid);
    if (customMsg != nullptr) {
        // Use the detailed custom message. Pass the real ItemID so Message_LoadItemIcon's
        // ">= ITEM_ROCS_FEATHER_SKIJER" branch fires (z_message_PAL.c:1671) and loads the
        // 32x32 icon via ExtInv_GetItemIcon(itemId). Without this, AutoFormat() with no
        // argument leaves the message without an ITEM_OBTAINED token at all, and the
        // textbox renders with no icon on the left.
        msg = CustomMessage(customMsg->english, customMsg->german, customMsg->french, TEXTBOX_TYPE_BLUE);
        msg.AutoFormat(customMsg->itemId);
        return;
    }

    // Fall back to generic "You found X!" message for other items
    msg = CustomMessage("You found [[article]][[color]][[name]]%w!",
                        "Du erhältst [[article]][[color]][[name]]%w gefunden!",
                        "Vous avez trouvé [[article]][[color]][[name]]%w!", TEXTBOX_TYPE_BLUE);
    CustomMessage name =
        CustomMessage(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetName(), TEXTBOX_TYPE_BLUE);
    if (rgid == RG_OPEN_CHEST &&
        OTRGlobals::Instance->gRandoContext->GetOption(RSK_SHUFFLE_OPEN_CHEST).Is(RO_OPEN_CHEST_PROGRESSIVE)) {
        // message is built before the item is given, so the flags still say which copy this is
        name = Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_CHEST)
                   ? CustomMessage("Open Big Chests", "Große Truhen öffnen", "Ouvrir les grands coffres",
                                   TEXTBOX_TYPE_BLUE)
                   : CustomMessage("Open Small Chests", "Kleine Truhen öffnen", "Ouvrir les petits coffres",
                                   TEXTBOX_TYPE_BLUE);
    }
    CustomMessage article = CustomMessage(
        Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetArticle(), TEXTBOX_TYPE_BLUE);
    msg.Replace("[[article]]", article);
    msg.Replace("[[color]]", Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetColor());
    msg.Replace("[[name]]", name);
    if (Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).HasCustomIcon()) {
        // Use the real ItemID from the item table so vanilla's Message_LoadItemIcon picks
        // up the ">= ITEM_ROCS_FEATHER_SKIJER" branch and resolves via ExtInv_GetItemIcon.
        ItemID itemId =
            static_cast<ItemID>(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetItemID());
        msg.AutoFormat(itemId);
    } else {
        // No custom icon: AutoFormat() with no argument inserts no item-icon token, so the textbox
        // renders with NO icon on the left. For a plain vanilla item (bomb bag, quiver, hover boots,
        // tunics...) that is just a missing icon, and its real one is one lookup away: pass
        // giEntry->itemId — the actual ItemID, NOT GetItemID() which returns the get-item id.
        //
        // Bounded on purpose. Many MM-port rows are built with a RandomizerGet in the itemId slot
        // (see RG_MM_SONG_SONATA), which is far past the end of gItemIcons; handing that to
        // Message_LoadItemIcon would take the custom-item branch and memcpy from a NULL icon.
        // Below ITEM_ROCS_FEATHER_SKIJER is exactly the vanilla range, and everything custom
        // already went through the HasCustomIcon path above. Anything else stays iconless — an
        // empty textbox beats a wrong or invented icon (Skijer's call). Skijer's NEI
        auto gi = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rgid)).GetGIEntry();
        if (gi != nullptr && gi->itemId != ITEM_NONE && gi->itemId < ITEM_ROCS_FEATHER_SKIJER) {
            msg.AutoFormat(static_cast<ItemID>(gi->itemId));
        } else {
            msg.AutoFormat();
        }
    }
}

void LoadCustomItemIcon(bool displayAsEnglish) {
    Player* player = GET_PLAYER(gPlayState);
    const char* customIcon = nullptr;
    CustomIconSize iconSize = ICON_SIZE_32;
    // Same rule as the hooks above: getItemId is only an RG on MOD_RANDOMIZER entries.
    if (player->getItemEntry.objectId != OBJECT_INVALID && player->getItemEntry.modIndex == MOD_RANDOMIZER) {
        RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
        customIcon = Rando::StaticData::RetrieveItem(rgid).GetCustomIcon();
        iconSize = Rando::StaticData::RetrieveItem(rgid).GetCustomIconSize();
    } else if (player->getItemEntry.objectId != OBJECT_INVALID) {
        customIcon = nullptr; // vanilla entry: its own icon token in the message is already right
    } else {
        // if we're seeing an icon and we don't have a GI, assume we're in the alter text showing a triforce piece
        customIcon = Rando::StaticData::RetrieveItem(RG_TRIFORCE_PIECE).GetCustomIcon();
        iconSize = Rando::StaticData::RetrieveItem(RG_TRIFORCE_PIECE).GetCustomIconSize();
    }
    if (customIcon != nullptr) {
        static int16_t sIconItem32XOffsets[] = { 74, 74, 74, 54 };
        static int16_t sIconItem24XOffsets[] = { 72, 72, 72, 50 };
        MessageContext* msgCtx = &gPlayState->msgCtx;
        uint8_t language = displayAsEnglish ? LANGUAGE_ENG : (Language)gSaveContext.language;
        if (iconSize == ICON_SIZE_32) {
            R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem32XOffsets[language];
            R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + 6;
            R_TEXTBOX_ICON_SIZE = 32;
        } else {
            R_TEXTBOX_ICON_XPOS = R_TEXT_INIT_XPOS - sIconItem24XOffsets[language];
            R_TEXTBOX_ICON_YPOS = (R_TEXTBOX_Y + 10) + 10;
            R_TEXTBOX_ICON_SIZE = 24;
        }
        strcpy((char*)((uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE), customIcon);
        msgCtx->msgBufPos++;
        msgCtx->choiceNum = 1;
    }
}

void DrawCustomItemIcon(Gfx** p) {
    Gfx* gfx = *p;
    MessageContext* msgCtx = &gPlayState->msgCtx;
    Player* player = GET_PLAYER(gPlayState);
    CustomIconSize iconSize = ICON_SIZE_32;
    if (player->getItemEntry.objectId != OBJECT_INVALID && player->getItemEntry.modIndex == MOD_RANDOMIZER) {
        RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
        iconSize = Rando::StaticData::RetrieveItem(rgid).GetCustomIconSize();
    }
    if (iconSize == ICON_SIZE_24) {
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 24, 24, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(gfx++, (uintptr_t)msgCtx->textboxSegment + MESSAGE_STATIC_TEX_SIZE, G_IM_FMT_RGBA,
                            G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }
    *p = gfx;
}

void BuildItemMessage(u16* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    CustomMessage msg;

    if (player->getItemEntry.getItemId == RG_ICE_TRAP) {
        Rando::Traps::BuildIceTrapMessage(msg, player->getItemEntry);
    } else if (player->getItemEntry.getItemId == RG_TRIFORCE_PIECE) {
        BuildTriforcePieceMessage(msg);
    } else if (player->getItemEntry.getItemId == RG_TRIFORCE) {
        BuildTriforceMessage(msg);
    } else {
        BuildCustomItemMessage(player, msg);
    }
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void BuildMapMessage(uint16_t* textId, bool* loadFromMessageTable) {
    GetItemEntry itemEntry = GET_PLAYER(gPlayState)->getItemEntry;
    auto ctx = OTRGlobals::Instance->gRandoContext;
    CustomMessage msg =
        CustomMessage("You found the %g[[name]]%w! [[typeHint]]", "Du erhältst das %g[[name]]%w! [[typeHint]]",
                      "Vous ebtenez %g[[name]]%w! [[typeHint]]", TEXTBOX_TYPE_BLUE);
    int sceneNum;
    switch (itemEntry.getItemId) {
        case RG_DEKU_TREE_MAP:
            sceneNum = SCENE_DEKU_TREE;
            break;
        case RG_DODONGOS_CAVERN_MAP:
            sceneNum = SCENE_DODONGOS_CAVERN;
            break;
        case RG_JABU_JABUS_BELLY_MAP:
            sceneNum = SCENE_JABU_JABU;
            break;
        case RG_FOREST_TEMPLE_MAP:
            sceneNum = SCENE_FOREST_TEMPLE;
            break;
        case RG_FIRE_TEMPLE_MAP:
            sceneNum = SCENE_FIRE_TEMPLE;
            break;
        case RG_WATER_TEMPLE_MAP:
            sceneNum = SCENE_WATER_TEMPLE;
            break;
        case RG_SPIRIT_TEMPLE_MAP:
            sceneNum = SCENE_SPIRIT_TEMPLE;
            break;
        case RG_SHADOW_TEMPLE_MAP:
            sceneNum = SCENE_SHADOW_TEMPLE;
            break;
        case RG_BOTTOM_OF_THE_WELL_MAP:
            sceneNum = SCENE_BOTTOM_OF_THE_WELL;
            break;
        case RG_ICE_CAVERN_MAP:
            sceneNum = SCENE_ICE_CAVERN;
            break;
    }
    CustomMessage name =
        CustomMessage(Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(itemEntry.getItemId)).GetName());
    msg.Replace("[[name]]", name);
    if (ctx->GetOption(RSK_MQ_DUNGEON_RANDOM).Is(RO_MQ_DUNGEONS_NONE) ||
        (ctx->GetOption(RSK_MQ_DUNGEON_RANDOM).Is(RO_MQ_DUNGEONS_SET_NUMBER) &&
         ctx->GetOption(RSK_MQ_DUNGEON_COUNT).Is(MAX_MQ_DUNGEON_COUNT))) {
        msg.Replace("[[typeHint]]", "");
    } else if (ResourceMgr_IsSceneMasterQuest(sceneNum)) {
        msg.Replace("[[typeHint]]", Rando::StaticData::hintTextTable[RHT_DUNGEON_MASTERFUL].GetHintMessage());
    } else {
        msg.Replace("[[typeHint]]", Rando::StaticData::hintTextTable[RHT_DUNGEON_ORDINARY].GetHintMessage());
    }
    *loadFromMessageTable = false;
    msg.AutoFormat(ITEM_DUNGEON_MAP);
    msg.LoadIntoFont();
}

void BuildBossKeyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    if (player->getItemEntry.getItemId == RG_GANONS_CASTLE_BOSS_KEY &&
        !DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_GANONS_BOSS_KEY)) {
        return;
    }
    if (player->getItemEntry.getItemId != RG_GANONS_CASTLE_BOSS_KEY &&
        !DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_BOSS_KEYSANITY)) {
        return;
    }
    CustomMessage msg;
    BuildCustomItemMessage(player, msg);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

void BuildSmallKeyMessage(uint16_t* textId, bool* loadFromMessageTable) {
    Player* player = GET_PLAYER(gPlayState);
    if (player->getItemEntry.getItemId == RG_GERUDO_FORTRESS_SMALL_KEY &&
        OTRGlobals::Instance->gRandoContext->GetOption(RSK_GERUDO_KEYS).Is(RO_GERUDO_KEYS_VANILLA)) {
        return;
    }
    if (player->getItemEntry.getItemId != RG_GERUDO_FORTRESS_SMALL_KEY &&
        DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_KEYSANITY)) {
        return;
    }
    CustomMessage msg;
    BuildCustomItemMessage(player, msg);
    *loadFromMessageTable = false;
    msg.LoadIntoFont();
}

// Time Gate custom item - "Travel through time?" Yes/No prompt
void BuildTimeGateMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("Travel through time?\x1B%g&&Yes&No%w", "Durch die Zeit reisen?\x1B%g&&Ja&Nein%w",
                                      "Voyager dans le temps?\x1B%g&&Oui&Non%w");
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterItemMessages() {
    COND_ID_HOOK(OnOpenText, TEXT_RANDOMIZER_CUSTOM_ITEM, IS_RANDO, BuildItemMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_DUNGEON_MAP, DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS),
                 BuildMapMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_COMPASS, DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_SHUFFLE_MAPANDCOMPASS),
                 BuildItemMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_KEY_BOSS,
                 (DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_BOSS_KEYSANITY) ||
                  DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_GANONS_BOSS_KEY)),
                 BuildBossKeyMessage);
    COND_ID_HOOK(OnOpenText, TEXT_ITEM_KEY_SMALL,
                 (OTRGlobals::Instance->gRandoContext->GetOption(RSK_GERUDO_KEYS).IsNot(RO_GERUDO_KEYS_VANILLA) ||
                  DUNGEON_ITEMS_CAN_BE_OUTSIDE_DUNGEON(RSK_KEYSANITY)),
                 BuildSmallKeyMessage);
}

// ── Lantern fire catch messages (always available) ──────────────────────────

#define TEXT_LANTERN_CATCH 0x00F9

void BuildLanternCatchMessage(uint16_t* textId, bool* loadFromMessageTable) {
    u8 fireType = gLanternCatchPending;
    CustomMessage msg;

    // \x13\xB4 = item icon for ITEM_LANTERN (0xB4)
    // All fire types: swing lights torches, burns grass (updraft + spread)
    switch (fireType) {
        case 1: // REGULAR (orange)
            msg = CustomMessage(
                "\x13\xB4"
                "You caught %rRegular Fire%w!&Swing to %rlight torches%w,&%rburn grass%w and spawn flames.",
                "\x13\xB4"
                "Du hast %rnormales Feuer%w!&Schwinge um %rFackeln%w und&%rGras zu verbrennen%w.",
                "\x13\xB4"
                "Vous avez le %rFeu Normal%w!&Agitez pour %rallumer%w et&%rbruler l'herbe%w.",
                TEXTBOX_TYPE_BLUE);
            break;
        case 2: // BLUE
            msg = CustomMessage("\x13\xB4"
                                "You caught %bBlue Fire%w!&Swing to release %bblue fire%w&that %cmelts red ice%w.",
                                "\x13\xB4"
                                "Du hast %bblaues Feuer%w!&Schwinge um %crotes Eis%w&%bzu schmelzen%w.",
                                "\x13\xB4"
                                "Vous avez le %bFeu Bleu%w!&Agitez pour %cfondre la&glace rouge%w.",
                                TEXTBOX_TYPE_BLUE);
            break;
        case 3: // POE (purple)
            msg =
                CustomMessage("\x13\xB4"
                              "You caught %pPoe Fire%w!&%pReveals the invisible%w and&%pdispels illusions%w. No magic.",
                              "\x13\xB4"
                              "Du hast %pIrrlichterfeuer%w!&%pEnthullt Unsichtbares%w und&%plost Illusionen auf%w.",
                              "\x13\xB4"
                              "Vous avez le %pFeu Spectral%w!&%pRevele l'invisible%w et&%pdissipe les illusions%w.",
                              TEXTBOX_TYPE_BLUE);
            break;
        case 4: // GREEN
            msg = CustomMessage("\x13\xB4"
                                "You caught %gGreen Fire%w!&Slowly %gregenerates health%w&while it stays lit.",
                                "\x13\xB4"
                                "Du hast %ggruenes Feuer%w!&%gRegeneriert langsam Leben%w,&solange es brennt.",
                                "\x13\xB4"
                                "Vous avez le %gFeu Vert%w!&%gRegenere lentement la vie%w&tant qu'il brule.",
                                TEXTBOX_TYPE_BLUE);
            break;
        default:
            msg = CustomMessage("\x13\xB4"
                                "The lantern is empty.",
                                "\x13\xB4"
                                "Die Laterne ist leer.",
                                "\x13\xB4"
                                "La lanterne est vide.",
                                TEXTBOX_TYPE_BLUE);
            break;
    }

    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterLanternCatchMessage() {
    // Always available — not randomizer-dependent
    static HOOK_ID hookId = 0;
    GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnOpenText>(hookId);
    hookId = GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(TEXT_LANTERN_CATCH,
                                                                                         BuildLanternCatchMessage);
}

// Time Gate message registration (always available, not rando-dependent)
void RegisterTimeGateMessage() {
    COND_ID_HOOK(OnOpenText, TEXT_TIME_GATE_PROMPT, true, BuildTimeGateMessage);
}

// Chateau Romani get-item message (always available, not rando-dependent)
void BuildChateauRomaniMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage("You got %r\x08"
                                      "Chateau Romani%w!\x04"
                                      "Your magic power won't run out!%w",
                                      "Du hast %r\x08"
                                      "Chateau Romani%w erhalten!\x04"
                                      "Deine Magie wird nicht leer!%w",
                                      "Vous obtenez le %r\x08"
                                      "Chateau Romani%w!\x04"
                                      "Votre magie ne s'\xE9puisera pas!%w");
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterChateauRomaniMessage() {
    COND_ID_HOOK(OnOpenText, 0x9214, true, BuildChateauRomaniMessage);
}

// (Fleet Ship Combo: the old Happy Mask Shop "Travel to Termina?" prompt (0x9215) was removed —
// the blue warp is now a Door_Ana hole; falling in IS the confirmation.)

static RegisterShipInitFunc initFunc(RegisterItemMessages, { "IS_RANDO" });
static RegisterShipInitFunc initTimeGate(RegisterTimeGateMessage);
static RegisterShipInitFunc initChateau(RegisterChateauRomaniMessage);
static RegisterShipInitFunc initLanternCatch(RegisterLanternCatchMessage);

void RegisterCustomIconHooks() {
    // The original hook only fires when *should == false, but nothing in the call path
    // ever sets it to false for custom items — so the custom icon loaders never run and
    // vanilla tries to load Message_LoadItemIcon(ITEM_CUSTOM=0x9C) which is not a valid
    // OBJECT_GI_*. Detect custom-icon items via the player's getItemEntry, suppress
    // vanilla, and call our loader/drawer.
    // getItemId only holds a RandomizerGet when the entry IS a randomizer entry: the Item ctor puts
    // the RG there for MOD_RANDOMIZER rows and the vanilla GI id there for MOD_NONE ones. Casting a
    // GI id to RandomizerGet indexes a completely unrelated row, and if THAT row has a custom icon
    // the hook hijacks the textbox — which is why Iron Boots (GI 0x2E) showed Deku Nuts
    // (RG #0x2E = RG_PROGRESSIVE_NUT_UPGRADE) and Hover Boots (GI 0x2F) showed Deku Sticks. Gate on
    // modIndex so vanilla items keep their own icon token. Skijer's NEI
    COND_VB_SHOULD(VB_LOAD_ITEM_ICON, IS_RANDO, {
        Player* player = GET_PLAYER(gPlayState);
        if (player->getItemEntry.objectId != OBJECT_INVALID && player->getItemEntry.modIndex == MOD_RANDOMIZER) {
            RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
            if (Rando::StaticData::RetrieveItem(rgid).HasCustomIcon()) {
                *should = false;
                LoadCustomItemIcon(static_cast<bool>(va_arg(args, int)));
                return;
            }
        }
        if (*should == false) {
            LoadCustomItemIcon(static_cast<bool>(va_arg(args, int)));
        }
    });
    COND_VB_SHOULD(VB_DRAW_ITEM_ICON, IS_RANDO, {
        Player* player = GET_PLAYER(gPlayState);
        if (player->getItemEntry.objectId != OBJECT_INVALID && player->getItemEntry.modIndex == MOD_RANDOMIZER) {
            RandomizerGet rgid = static_cast<RandomizerGet>(player->getItemEntry.getItemId);
            if (Rando::StaticData::RetrieveItem(rgid).HasCustomIcon()) {
                *should = false;
                DrawCustomItemIcon(va_arg(args, Gfx**));
                return;
            }
        }
        if (*should == false) {
            DrawCustomItemIcon(va_arg(args, Gfx**));
        }
    });
}

static RegisterShipInitFunc customIconInitFunc(RegisterCustomIconHooks, { "IS_RANDO" });
