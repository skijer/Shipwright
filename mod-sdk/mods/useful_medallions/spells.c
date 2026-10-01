// The six medallions as spells: pressing one runs NEI's cast (vanilla's Din's Fire flow widened to six spells)
// and spawns its SW97 actor. Holding the Shadow Medallion trades three hearts for magic.

#include "z64items.h"

#include <string.h>

#include "sw97_mod.h"

#define SPELL_ANIM_SPEED 0.83f
#define SPELL_FAST_FARORES_HOLD_FRAMES 10
#define SHADOW_EXCHANGE_HOLD_FRAMES 20
#define SHADOW_EXCHANGE_HEART_COST (3 * 0x10)
#define SHADOW_EXCHANGE_MAGIC_GAIN 24
#define C_BUTTON_FIRST 1
#define C_BUTTON_LAST 3

#define ANIMSFX_SHIFT_TYPE(type) ((type) << 11)
#define ANIMSFX_DATA(type, frame) ((ANIMSFX_SHIFT_TYPE(type) | ((frame)&0x7FF)))
#define ANIMSFX_TYPE_GENERAL 1
#define ANIMSFX_TYPE_VOICE 4
#define ANIMSFX_TYPE_RUNNING 6
#define ANIMSFX_TYPE_WALKING 8
#define GAMEPLAY_KEEP_ANIM(name) "__OTR__objects/gameplay_keep/" name

// Layout of z_player.c's private AnimSfxEntry.
typedef struct {
    u16 sfxId;
    s16 data;
} SpellAnimSfx;

typedef struct {
    const char* key;
    uint8_t questItem;
    Sw97Spell spell;
    const char* iconPath;
    const char* namePath;
    const char* pauseText;
} MedallionInfo;

void Player_SetupActionPreserveItemAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ProcessAnimSfxList(Player* player, SpellAnimSfx* entry);
s32 Player_DecelerateToZero(Player* player);
s32 func_808332B8(Player* player);
void func_80832224(Player* player);
void func_80835EA4(PlayState* play, s32 cameraArg);
void func_80839FFC(Player* player, PlayState* play);

static const ALIGN_ASSET(2) char sTameAnim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tame");
static const ALIGN_ASSET(2) char sKaze1Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_kaze1");
static const ALIGN_ASSET(2) char sKaze2Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_kaze2");
static const ALIGN_ASSET(2) char sKaze3Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_kaze3");
static const ALIGN_ASSET(2) char sHonoo1Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_honoo1");
static const ALIGN_ASSET(2) char sHonoo2Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_honoo2");
static const ALIGN_ASSET(2) char sHonoo3Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_honoo3");
static const ALIGN_ASSET(2) char sTamashii1Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tamashii1");
static const ALIGN_ASSET(2) char sTamashii2Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tamashii2");
static const ALIGN_ASSET(2) char sTamashii3Anim[] = GAMEPLAY_KEEP_ANIM("gPlayerAnim_link_magic_tamashii3");

static const ALIGN_ASSET(2) char sForestIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionForestTex";
static const ALIGN_ASSET(2) char sFireIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionFireTex";
static const ALIGN_ASSET(2) char sWaterIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionWaterTex";
static const ALIGN_ASSET(2) char sSpiritIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionSpiritTex";
static const ALIGN_ASSET(2) char sShadowIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionShadowTex";
static const ALIGN_ASSET(2) char sLightIcon[] = "__OTR__textures/icon_item_custom/gItemIconMedallionLightTex";

static const ALIGN_ASSET(2) char sForestName[] = "__OTR__textures/item_name_static/gForestMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sFireName[] = "__OTR__textures/item_name_static/gFireMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sWaterName[] = "__OTR__textures/item_name_static/gWaterMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sSpiritName[] = "__OTR__textures/item_name_static/gSpiritMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sShadowName[] = "__OTR__textures/item_name_static/gShadowMedallionItemNameENGTex";
static const ALIGN_ASSET(2) char sLightName[] = "__OTR__textures/item_name_static/gLightMedallionItemNameENGTex";

