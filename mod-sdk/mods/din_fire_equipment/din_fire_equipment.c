// Din's Fire on the shield and sword. Port of marsh6487's din_fire_shield / din_fire_sword POCs (d54391b5,
// db3f11fc). The originals draw HD flame meshes from an external pack; this one asks the engine's own fire
// particles for the same look, so there is nothing to install. The flames run from OnPlayerUpdate, with the
// limb positions the engine already keeps for Link (bodyPartsPos, meleeWeaponInfo).
//
// Fire Damage (off by default, as in the original) makes a direct sword hit use the enemy's Fire Arrow reaction.
// The original added the fire bit to the sword's damage flags; the engine then picks the damage-table row of the
// highest set bit. Here the hit resolves as a plain sword hit and OnCollisionResolveDamage, which runs right
// after the row is read, swaps in the fire row's effect (or the whole row) by the original's rules.

#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#define DIN_SHIELD_CVAR "gMods.DinFire.Shield"
#define DIN_SWORD_CVAR "gMods.DinFire.Sword"
#define DIN_SFX_CVAR "gMods.DinFire.ChargeSfx"
#define DIN_DAMAGE_CVAR "gMods.DinFire.SwordDamage"
#define FIRE_ROW 0x0B // damage-table row of DMG_ARROW_FIRE
#define SWORD_PERIOD 1
#define BLADE_FLAMES 4

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnCollisionResolveDamage" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;

static bool IsGuarding(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_SHIELDING) && player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

static bool IsBladeOut(Player* player) {
    return player->leftHandType == PLAYER_MODELTYPE_LH_SWORD || player->meleeWeaponState != 0;
}

static bool IsPlayerVisible(Player* player) {
    return player->actor.scale.y > 0.0f && !(player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) &&
           !(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_WATER));
}

static void WreatheShield(PlayState* play, Player* player) {
    // The shield is held in the right hand; flames sit on the hand so they follow every guard pose.
    // colorIntensity is the flame's colour scale: 0 would draw it black, 1 is the plain orange fire.
    EffectSsFireTail_SpawnFlameOnPlayer(play, 0.012f, PLAYER_BODYPART_R_HAND, 1.0f);
    if (CVarGetInteger(DIN_SFX_CVAR, 0)) {
        Audio_PlayActorSound2(&player->actor, NA_SE_PL_ARROW_CHARGE_FIRE - SFX_FLAG);
    }
}

static void WreatheBlade(PlayState* play, Player* player) {
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    Color_RGBA8 prim = { 255, 225, 122, 255 };
    Color_RGBA8 env = { 255, 43, 3, 0 };
    WeaponInfo* info = &player->meleeWeaponInfo[0];

    if (play->gameplayFrames % SWORD_PERIOD != 0) {
        return;
    }
    for (s32 i = 0; i < BLADE_FLAMES; i++) {
        f32 t = (f32)i / (BLADE_FLAMES - 1);
        Vec3f pos = { info->base.x + (info->tip.x - info->base.x) * t, info->base.y + (info->tip.y - info->base.y) * t,
                      info->base.z + (info->tip.z - info->base.z) * t };
        Vec3f vel = { Rand_CenteredFloat(0.8f), 0.6f + Rand_ZeroFloat(0.6f), Rand_CenteredFloat(0.8f) };

        if (player->meleeWeaponState == 0) {
            // Blade at rest: the weapon quad is not live, so anchor on the left hand and let it rise.
            pos = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
            pos.y += 10.0f + 6.0f * i;
        }
        EffectSsKiraKira_SpawnSmall(play, &pos, &vel, &accel, &prim, &env);
        // The flame sits relative to its actor when no body part is given.
        pos.x -= player->actor.world.pos.x;
        pos.y -= player->actor.world.pos.y;
        pos.z -= player->actor.world.pos.z;
        EffectSsFireTail_SpawnFlame(play, &player->actor, &pos, 0.007f, -1, 1.0f);
    }
}

