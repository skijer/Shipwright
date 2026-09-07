/**
 * item_rod_of_seasons.c — Rod of Seasons (Skijer's NEI)
 *
 * Four seasons share ONE page-2 cell (SLOT_ROD_OF_SEASONS) behind ONE ext item id
 * (EXT_ITEM_ROD_OF_SEASONS), the Sheikah Slate idiom: ownership is the NeiSaveData.seasonsOwned
 * bitmask, one sibling pickup per season, and NeiSaveData.season is the live one.
 *
 * CONTROLS
 * --------
 *   C (the button holding the rod) : FIRST press draws the staff into Link's hand (the Cane of
 *                                    Somaria idiom); every press after that opens the change-season
 *                                    prompt — A confirms, B cancels, unowned seasons draw grey and
 *                                    the cursor skips them
 *
 * Confirming a DIFFERENT season reloads the scene in place, the Time Gate's trick, so actors and
 * scene behaviour re-init under the new season instead of waiting for the next room change.
 *
 * The season is world state, not a cast: this file pushes it into envCtx every frame, which is also
 * what re-applies it after that reload (Environment_Init zeroes the whole context).
 */

#include "global.h"
#include "mods/extended_inventory.h"
#include "mods/items/helpers/equip_helper.h"
// box_menu.c is unity-included just before this file in custom_items.c, so its BoxMenu_*
// declarations are already in scope — it has no header (see the note at its top).

// Ext-button store: which u16 item a button really holds when it shows ITEM_EXT_BUTTON.
extern u16 ExtButton_GetItem(s32 btn);

// Precipitation targets. Rain matches the Kakariko thunderstorm's density (without its thunder) and
// snow the vanilla flurry; Spring is thinner because its particles are drifting sprites, not weather.
#define SEASON_RAIN_DROPS 30
#define SEASON_SNOWFLAKES 64
#define SEASON_SPRITES 32

// Object_Kankyo params. Spring wears Kokiri Forest's own atmosphere — the drifting fairy sprites —
// which is the same particle system the snow uses, so the two can never be on screen together.
#define OBJECT_KANKYO_FAIRIES 0
#define OBJECT_KANKYO_SNOW 3

// Drains the scene's water in Summer and hands it back otherwise (season_scene.cpp). Called every
// frame, season included in its own decision, so the restore has one place to happen too.
extern void SeasonScene_UpdateWater(PlayState* play);
extern void SeasonScene_UpdateWasteland(PlayState* play);

static u8 sRodDrawn = 0; // the staff is out, in Link's hand
static s8 sRodPrevInvinc = 0;

// Is the rod currently in Link's hand? Read by the in-hand draw.
u8 Seasons_IsDrawn(void) {
    return sRodDrawn;
}

// Winter freezes every water surface, so Link walks it. Answered here and OR'd into
// RocBoots_WalksOnWater, which is the one gate for that ability — the Garo form joins the same way.
// Its own conditions keep the water swimmable underneath: the pin only takes while Link is NOT
// already in the water and is not moving up.
u8 Seasons_WalksOnWater(void) {
    return (Seasons_SeasonCount() != 0) && (Seasons_GetSeason() == SEASON_WINTER);
}

// ============================================================================
// WEATHER
// ============================================================================

// The vanilla snow grey, which is what every season but Spring draws with.
#define SEASON_SNOW_GREY 200

// Spring's drifting sprites are pale green, the Kokiri Forest look. Deliberately not
// Seasons_SeasonColor: that is the coin's identity colour, far too saturated for a particle.
static const u8 sSpringSpriteTint[3] = { 190, 245, 200 };

/**
 * Tint for the Object_Kankyo particles, read from its draw. Spring's sprites ride the same particle
 * system the snow does, so the colour is the only thing separating the two.
 */
void Seasons_PrecipTint(u8* r, u8* g, u8* b) {
    if ((Seasons_SeasonCount() != 0) && (Seasons_GetSeason() == SEASON_SPRING)) {
        *r = sSpringSpriteTint[0];
        *g = sSpringSpriteTint[1];
        *b = sSpringSpriteTint[2];
        return;
    }
    *r = *g = *b = SEASON_SNOW_GREY;
}

