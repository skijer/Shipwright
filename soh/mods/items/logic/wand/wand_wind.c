/**
 * wand_wind.c — Tornado Rod (Skijer's NEI).
 *
 * A toggle, not a cast: two green tornadoes spin at Link's boots and every launch off the ground
 * comes out harder. It burns magic for as long as it is lit, and dies on damage, on an empty meter,
 * or on another press.
 *
 * The boost is applied where the engine writes the launch velocity, not where the jump is decided —
 * that is one place per launch kind instead of one per input, and it is the same pair of hooks the
 * Rito form already uses.
 */

#include "../../objects/object_tornado.h" // header ONLY: the .cpp is its own TU (LNK1179)

#define WIND_BOOST 1.55f
#define WIND_HOVER_RISE 4.0f   // held: the rise is pinned here instead of decaying into gravity
#define WIND_DRAIN_INTERVAL 20 // frames between each point of magic
#define WIND_DRAIN_COST 1

// Tip at the boot, mouth above it — the Rito updraft's convention, and the only pitch value that
// stands the cone upright.
#define WIND_TORNADO_PITCH (-0x4000)
#define WIND_TORNADO_LENGTH 34.0f
#define WIND_TORNADO_RADIUS 11.0f
#define WIND_TORNADO_SPIN 0x1800
#define WIND_TORNADO_SCROLL 18 // streaks travelling UP the column, in quarter-texels

// Forest green, and bright: the cone takes ALL of its colour from the primitive.
#define WIND_TINT_R 90
#define WIND_TINT_G 225
#define WIND_TINT_B 110
#define WIND_TINT_A 190

static u8 sWindOn = 0;
static s16 sWindDrainTimer = 0;
static s8 sWindPrevInvinc = 0;
static TornadoParams sWindCone[2];

static void WandWind_Stop(PlayState* play, Player* player) {
    if (!sWindOn) {
        return;
    }
    sWindOn = 0;
    sWindDrainTimer = 0;
    ItemEquip_PlayUnequipSFX(play, player);
}

// Cast = toggle. Lighting it costs nothing up front; the meter is spent while it burns.
u8 WandWind_Cast(Player* player, PlayState* play) {
    if (sWindOn) {
        WandWind_Stop(play, player);
        return 1;
    }
    if (!ItemMagic_HasEnough(play, WIND_DRAIN_COST)) {
        return 0;
    }
    sWindOn = 1;
    sWindDrainTimer = WIND_DRAIN_INTERVAL;
    ItemEquip_PlayEquipSFX(play, player);
    return 1;
}

void WandWind_Tick(PlayState* play, Player* player) {
    if (!sWindOn) {
        sWindPrevInvinc = player->invincibilityTimer;
        return;
    }

    if (ItemInput_CheckDamage(player, &sWindPrevInvinc)) {
        WandWind_Stop(play, player);
        return;
    }
    if (--sWindDrainTimer > 0) {
        return;
    }
    sWindDrainTimer = WIND_DRAIN_INTERVAL;
    if (!ItemMagic_HasEnough(play, WIND_DRAIN_COST)) {
        WandWind_Stop(play, player);
        return;
    }
    ItemMagic_Consume(play, WIND_DRAIN_COST);
}

/**
 * Multiply whatever vertical launch just happened. Called from z_player.c the instant the engine
 * writes velocity.y, so it covers the plain jump, the side hops, the backflip and the jump slash
 * without knowing which of them ran.
 *
 * PLAYER_STATE2_HOPPING is cleared for the same reason the Rito's hop clears it: a boosted hop that
 * keeps the flag can no longer grab a ledge on the way up.
 */
void WandWind_Boost(Player* player) {
    if (!sWindOn || (player->actor.velocity.y <= 0.0f)) {
        return;
    }
    player->actor.velocity.y *= WIND_BOOST;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
}

/**
 * Holding the button with the wind lit keeps Link rising instead of letting gravity win, so he
 * floats. Used out of a boosted hop or a jump slash it turns that launch into flight.
 *
 * Only ever raises: a dive is still a dive, and the ground clamp stays untouched.
 */
void WandWind_TickHover(Player* player, u8 held) {
    if (!sWindOn || !held) {
        return;
    }
    if (player->actor.velocity.y < WIND_HOVER_RISE) {
        player->actor.velocity.y = WIND_HOVER_RISE;
    }
}

// Drawn from the wand's draw hook, never from the tick: the gust jar documents why (the tornado
// emits into POLY_XLU and the update pass has no display list open).
void WandWind_Draw(Player* player, PlayState* play) {
    static const u8 kBootParts[2] = { PLAYER_BODYPART_L_FOOT, PLAYER_BODYPART_R_FOOT };

    if (!sWindOn) {
        return;
    }
    for (u8 i = 0; i < ARRAY_COUNT(kBootParts); i++) {
        TornadoParams* cone = &sWindCone[i];

        cone->origin = player->bodyPartsPos[kBootParts[i]];
        cone->yaw = player->actor.shape.rot.y;
        cone->pitch = WIND_TORNADO_PITCH;
        cone->length = WIND_TORNADO_LENGTH;
        cone->radius = WIND_TORNADO_RADIUS;
        cone->color.r = WIND_TINT_R;
        cone->color.g = WIND_TINT_G;
        cone->color.b = WIND_TINT_B;
        cone->color.a = WIND_TINT_A;
        cone->spin += WIND_TORNADO_SPIN;
        Tornado_AdvanceScroll(cone, 0, WIND_TORNADO_SCROLL);
        Tornado_Draw(play, cone);
    }
}