// NEI's six-spell tables: 0..2 keep vanilla's Farore/Din/Nayru rows, 3..5 are the SW97 additions.
static const char* const sCastAnims[SW97_SPELL_COUNT] = {
    sKaze1Anim, sHonoo1Anim, sTamashii1Anim, sKaze1Anim, sTamashii1Anim, sHonoo1Anim,
};
static const char* const sHoldAnims[SW97_SPELL_COUNT] = {
    sKaze2Anim, sHonoo2Anim, sTamashii2Anim, sKaze2Anim, sTamashii2Anim, sHonoo2Anim,
};
static const char* const sReleaseAnims[SW97_SPELL_COUNT] = {
    sKaze3Anim, sHonoo3Anim, sTamashii3Anim, sKaze3Anim, sTamashii3Anim, sHonoo3Anim,
};
static const u8 sHoldFrames[SW97_SPELL_COUNT] = { 70, 10, 10, 70, 10, 10 };
static const u8 sSpellCosts[SW97_SPELL_COUNT] = { 12, 24, 24, 12, 24, 12 };

static SpellAnimSfx sCastSfx[] = {
    { NA_SE_PL_SKIP, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    { NA_SE_VO_LI_SWORD_N, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
    { 0, -ANIMSFX_DATA(ANIMSFX_TYPE_RUNNING, 26) },
};

static SpellAnimSfx sSpellSfx[SW97_SPELL_COUNT][2] = {
    {
        { 0, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_FROL, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 30) },
    },
    {
        { 0, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_NALE, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 44) },
    },
    {
        { NA_SE_VO_LI_MAGIC_ATTACK, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
        { NA_SE_IT_SWORD_SWING_HARD, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    },
    {
        { 0, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_FROL, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 30) },
    },
    {
        { NA_SE_VO_LI_MAGIC_ATTACK, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
        { NA_SE_IT_SWORD_SWING_HARD, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    },
    {
        { 0, ANIMSFX_DATA(ANIMSFX_TYPE_WALKING, 20) },
        { NA_SE_VO_LI_MAGIC_NALE, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 44) },
    },
};

static const MedallionInfo sMedallions[SW97_SPELL_COUNT] = {
    { "nei.medallion.forest", QUEST_MEDALLION_FOREST, SW97_SPELL_WIND, sForestIcon, sForestName,
      "%gForest Medallion&%wWind spell, 12 MP. A tornado that&drags enemies in and grinds them." },
    { "nei.medallion.fire", QUEST_MEDALLION_FIRE, SW97_SPELL_FIRE, sFireIcon, sFireName,
      "%rFire Medallion&%wFire spell, 12 MP. A column of flame&that burns harder the longer it stands." },
    { "nei.medallion.water", QUEST_MEDALLION_WATER, SW97_SPELL_ICE, sWaterIcon, sWaterName,
      "%cWater Medallion&%wIce spell, 12 MP. Freezes every enemy&it touches for 6 seconds." },
    { "nei.medallion.spirit", QUEST_MEDALLION_SPIRIT, SW97_SPELL_SOUL, sSpiritIcon, sSpiritName,
      "%oSpirit Medallion&%wSoul spell, 24 MP. Turns you into a&fairy until you cast it again." },
    { "nei.medallion.shadow", QUEST_MEDALLION_SHADOW, SW97_SPELL_DARK, sShadowIcon, sShadowName,
      "%pShadow Medallion&%wDark spell, 24 MP. A shield that blocks&all damage for a minute. Hold %y\xA1%w to&trade "
      "3 hearts for magic." },
    { "nei.medallion.light", QUEST_MEDALLION_LIGHT, SW97_SPELL_LIGHT, sLightIcon, sLightName,
      "%yLight Medallion&%wLight spell, 24 MP. Undead freeze for&30 seconds and you heal 6 hearts." },
};

static s8 sPendingSpell = -1;
static uint32_t sOwnedQuestItems = 0xFFFFFFFF;
static s16 sShadowHoldFrames;
static bool sIsShadowExchanged;

static void CastAction(Player* player, PlayState* play);

static const MedallionInfo* FindMedallion(Sw97Spell spell) {
    for (uint8_t i = 0; i < SW97_SPELL_COUNT; i++) {
        if (sMedallions[i].spell == spell) {
            return &sMedallions[i];
        }
    }
    return NULL;
}

// Nothing in the item registry derives ownership from a quest flag, so the mod restates it as the flags move.
static void SyncQuestOwnership(void) {
    uint32_t questItems = gSaveContext.inventory.questItems;

    if (questItems == sOwnedQuestItems) {
        return;
    }
    sOwnedQuestItems = questItems;
    for (uint8_t i = 0; i < SW97_SPELL_COUNT; i++) {
        gSw97Api->SetCustomItemOwned(sMedallions[i].key, CHECK_QUEST_ITEM(sMedallions[i].questItem) != 0);
    }
}

// Water rides Farore's item action in NEI, so Fast Farore's Wind speeds it up too.
static bool IsFastFarores(s8 spell) {
    return spell == SW97_SPELL_ICE && CVarGetInteger(SW97_CVAR_FAST_FARORES, 0);
}

static Actor* SpawnSpell(PlayState* play, Player* player, Sw97Spell spell) {
    return Actor_Spawn(&play->actorCtx, play, Sw97_GetSpellActorId(spell), player->actor.world.pos.x,
                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 0);
}

static void PlayCastAnim(PlayState* play, Player* player, const char* animation, bool isFast) {
    LinkAnimation_PlayOnceSetSpeed(play, &player->skelAnime, (LinkAnimationHeader*)animation,
                                   SPELL_ANIM_SPEED * (isFast ? 2 : 1));
}

static void StartCast(PlayState* play, Player* player, Sw97Spell spell) {
    bool isFast = IsFastFarores(spell);

    Player_SetupActionPreserveItemAction(play, player, CastAction, 0);
    player->av1.actionVar1 = spell;
    player->av2.actionVar2 = 0;
    Magic_RequestChange(play, sSpellCosts[spell], MAGIC_CONSUME_WAIT_PREVIEW);
    PlayCastAnim(play, player, sTameAnim, isFast);
    if (!isFast && spell == SW97_SPELL_FIRE) {
        player->subCamId = OnePointCutscene_Init(play, 1100, -101, NULL, MAIN_CAM);
    } else if (!isFast) {
        func_80835EA4(play, 10);
    }
    func_80832224(player);
}

static void LeaveCast(Player* player, PlayState* play) {
    func_80839FFC(player, play);
    func_8005B1A4(Play_GetCamera(play, 0));
}

static void AdvanceCast(Player* player, PlayState* play, Sw97Spell spell, bool isFast) {
    if (player->av2.actionVar2 == 0) {
        PlayCastAnim(play, player, sCastAnims[spell], isFast);
        if (SpawnSpell(play, player, spell) != NULL) {
            player->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
            gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
        } else {
            Magic_Reset(play);
        }
    } else {
        LinkAnimation_PlayLoopSetSpeed(play, &player->skelAnime, (LinkAnimationHeader*)sHoldAnims[spell],
                                       SPELL_ANIM_SPEED * (isFast ? 2 : 1));
    }
    player->av2.actionVar2++;
}

static void PlayCastSounds(Player* player, PlayState* play, Sw97Spell spell, bool isFast) {
    if (player->av2.actionVar2 == 0) {
        Player_ProcessAnimSfxList(player, sCastSfx);
        return;
    }
    if (player->av2.actionVar2 == 1) {
        Player_ProcessAnimSfxList(player, sSpellSfx[spell]);
        if (LinkAnimation_OnFrame(&player->skelAnime, 30.0f)) {
            player->stateFlags1 &= ~(PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE);
        }
        return;
    }
    if ((isFast ? SPELL_FAST_FARORES_HOLD_FRAMES : sHoldFrames[spell]) < player->av2.actionVar2++) {
        PlayCastAnim(play, player, sReleaseAnims[spell], isFast);
        player->yaw = player->actor.shape.rot.y;
        player->av1.actionVar1 = -1;
    }
}

// Player_Action_808507F4 as NEI runs it for a medallion: every spell fires and forgets, none saves a warp.
static void CastAction(Player* player, PlayState* play) {
    s8 spell = (s8)player->av1.actionVar1;
    bool isFast = IsFastFarores(spell);

    if (LinkAnimation_Update(play, &player->skelAnime)) {
        if (spell < 0) {
            LeaveCast(player, play);
        } else {
            AdvanceCast(player, play, (Sw97Spell)spell, isFast);
        }
    } else if (spell >= 0) {
        PlayCastSounds(player, play, (Sw97Spell)spell, isFast);
    }
    Player_DecelerateToZero(player);
}

static bool CanAffordSpell(Sw97Spell spell) {
    return gSaveContext.magicCapacity != 0 && gSaveContext.magicState == MAGIC_STATE_IDLE &&
           gSaveContext.magic >= sSpellCosts[spell];
}

// The press lands inside vanilla's item handling, which would replace the action func the same frame: the
// cast starts at the end of the player's update instead.
static void PressMedallion(Sw97Spell spell) {
    if (!CanAffordSpell(spell)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    sPendingSpell = spell;
}

static void PressForest(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_WIND);
}

static void PressSpirit(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_SOUL);
}

static void PressShadow(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_DARK);
}

