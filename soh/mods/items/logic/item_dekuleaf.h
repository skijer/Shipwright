/**
 * Deku Leaf Item Header
 * Toggle item with dual functionality: gliding (air) and wind blow (ground)
 */

#ifndef ITEM_DEKULEAF_H
#define ITEM_DEKULEAF_H

#include "z64.h"
#include "../custom_items.h"

// Modes
#define DEKULEAF_MODE_INACTIVE 0
#define DEKULEAF_MODE_GLIDING 1
#define DEKULEAF_MODE_BLOWING 2

// Physics — floaty paraglider descent + gentle forward glide. Skijer's NEI
#define DEKULEAF_FALL_VELOCITY -1.0f  // slow, floaty descent (was -1.5)
#define DEKULEAF_GLIDE_FWD_SPEED 6.0f // gentle forward drift while gliding (paraglider momentum)
// Canopy placement above the two-hand grip (paraglider look) — dialed in live in MM, then baked.
#define DEKULEAF_GLIDE_HAND_OFFSET 6.0f
#define DEKULEAF_GLIDE_SCALE 0.16f

// Magic costs
#define DEKULEAF_GLIDE_MAGIC_INTERVAL 7
#define DEKULEAF_GLIDE_MAGIC_COST 1
#define DEKULEAF_BLOW_MAGIC_COST 3

// Blow effect
#define DEKULEAF_BLOW_RANGE 170.0f // horizontal reach of the gust
#define DEKULEAF_BLOW_FORCE 26.0f  // horizontal push speed (linear, NO height) — Skijer's NEI
#define DEKULEAF_BLOW_DURATION 60
#define DEKULEAF_WIND_SPAWN_RATE 2
#define DEKULEAF_BLOW_SPEED 2.0f      // anim playback (2x fast) — Skijer's NEI
#define DEKULEAF_BLOW_ACTIVE_FRAMES 6 // update ticks the gust stays live

// Wind AT collider (DMG_DEKU_NUT native stun) — placed in front of Link during the gust. Skijer's NEI
#define DEKULEAF_COL_RADIUS 55
#define DEKULEAF_COL_HEIGHT 60
#define DEKULEAF_COL_FORWARD 70.0f // distance in front of Link

// =============================================================================
// Scale settings for Deku Leaf in hand
// =============================================================================
#define DEKULEAF_HOLD_SCALE 0.08f   // Small scale when held in hand
#define DEKULEAF_ATTACK_SCALE 0.25f // Large scale during attack frames 10-22

// Frame range the leaf is drawn big during the swing — in ANIMATION frames, so it tracks the swing
// no matter the playback speed. Skijer's NEI
#define DEKULEAF_ATTACK_FRAME_START 10.0f
#define DEKULEAF_ATTACK_FRAME_END 22.0f

// Animation timings (blow animation is now 39 frames from skeletal anim)
#define DEKULEAF_BLOW_ANIM_FRAMES 39

// Blow effect frame — in ANIMATION frames (0..39 of the blow anim), so it stays in sync
// regardless of playback speed. Skijer's NEI
#define DEKULEAF_BLOW_EFFECT_FRAME 15.0f

// Sound
#define DEKULEAF_SOUND_WIND NA_SE_PL_MAGIC_WIND_NORMAL
#define DEKULEAF_SOUND_BLOW NA_SE_EV_WIND_TRAP

// Animation reference
#define DEKULEAF_ANIM_GLIDE gPlayerAnim_link_normal_carryB_wait

// State aliases
#define dlActive gCustomItemState.dekuLeafActive
#define dlMode gCustomItemState.dekuLeafMode
#define dlGliding gCustomItemState.dekuLeafGliding
#define dlBlowing gCustomItemState.dekuLeafBlowing
#define dlAnimTimer gCustomItemState.dekuLeafAnimTimer
#define dlBlowTimer gCustomItemState.dekuLeafBlowTimer
#define dlCollider gCustomItemState.dekuLeafCollider // wind AT collider — Skijer's NEI

// =============================================================================
// Blow animation is loaded from soh.o2r (see anim/nei_anims.h)
// =============================================================================
// (animation now loads from soh.o2r — see anim/nei_anims.h)

// =============================================================================
// Functions
// =============================================================================
void Handle_DekuLeaf(Player* player, PlayState* play);
s32 Player_UpperAction_DekuLeaf(Player* player, PlayState* play);
void CustomItems_DrawDekuLeaf(Player* player, PlayState* play);

#endif // ITEM_DEKULEAF_H