// Is the rod currently the thing driving the particle count? Object_Kankyo's fairies keep THEIR
// count in that same field, so a season with no particles must hand it back instead of pinning it
// to zero — otherwise the rod deletes the fairies in every scene that has them.
static u8 sOwnsPrecip = 0;

/**
 * Snow and blossom are drawn by Object_Kankyo, never by the engine — setting the flake count with no
 * such actor in the scene draws nothing at all. Spawning while nothing is falling is the vanilla
 * "Let It Snow" idiom; the actor's own init kills the duplicate.
 *
 * KNOWN GAP: that init keeps ONE Object_Kankyo of any kind, so in the three fairy scenes (Kokiri
 * Forest, Lost Woods, Sacred Forest Meadow) the fairies hold the slot and Winter falls dry there.
 */
static void Seasons_TakePrecip(PlayState* play, u8 count, s16 kankyoType) {
    play->envCtx.unk_EE[3] = count;
    sOwnsPrecip = 1;
    if (play->envCtx.unk_EE[2] == 0) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_OBJECT_KANKYO, 0, 0, 0, 0, 0, 0, kankyoType);
    }
}

// Drain what the previous season left falling, then stop touching the field at all.
static void Seasons_ReleasePrecip(PlayState* play) {
    if (!sOwnsPrecip) {
        return;
    }
    play->envCtx.unk_EE[3] = 0;
    if (play->envCtx.unk_EE[2] == 0) {
        sOwnsPrecip = 0;
    }
}

// Same shared-field problem as the particle count: the scene's own weather tags write the rain
// count and the sky mode too, so the rod may only undo what the rod set.
static u8 sOwnsRain = 0;
static u8 sOwnsSky = 0;

// gloomySkyMode is a two-step handshake: 1 darkens, 2 hands it to the restore branch, which zeroes
// it itself. Writing 2 unconditionally would strand it non-zero.
static void Seasons_TakeSky(EnvironmentContext* env) {
    env->gloomySkyMode = 1;
    sOwnsSky = 1;
}

static void Seasons_ReleaseSky(EnvironmentContext* env) {
    if (!sOwnsSky) {
        return;
    }
    if (env->gloomySkyMode == 1) {
        env->gloomySkyMode = 2;
    }
    sOwnsSky = 0;
}

// Per-channel offsets onto the scene's own light, so they shade every scene without knowing
// anything about it — the trick the fishing pond's storm uses (z_fishing.c:5808). Kept gentle: this
// is meant to read as an overcast sky, not as nightfall.
//
// Winter takes red down hardest and barely touches blue, which is what leaves the cold blue-grey
// cast rather than a flat dimming. Ambient goes a little further than the directional light so
// shadows deepen instead of the whole scene greying out evenly.
typedef struct {
    s16 light[3];
    s16 ambient[3];
} SeasonShade;

static const SeasonShade sSeasonShade[SEASON_COUNT] = {
    { { 0, 0, 0 }, { 0, 0, 0 } },             // Spring — clear
    { { 0, 0, 0 }, { 0, 0, 0 } },             // Summer — clear
    { { -34, -32, -26 }, { -42, -40, -34 } }, // Autumn — overcast, a touch warm
    { { -46, -34, -14 }, { -54, -42, -20 } }, // Winter — blue-grey
};

// Applied every frame: Environment_Update resets these each pass.
static void Seasons_ShadeOvercast(EnvironmentContext* env, s32 season) {
    const SeasonShade* shade = &sSeasonShade[(season < SEASON_COUNT) ? season : SEASON_SPRING];

    for (s32 i = 0; i < 3; i++) {
        env->adjLight1Color[i] = shade->light[i];
        env->adjAmbientColor[i] = shade->ambient[i];
    }
}

static void Seasons_TakeRain(EnvironmentContext* env, u8 drops) {
    env->unk_EE[0] = drops;
    sOwnsRain = 1;
}

static void Seasons_ReleaseRain(EnvironmentContext* env) {
    if (!sOwnsRain) {
        return;
    }
    env->unk_EE[0] = 0;
    sOwnsRain = 0;
}

// The rain and thunder tracks belong to the nature ambience sequence, which shares the main BGM
// player — starting it would replace the scene's music. A season lasts, so the rain is voiced as a
// plain looping sound effect instead.
static void Seasons_PlayRainSfx(void) {
    Sfx_PlaySfxCentered(NA_SE_EV_RAIN - SFX_FLAG);
}