static void PressWater(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_ICE);
}

static void PressLight(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_LIGHT);
}

static void PressFire(PlayState* play, Player* player) {
    PressMedallion(SW97_SPELL_FIRE);
}

// Vanilla only casts from the ground or the water surface.
static bool CanStartCast(Player* player, PlayState* play) {
    bool isGrounded = (player->actor.bgCheckFlags & 1) != 0;

    return (isGrounded || func_808332B8(player)) && player->actionFunc != CastAction &&
           !Player_InBlockingCsMode(play, player);
}

static void StartPendingCast(Player* player, PlayState* play) {
    s8 spell = sPendingSpell;

    if (spell < 0) {
        return;
    }
    sPendingSpell = -1;
    if (CanStartCast(player, play)) {
        StartCast(play, player, (Sw97Spell)spell);
    }
}

static uint16_t FindShadowButtons(void) {
    static const u16 sCButtons[] = { BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
    const char* shadowKey = FindMedallion(SW97_SPELL_DARK)->key;
    uint16_t buttons = 0;

    for (uint8_t button = C_BUTTON_FIRST; button <= C_BUTTON_LAST; button++) {
        const char* equipped = gSw97Api->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, shadowKey) == 0) {
            buttons |= sCButtons[button - C_BUTTON_FIRST];
        }
    }
    return buttons;
}