static void Burn(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL || play->pauseCtx.state != 0 || play->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    player = GET_PLAYER(play);
    if (!IsPlayerVisible(player)) {
        return;
    }
    if (CVarGetInteger(DIN_SHIELD_CVAR, 1) && IsGuarding(player)) {
        WreatheShield(play, player);
    }
    if (CVarGetInteger(DIN_SWORD_CVAR, 1) && IsBladeOut(player) && player->currentSwordItemId != ITEM_NONE) {
        WreatheBlade(play, player);
    }
}

// A direct sword hit: one of the two melee quads, carrying sword damage and nothing else.
static bool IsPlainSwordHit(Player* player, ColliderInfo* attack) {
    u32 flags = attack->toucher.dmgFlags;

    return (attack == &player->meleeWeaponQuads[0].info || attack == &player->meleeWeaponQuads[1].info) &&
           (flags & DMG_SWORD) && !(flags & ~((u32)DMG_SWORD));
}

static s32 TopBit(u32 flags) {
    s32 index = 0;

    for (; flags > 1; flags >>= 1) {
        index++;
    }
    return index;
}

static void BurnOnHit(Actor* victim, ColliderInfo* attack, float* damage) {
    PlayState* play = gPlayState;
    Player* player;
    DamageTable* table;
    u8 swordRow;
    u8 fireRow;

    if (play == NULL || victim == NULL || attack == NULL || !CVarGetInteger(DIN_DAMAGE_CVAR, 0) ||
        !CVarGetInteger(DIN_SWORD_CVAR, 1)) {
        return;
    }
    player = GET_PLAYER(play);
    table = victim->colChkInfo.damageTable;
    if (table == NULL || !IsPlainSwordHit(player, attack) || !IsBladeOut(player) ||
        player->currentSwordItemId == ITEM_NONE) {
        return;
    }
    swordRow = table->table[TopBit(attack->toucher.dmgFlags)];
    fireRow = table->table[FIRE_ROW];
    if (!(fireRow & 0xF)) {
        return; // the enemy has no fire reaction to borrow
    }

    // Anubis ignores sword damage and only dies to fire; a Freezard's zero-damage Kokiri row is inert.
    if (victim->id == ACTOR_EN_ANUBICE ||
        ((swordRow == 0 || (victim->id == ACTOR_EN_FZ && !(swordRow & 0xF))) && (fireRow & 0xF))) {
        *damage = fireRow & 0xF;
        victim->colChkInfo.damageEffect = fireRow >> 4 & 0xF;
        return;
    }
    // These share the ordinary damage and recoil path with their fire reaction: keep the sword's power and add the
    // enemy's own fire effect. Other effects (Baba cutting, jellyfish shock, Armos kill...) are not interchangeable.
    if ((swordRow >> 4) == 0) {
        switch (victim->id) {
            case ACTOR_EN_WF:
            case ACTOR_EN_WALLMAS:
            case ACTOR_EN_FLOORMAS:
            case ACTOR_EN_CROW:
            case ACTOR_EN_DEKUNUTS:
            case ACTOR_EN_PEEHAT:
            case ACTOR_EN_FIREFLY:
                victim->colChkInfo.damageEffect = fireRow >> 4 & 0xF;
                break;
            default:
                break;
        }
    }
}

static void RegisterToggle(const char* label, const char* cvar, const char* tooltip, int32_t defaultValue) {
    SOHModMenuWidget widget = { sizeof(SOHModMenuWidget) };

    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = label;
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    widget.defaultInt = defaultValue;
    sApi->RegisterMenuWidget(&widget);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, Burn);
    SOH_REGISTER_HOOK(sApi, OnCollisionResolveDamage, BurnOnHit);
    if (SOH_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        RegisterToggle("Din's Fire Shield", DIN_SHIELD_CVAR, "Flames wreathe the shield while you guard.", 1);
        RegisterToggle("Din's Fire Sword", DIN_SWORD_CVAR, "Flames run along the drawn blade.", 1);
        RegisterToggle("Din's Fire Sword Damage", DIN_DAMAGE_CVAR,
                       "Direct sword hits use the enemy's Fire Arrow reaction. Changes damage and immunities; does "
                       "not add a projectile or change reach.",
                       0);
        RegisterToggle("Din's Fire Charge Sound", DIN_SFX_CVAR, "Play the Fire Arrow charge sound while guarding.", 0);
    }
}
