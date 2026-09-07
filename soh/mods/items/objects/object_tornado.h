/**
 * object_tornado.h - Shared wind-cone visual (Skijer's NEI)
 *
 * The mesh is SM64's Tornado/Whirlpool map object converted to a custom asset
 * (assets/custom/objects/object_nei_tornado). Its texture is pure intensity+alpha, so the
 * cone takes ALL of its colour from the primitive colour — pass the damage type's colour and
 * the tornado comes out in that colour.
 *
 * Baked local space: tip at the origin, mouth opening toward +Y, TORNADO_MODEL_LENGTH long.
 * Tornado_Draw orients and scales it, so callers just say where/which way/how big.
 *
 * Not gust-jar specific on purpose — any item that wants a wind cone can use it.
 */

#ifndef OBJECT_TORNADO_H
#define OBJECT_TORNADO_H

#include "z64.h"

// The implementation is a .cpp so the mods/*.cpp glob compiles it as its OWN translation unit.
// It deliberately does NOT live in the z_player.c unity build: that TU is already enormous and
// adding to it brought back `z_player.obj : LNK1179 duplicate COMDAT` (see
// mods/items/logic/custom_items.c for the same story with snap.c).
#ifdef __cplusplus
extern "C" {
#endif

// Dimensions the mesh was baked at (see scratchpad gen_tornado.py).
#define TORNADO_MODEL_LENGTH 100.0f
#define TORNADO_MODEL_RADIUS 47.0f

// Texture size, and the scroll wrap points in quarter-texels (Gfx_TexScroll's unit). Both axes
// are G_TX_WRAP in the material, so keeping the offsets inside one texture period makes the
// scroll seamless.
#define TORNADO_TEX_WIDTH 32
#define TORNADO_TEX_HEIGHT 64
#define TORNADO_SCROLL_S_WRAP (TORNADO_TEX_WIDTH * 4)
#define TORNADO_SCROLL_T_WRAP (TORNADO_TEX_HEIGHT * 4)

// Spiral ribbons that wrap the cone. 6 is comfortable: MM and OoT both have BLURE_COUNT 25.
#define TORNADO_RIBBON_MAX 6
// Turns each ribbon makes between the tip and the mouth, in binary angle.
#define TORNADO_RIBBON_TURNS 0x18000
// Fraction of the spiral a ribbon advances per frame (1/14 -> a full trace every 14 frames).
#define TORNADO_RIBBON_STEP (1.0f / 14.0f)

typedef struct {
    Vec3f origin;      // cone tip — e.g. the gust jar nozzle
    s16 yaw;           // aim direction
    s16 pitch;         // aim pitch (same sign convention as the gust jar nozzle maths)
    f32 length;        // world length from tip to mouth
    f32 radius;        // world radius at the mouth
    Color_RGBA8 color; // damage-type colour; .a is the global fade
    s16 spin;          // roll about the cone axis, advanced by the caller each frame
    // Texture scroll, in quarter-texels, advanced by the caller. This is what makes the streaks
    // travel over the surface instead of the cone just rotating as a rigid body.
    //   scrollT > 0 -> the pattern flows from the tip toward the mouth (outward, "blowing")
    //   scrollT < 0 -> it flows back into the tip (inward, "sucking")
    // Use Tornado_AdvanceScroll to step them; it keeps both inside one texture period.
    s16 scrollS; // around the cone
    s16 scrollT; // along the cone's length
} TornadoParams;

// Per-instance ribbon state. Zero-initialise it; Tornado_RibbonsUpdate starts them on demand.
typedef struct {
    s32 blureIdx[TORNADO_RIBBON_MAX];
    f32 t[TORNADO_RIBBON_MAX];
    u8 count;
    u8 active;
    Color_RGBA8 color; // colour the blures were created with; a change restarts them
} TornadoRibbons;

/** Draw the cone. Emits to the XLU list; safe to call before the asset exists (no-ops). */
void Tornado_Draw(PlayState* play, const TornadoParams* p);

/**
 * Unit vector the cone points along for a given aim. Exposed so gameplay volume tests can use
 * the EXACT axis the tornado is drawn along instead of re-deriving it and drifting.
 */
void Tornado_GetAxis(s16 yaw, s16 pitch, Vec3f* axis);

/**
 * Step the texture scroll by the given quarter-texel deltas, wrapping into one texture period so
 * the offsets never drift out of Gfx_TexScroll's range.
 */
void Tornado_AdvanceScroll(TornadoParams* p, s16 stepS, s16 stepT);

/**
 * Advance and feed the spiral ribbons. Call once per frame while the tornado is up; it starts
 * the blures itself on the first call and restarts them if the colour changed.
 * @param count how many ribbons (clamped to TORNADO_RIBBON_MAX)
 */
void Tornado_RibbonsUpdate(PlayState* play, TornadoRibbons* rb, const TornadoParams* p, u8 count);

/** Release the ribbon blures. Call when the effect stops. */
void Tornado_RibbonsStop(PlayState* play, TornadoRibbons* rb);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_TORNADO_H