static void ResetShadowExchange(void) {
    sShadowHoldFrames = 0;
    sIsShadowExchanged = false;
}

// Outside the cast on purpose: the trade is what a player with an empty meter reaches for.
static void TickShadowExchange(Player* player, PlayState* play) {
    uint16_t buttons = FindShadowButtons();

    if (buttons == 0 || !(play->state.input[0].cur.button & buttons)) {
        ResetShadowExchange();
        return;
    }
    sShadowHoldFrames++;
    if (sIsShadowExchanged || sShadowHoldFrames < SHADOW_EXCHANGE_HOLD_FRAMES ||
        gSaveContext.health <= SHADOW_EXCHANGE_HEART_COST) {
        return;
    }
    gSaveContext.health -= SHADOW_EXCHANGE_HEART_COST;
    gSaveContext.magic = MIN(gSaveContext.magic + SHADOW_EXCHANGE_MAGIC_GAIN, gSaveContext.magicCapacity);
    Audio_PlayActorSound2(&player->actor, NA_SE_SY_GET_RUPY);
    sIsShadowExchanged = true;
}

static void TickSpells(void) {
    PlayState* play = gPlayState;
    Player* player = play == NULL ? NULL : GET_PLAYER(play);

    if (player == NULL) {
        return;
    }
    SyncQuestOwnership();
    StartPendingCast(player, play);
    TickShadowExchange(player, play);
}

static void ForgetSpellState(int32_t fileNum) {
    sOwnedQuestItems = 0xFFFFFFFF;
    sPendingSpell = -1;
    ResetShadowExchange();
}

static bool RegisterMedallion(const MedallionInfo* medallion, SOHCustomItemInitFunc press) {
    SOHCustomItemDefinition item = Z64Items_Define(medallion->key, medallion->iconPath, medallion->namePath);

    Z64Items_SetButtons(&item, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&item, SOH_ITEM_PAGE_QUEST_VANILLA, medallion->questItem, 0);
    Z64Items_SetPauseText(&item, medallion->pauseText);
    Z64Items_SetAction(&item, press, NULL);
    item.flags |= SOH_CUSTOM_ITEM_INSTANT;
    return Z64Items_Register(gSw97Api, &item);
}

bool Sw97_RegisterSpells(void) {
    static const SOHCustomItemInitFunc sPresses[SW97_SPELL_COUNT] = {
        PressForest, PressSpirit, PressShadow, PressWater, PressLight, PressFire,
    };

    for (uint8_t spell = 0; spell < SW97_SPELL_COUNT; spell++) {
        if (!RegisterMedallion(FindMedallion((Sw97Spell)spell), sPresses[spell])) {
            return false;
        }
    }
    SOH_REGISTER_HOOK(gSw97Api, OnPlayerUpdate, TickSpells);
    SOH_REGISTER_HOOK(gSw97Api, OnLoadFile, ForgetSpellState);
    return true;
}