/**
 * Pushes the active season into the environment context. Every write is idempotent, so this runs
 * unconditionally each frame and needs no "what changed" bookkeeping — which is exactly what makes
 * the season survive a scene load, where Environment_Init zeroes all of it.
 *
 * Autumn and Winter darken the world two ways, because neither reaches everywhere alone:
 * gloomySkyMode redraws the SKYBOX but bows out while gSkyboxBlendingEnabled is set, and the
 * lighting offsets shade the GEOMETRY in every scene regardless. Owning gloomySkyMode is also what
 * keeps the scene's own weather tags from firing, which is why season_scene.cpp retires them.
 */
static void Seasons_ApplyWeather(PlayState* play, u8 season) {
    EnvironmentContext* env = &play->envCtx;

    // The blank coin, and anywhere with a roof over it — without this the rod rains inside every
    // dungeon and shop. Only `indoors` is tested: gating on skyboxId as well was too strict, since
    // plenty of open-air scenes ship a skybox that is not SKYBOX_NORMAL_SKY.
    if ((season == SEASON_OFF) || env->indoors) {
        Seasons_ReleaseRain(env);
        Seasons_ReleasePrecip(play);
        Seasons_ShadeOvercast(env, SEASON_SPRING); // the clear row: no offsets at all
        return;
    }

    Seasons_ShadeOvercast(env, season);

    switch (season) {
        case SEASON_SPRING:
            Seasons_ReleaseRain(env);
            Seasons_TakePrecip(play, SEASON_SPRITES, OBJECT_KANKYO_FAIRIES);
            Seasons_ReleaseSky(env);
            break;
        case SEASON_SUMMER:
            Seasons_ReleaseRain(env);
            Seasons_ReleasePrecip(play);
            Seasons_ReleaseSky(env);
            break;
        case SEASON_AUTUMN:
            // Particles on screen suppress the engine's whole rain pass, so the rain can only start
            // once the previous season's flakes have drained out.
            Seasons_ReleasePrecip(play);
            Seasons_TakeRain(env, SEASON_RAIN_DROPS);
            Seasons_TakeSky(env);
            Seasons_PlayRainSfx();
            // Env flag 5 is what the Song of Storms raises, and what a bean sprout watches for
            // before it shoots up and drops its fairies. Autumn rains, so the beans grow on their
            // own rather than being pushed into that state from outside.
            Flags_SetEnv(play, 5);
            break;
        case SEASON_WINTER:
            Seasons_ReleaseRain(env);
            Seasons_TakePrecip(play, SEASON_SNOWFLAKES, OBJECT_KANKYO_SNOW);
            Seasons_TakeSky(env);
            break;
        default:
            break;
    }
}

// ============================================================================
// INPUT TICK — C draws the rod, the next press opens the change-season prompt
// ============================================================================

/**
 * Reloads the scene around Link without moving him, so actors and scene behaviour re-init under the
 * new season. The age-switch idiom: respawnFlag + RESPAWN_MODE_DOWN is what preserves the position;
 * a plain entrance reload would drop him back at the scene's spawn point.
 */
static void Seasons_ReloadSceneInPlace(PlayState* play, Player* player) {
    // RESPAWN_MODE_DOWN is the pit-fall path, so vanilla would charge Link the void-out damage for
    // turning the season. season_scene.cpp arms a one-shot VB to waive it.
    extern void SeasonScene_SuppressVoidDamage(void);

    SeasonScene_SuppressVoidDamage();
    gSaveContext.respawnFlag = 1;
    play->nextEntranceIndex = gSaveContext.entranceIndex;

    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = play->nextEntranceIndex;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = play->roomCtx.curRoom.num;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = player->actor.world.pos;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = player->actor.shape.rot.y;

    if (play->roomCtx.curRoom.behaviorType2 < 4) {
        gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0x0DFF;
    } else {
        // Static-background rooms ride a fixed camera that has to be restored with them.
        gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0x0D00 | GET_ACTIVE_CAM(play)->camDataIdx;
    }

    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_INSTANT;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;
}

