#ifndef PLAYER_ANIM_OVERRIDE_H
#define PLAYER_ANIM_OVERRIDE_H

// Skijer's NEI: site IDs for VB_PLAYER_ANIM_OVERRIDE (handler: RegisterPlayerAnimOverrideNEI).
// Included from both C (z_player.c) and C++ (customequipment.cpp).
typedef enum {
    VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_WAIT,  // func_80835884: Zora boomerang throw-wait
    VB_PLAYER_ANIM_SITE_ZORA_BOOMERANG_CATCH, // func_80835B60: Zora boomerang catch
    VB_PLAYER_ANIM_SITE_ROLL,                 // Player_SetupRoll: roll (MHR moveId 4)
    VB_PLAYER_ANIM_SITE_DODGE_HOP,            // func_8083BCD0: dodge hop; siteArg = direction 0..3
    VB_PLAYER_ANIM_SITE_SHIELD_RAISE,         // shield raise
    VB_PLAYER_ANIM_SITE_SHIELD_LOOP,          // Player_Action_80843188: shield hold loop
    VB_PLAYER_ANIM_SITE_JUMPSLASH_RECOVERY,   // jump-slash recovery
    VB_PLAYER_ANIM_SITE_FALL_WAIT,            // free fall: gPlayerAnim_link_normal_landing_wait
    VB_PLAYER_ANIM_SITE_MELEE_SWING,          // func_80837948: any swing; siteArg = PLAYER_MWA_*
} VBPlayerAnimOverrideSite;

#endif // PLAYER_ANIM_OVERRIDE_H