// Set by the confirm callback, acted on in the tick: the menu closes and unpauses BEFORE its
// callback runs, and the reload must be requested from a normal frame.
static u8 sSeasonReloadPending = 0;

// The prompt's confirm: the highlighted season becomes the active one.
static void Seasons_OnWheelConfirm(s32 index) {
    if (Seasons_GetSeason() == (u8)index) {
        return; // picked the season already running; nothing to re-init
    }
    Seasons_SetSeason((u8)index); // no-op if that season is not owned
    sSeasonReloadPending = (Seasons_GetSeason() == (u8)index);
}

// Fills the row with ALL slots — locked seasons included, drawn grayed and unselectable, so the row
// doubles as a reminder of what is still missing (the slate's rune row does the same). The blank
// coin is last and always selectable.
static s32 Seasons_BuildWheel(BoxMenuEntry* out) {
    for (s32 s = 0; s < SEASON_SLOTS; s++) {
        out[s].iconPath = (const char*)Seasons_SeasonIcon((u8)s);
        out[s].iconSize = 32;
        out[s].enabled = Seasons_SeasonOwned((u8)s);
    }
    return SEASON_SLOTS;
}

// Every C button currently holding the rod. C items live in the flat button array at 1..3.
static u16 Seasons_EquippedButtonMask(void) {
    u16 mask = 0;

    if (ExtButton_GetItem(1) == EXT_ITEM_ROD_OF_SEASONS) {
        mask |= BTN_CLEFT;
    }
    if (ExtButton_GetItem(2) == EXT_ITEM_ROD_OF_SEASONS) {
        mask |= BTN_CDOWN;
    }
    if (ExtButton_GetItem(3) == EXT_ITEM_ROD_OF_SEASONS) {
        mask |= BTN_CRIGHT;
    }
    return mask;
}

// Put it away. Idempotent, so every blocking path can call it unconditionally.
static void Seasons_Stow(PlayState* play, Player* player) {
    if (!sRodDrawn) {
        return;
    }
    sRodDrawn = 0;
    ItemEquip_PlayUnequipSFX(play, player);
}

/**
 * Per-frame rod input, called from Player_UpdateCommon. The weather runs off ownership alone: once a
 * season is owned the world is in it, whether or not the rod sits on a button.
 */
void Seasons_TickInput(PlayState* play, Player* player) {
    BoxMenuEntry entries[SEASON_SLOTS];
    u16 btnMask;

    if (Seasons_SeasonCount() == 0) {
        return;
    }

    Seasons_ApplyWeather(play, Seasons_GetSeason());
    // Outside ApplyWeather's indoor guard on purpose: frozen water is not sky weather, so the
    // Water Temple and every other indoor pool freeze too.
    SeasonScene_UpdateWater(play);
    SeasonScene_UpdateWasteland(play);

    if (sSeasonReloadPending) {
        sSeasonReloadPending = 0;
        Seasons_Stow(play, player);
        Seasons_ReloadSceneInPlace(play, player);
        return;
    }

    if (BoxMenu_IsOpen()) {
        return; // the menu owns the frame (and the game is paused anyway)
    }

    btnMask = Seasons_EquippedButtonMask();
    if (btnMask == 0) {
        Seasons_Stow(play, player); // taken off the button while it was out
        return;
    }

    // Anything that would look wrong with a staff in hand puts it away.
    if (ItemInput_IsBlocked(player, play) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        (player->meleeWeaponState != 0)) {
        Seasons_Stow(play, player);
        return;
    }
    if (ItemInput_CheckDamage(player, &sRodPrevInvinc)) {
        Seasons_Stow(play, player);
        return;
    }

    if (play->state.input[0].press.button & btnMask) {
        if (!sRodDrawn) {
            // Equip only — the press that draws the rod never also opens the prompt, same as the
            // cane and the slate.
            sRodDrawn = 1;
            ItemEquip_PlayEquipSFX(play, player);
            return;
        }
        // Latched (holdButton 0): opened on a press, so A confirms and B cancels.
        BoxMenu_Open(play, entries, Seasons_BuildWheel(entries), Seasons_GetSeason(), 0, Seasons_OnWheelConfirm);
    }
}

// Unity include, like the cane and the slate: keeps the in-hand draw out of the build files.
#include "../objects/object_rod_of_seasons.c"
