/**
 * item_cane_of_somaria.c — Dual Cane: Cane of Somaria + Cane of Pacci.
 *
 * See item_cane_of_somaria.h for the full control scheme. In short:
 *
 *   A on the kaleido cell       -> switch cane
 *   the equipped C button       -> draw the cane, then cast the active skill
 *   L / R with the cane drawn   -> step the summon
 *
 *   Somaria (red)     Statue  Block  Platform
 *   Pacci   (yellow)  Flip    Stone  Ultrahand
 *
 * Statue drops at Link's own feet. Block and Platform are AIMED: a translucent
 * ghost follows the CAMERA's facing (blue = placeable, red = blocked) and the
 * cast only fires on a blue ghost. The Pacci skills act on whatever enemy /
 * object Link is looking at, through the shared target selector.
 *
 * Ownership is six independent bits in NeiSaveData.caneSkills — six separate
 * obtainable items sharing this one inventory slot, gettable in any order.
 * Skijer's NEI
 */

#include "z64.h"
#include "item_cane_of_somaria.h"
#include "../custom_items.h"
#include "../helpers/equip_helper.h"
#include "../helpers/fx_helper.h"
#include "../helpers/combat_helper.h"      // cane_pacci.c's carried-flame collider
#include "objects/object_ru1/object_ru1.h" // cane_pacci.c tells sitting Ruto from standing Ruto
// cane_pacci.c reads one field out of each of these. The headers rather than hand-computed offsets:
// the /* 0x01F8 */ comments in them are ORIGINAL N64 offsets and are wrong for this build - Actor,
// ColliderCylinder and every pointer are wider on 64-bit, so everything after them has moved. Only
// the compiler knows where these fields actually are.
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Ru1/z_en_ru1.h"
#include "overlays/actors/ovl_En_Siofuki/z_en_siofuki.h"
#include "overlays/actors/ovl_Bg_Spot00_Hanebasi/z_bg_spot00_hanebasi.h" // the drawbridge's own hinge
// trirod_echoes.inc.c's Brazier scan gate reads ObjSyokudai.litTimer.
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"

#include "../../nei_save.h"
#include "../../extended_inventory.h"

// Summon system (Somaria) and enemy/object effects (Pacci). Both are consumed by
// #include so they land in this translation unit — neither .c is in the vcxproj.
#include "../../actors/somaria_cubes.h"
#include "../../actors/somaria_cubes.c"
// The Flip cast visual is a STUB awaiting implementation — see pacci_flip_vfx.h.
// It is included BEFORE cane_pacci.c because Pacci_CastFlip calls into it.
#include "../../actors/pacci_flip_vfx.h"
#include "../../actors/pacci_flip_vfx.c"
#include "../../actors/cane_pacci.h"
#include "../../actors/cane_pacci.c"

// Harpoon Prop Hunt active check (C bridge from PropHunt.cpp). Forward-declared
// here to avoid pulling the C++ header into this C file.
extern s32 HarpoonPropHunt_IsActive(void);

// The Trirod expansion: Echoes of Wisdom's Tri Rod as the Somaria chain's level 3.
// Consumed by #include like the Pacci files — nothing here is a vcxproj entry. The
// header goes in before the object draw below, which calls Trirod_DrawPreview.
#include "../../../expansions/trirod/trirod.h"
#include "../../../expansions/trirod/trirod_echoes.inc.c"
#include "../../../expansions/trirod/trirod.c"

// Include object (for draw function) — the staff model in Link's hand.
#include "../objects/object_cane_of_somaria.c"

// Include animation
#include "../anim/nei_anims.h" // cast animation loads from soh.o2r — Skijer's NEI

static ItemEquipState sSomariaEquipState = { 0 };
static s8 sSomariaPrevInvinc = 0;
// Our own edge detection for the L/R summon cycler — see the note at its use site.
static u8 sCanePrevL = 0;
static u8 sCanePrevR = 0;

// ============================================================================
// OWNERSHIP / PROGRESSION
// ============================================================================

u8 Cane_HasSkill(u8 skill) {
    return Nei_CaneHasSkill(skill);
}

u8 Cane_GetType(void) {
    return Nei_CaneGetType();
}

u8 Cane_GetActiveSkill(void) {
    return Nei_CaneActiveSkill();
}

u8 Cane_GiveSkill(u8 skill) {
    if (skill >= CANE_SKILL_MAX) {
        return 0;
    }
    if (Nei_CaneHasSkill(skill)) {
        return 0; // already owned
    }

    u8 hadCane = Nei_CaneOwned();
    Nei_CaneGrantSkill(skill);

    // The first skill of ANY kind is what puts the cane itself in the player's
    // hands — the six pickups can arrive in any order, so "first one wins".
    if (!hadCane) {
        Nei_SetOwnedItem(SLOT_CANE_OF_SOMARIA, ITEM_CANE_OF_SOMARIA);
        Nei_CaneSetType(CANE_SKILL_TYPE(skill));
        Nei_CaneSetSkillSlot(CANE_SKILL_TYPE(skill), CANE_SKILL_SLOT(skill));
    }
    return 1;
}

u8 Cane_GiveByExtItem(u16 extItemId) {
    switch (extItemId) {
        case EXT_ITEM_CANE_SOMARIA_STATUE:
            return Cane_GiveSkill(CANE_SKILL_SOMARIA_STATUE);
        case EXT_ITEM_CANE_SOMARIA_BLOCK:
            return Cane_GiveSkill(CANE_SKILL_SOMARIA_BLOCK);
        case EXT_ITEM_CANE_SOMARIA_PLATFORM:
            return Cane_GiveSkill(CANE_SKILL_SOMARIA_PLATFORM);
        case EXT_ITEM_CANE_PACCI_FLIP:
            return Cane_GiveSkill(CANE_SKILL_PACCI_FLIP);
        case EXT_ITEM_CANE_PACCI_STONE:
            return Cane_GiveSkill(CANE_SKILL_PACCI_STONE);
        case EXT_ITEM_CANE_PACCI_ULTRAHAND:
            return Cane_GiveSkill(CANE_SKILL_PACCI_ULTRAHAND);
        default:
            return 0;
    }
}

// ============================================================================
// AIM / PREVIEW
// ============================================================================

// Which summons draw a placement ghost. The statue is included even though it is
// not AIMED: it always lands at Link's own feet, and the ghost there is what tells
// you which form is about to appear before you commit to the cast.
static u8 Cane_SkillNeedsAim(u8 skill) {
    return (skill == CANE_SKILL_SOMARIA_STATUE) || (skill == CANE_SKILL_SOMARIA_BLOCK) ||
           (skill == CANE_SKILL_SOMARIA_PLATFORM);
}

static CaneSummonKind Cane_SummonKindOf(u8 skill) {
    switch (skill) {
        case CANE_SKILL_SOMARIA_BLOCK:
            return CANE_SUMMON_BLOCK;
        case CANE_SKILL_SOMARIA_PLATFORM:
            return CANE_SUMMON_PLATFORM;
        case CANE_SKILL_SOMARIA_STATUE:
        default:
            return CANE_SUMMON_STATUE;
    }
}

// Where the camera is looking, horizontally. Placement follows the CAMERA, not
// Link's body, so the ghost always lands where the player is actually looking.
static s16 Cane_CameraYaw(PlayState* play, Player* player) {
    // OoT's View names the focus point `lookAt` (MM calls the same field `at`).
    Vec3f eye = play->view.eye;
    Vec3f at = play->view.lookAt;
    f32 dx = at.x - eye.x;
    f32 dz = at.z - eye.z;

    if ((fabsf(dx) < 0.001f) && (fabsf(dz) < 0.001f)) {
        return player->actor.shape.rot.y; // degenerate view — fall back to Link
    }
    return Math_Vec3f_Yaw(&eye, &at);
}

// Recompute canePreviewPos / canePreviewYaw / canePreviewValid for this frame.
static void Cane_UpdatePreview(Player* player, PlayState* play, u8 skill) {
    CaneSummonKind kind = Cane_SummonKindOf(skill);
    s16 yaw = Cane_CameraYaw(play, player);
    Vec3f pos;

    pos.x = player->actor.world.pos.x + (Math_SinS(yaw) * CANE_PLACE_DIST);
    pos.y = player->actor.world.pos.y;
    pos.z = player->actor.world.pos.z + (Math_CosS(yaw) * CANE_PLACE_DIST);

    if (kind == CANE_SUMMON_STATUE) {
        // Not aimed: the statue drops exactly where Link is standing, so the ghost
        // sits on him rather than out in front along the camera.
        canePreviewPos = player->actor.world.pos;
        canePreviewYaw = player->actor.shape.rot.y;
        canePreviewValid = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ? 1 : 0;
        return;
    }

    if (kind == CANE_SUMMON_BLOCK) {
        // The block is a solid dynapoly object: it has to sit on real ground.
        CollisionPoly* outPoly = NULL;
        s32 bgId = BGCHECK_SCENE;
        Vec3f rayFrom = pos;

        rayFrom.y += CANE_PLACE_RAY_UP;
        f32 floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &outPoly, &bgId, &player->actor, &rayFrom);

        if ((floorY <= BGCHECK_Y_MIN) || ((pos.y - floorY) > CANE_PLACE_RAY_DOWN)) {
            canePreviewValid = 0; // nothing to stand on down there
            canePreviewPos = pos;
            canePreviewYaw = yaw;
            return;
        }
        pos.y = floorY;
    } else {
        // The platform is meant to float: it stays at Link's own height so it can
        // bridge a gap he is standing beside.
        pos.y = player->actor.world.pos.y + 10.0f;
    }

    canePreviewPos = pos;
    canePreviewYaw = yaw;
    canePreviewValid = CaneSummon_PlacementValid(play, kind, &pos);
}

u8 Cane_IsAiming(void) {
    return (shSomariaActive && Cane_SkillNeedsAim(Nei_CaneActiveSkill())) ? 1 : 0;
}

u8 Cane_PreviewValid(void) {
    return canePreviewValid;
}

// ============================================================================
// CASTING
// ============================================================================

static void Cane_PlayError(Player* p, PlayState* play) {
    Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &p->actor.world.pos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Can the active skill fire right now? (aim validity, targets in range, ...)
static u8 Cane_CanCast(Player* p, PlayState* play, u8 skill) {
    switch (skill) {
        case CANE_SKILL_SOMARIA_STATUE:
            return (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ? 1 : 0;
        case CANE_SKILL_SOMARIA_BLOCK:
        case CANE_SKILL_SOMARIA_PLATFORM:
            return canePreviewValid;
        case CANE_SKILL_PACCI_ULTRAHAND:
            return 1; // grabbing OR releasing is always a legal press
        case CANE_SKILL_PACCI_FLIP:
        case CANE_SKILL_PACCI_STONE:
        default:
            return 1; // the cast itself reports "nothing in range"
    }
}

// Fire the skill. Called from the cast animation's action frame.
static void Cane_FireSkill(Player* p, PlayState* play, u8 skill) {
    // Prop Hunt turns Link statues into fake players all over the map, which
    // wrecks the disguise system — suppress the summons, keep the animation.
    u8 propHunt = (HarpoonPropHunt_IsActive() != 0);

    switch (skill) {
        case CANE_SKILL_SOMARIA_STATUE: {
            if (propHunt) {
                break;
            }
            // User-locked: the statue appears AT Link's position, like Elegy.
            Vec3f pos = p->actor.world.pos;
            Actor* statue = CaneSummon_Spawn(play, CANE_SUMMON_STATUE, &pos, p->actor.shape.rot.y);
            if (statue != NULL) {
                Vec3f zero = { 0.0f, 0.0f, 0.0f };
                Vec3f flash = pos;
                flash.y += 20.0f;
                EffectSsBlast_SpawnWhiteShockwave(play, &flash, &zero, &zero);
            }
            break;
        }

        case CANE_SKILL_SOMARIA_BLOCK:
        case CANE_SKILL_SOMARIA_PLATFORM: {
            if (propHunt) {
                break;
            }
            CaneSummonKind kind = Cane_SummonKindOf(skill);
            Vec3f pos = canePreviewPos;
            Actor* summon = CaneSummon_Spawn(play, kind, &pos, canePreviewYaw);
            if (summon == NULL) {
                // Usually "the object bank was not resident yet" — it has been
                // requested now, so the next press lands.
                Cane_PlayError(p, play);
            } else {
                Vec3f zero = { 0.0f, 0.0f, 0.0f };
                Vec3f flash = pos;
                flash.y += 20.0f;
                EffectSsBlast_SpawnWhiteShockwave(play, &flash, &zero, &zero);
            }
            break;
        }

        case CANE_SKILL_PACCI_FLIP:
            if (!Pacci_CastFlip(play, p)) {
                Cane_PlayError(p, play);
            }
            break;

        case CANE_SKILL_PACCI_STONE:
            if (!Pacci_CastStone(play, p)) {
                Cane_PlayError(p, play);
            }
            break;

        case CANE_SKILL_PACCI_ULTRAHAND:
            if (!Pacci_CastUltrahand(play, p)) {
                Cane_PlayError(p, play);
            }
            break;

        // Not a CANE_SKILL_* bit: the echo to spawn was committed by
        // Trirod_OnPress on the button press; this is just its landing frame.
        case CANE_SKILL_TRIROD:
            Trirod_FireSummon(p, play);
            break;

        default:
            break;
    }
}

// ============================================================================
// ANIMATION
// ============================================================================

// ── Pacci's animations: the two-handed sword set, not the Somaria cast ───────
// Pacci is a sword-stance item (PLAYER_MODELGROUP_BGS with the blade suppressed),
// so its motions come from the vanilla two-handed animations rather than from the
// Somaria summoning cast, which stays on the red cane.
//
//   FLIP / THROW : link_hammer_hit — the two-handed forward swing.
//   HOLD         : link_fighter_power_kiru_wait — the charged overhead pose, frozen
//                  on its first frame so the cane simply stays raised.
#define CANE_PACCI_ANIM_SWING &gPlayerAnim_link_hammer_hit
#define CANE_PACCI_ANIM_RAISED &gPlayerAnim_link_fighter_power_kiru_wait

// The cast animation (soh.o2r) is 60 frames, played at DOUBLE speed so the cast
// snaps instead of dragging — 30 real frames end to end. shSomariaAnimTimer counts
// REAL frames, so the midpoint the skill fires on halves along with the playback.
#define CANE_CAST_ANIM_SPEED 2.0f
#define SOMARIA_SPAWN_FRAME 15
// Hard ceiling on a cast. The animation is ~30 real frames; anything past this
// means it is never going to report completion, so Link gets his control back.
#define CANE_CAST_TIMEOUT_FRAMES 90

// Which animation a cast plays. Somaria keeps its own summoning cast; Pacci and
// Ultrahand use the two-handed forward swing.
static LinkAnimationHeader* Cane_CastAnimFor(u8 type) {
    // EVERY cast uses the Somaria casting animation, Pacci included: flipping is
    // that same motion doing something else, not a sword swing.
    //
    // It also has to be this one for a mechanical reason. The skill fires on
    // SOMARIA_SPAWN_FRAME (15) of whatever is playing, and link_hammer_hit is far
    // shorter than that — it ended before the action frame arrived, so the flip
    // silently never executed even though the animation looked fine. The
    // two-handed swing is only used where nothing has to fire from it: the throw.
    return NeiAnim_Load(NEI_ANIM_SOMARIA); // may be NULL if the resource is missing
}

static void Cane_StartCastAnim(Player* p, PlayState* play, u8 skill) {
    // The skill is driven by this animation's progress, so a missing resource
    // must not put us in the animating state at all — the cast would never
    // complete. Skijer's NEI
    LinkAnimationHeader* anim = Cane_CastAnimFor(Cane_GetType());

    if (anim == NULL) {
        // No animation available: fire immediately rather than swallow the input.
        Cane_FireSkill(p, play, skill);
        return;
    }
    LinkAnimation_PlayOnceSetSpeed(play, &p->upperSkelAnime, anim, CANE_CAST_ANIM_SPEED);
    shSomariaAnimating = 1;
    shSomariaAnimTimer = 0;
    canePendingSkill = skill;
    somariaState = SOMARIA_STATE_CASTING;
}

// Plays the swing purely as motion: the throw has already been resolved, so this
// must not schedule a skill the way Cane_StartCastAnim does.
static void Cane_StartCastAnimNoFire(Player* p, PlayState* play) {
    LinkAnimationHeader* anim = CANE_PACCI_ANIM_SWING;

    LinkAnimation_PlayOnce(play, &p->upperSkelAnime, anim);
    shSomariaAnimating = 1;
    shSomariaAnimTimer = 0;
    canePendingSkill = CANE_SKILL_MAX; // sentinel: fire nothing at the action frame
    somariaState = SOMARIA_STATE_CASTING;
}

// Which of Somaria's three summons the player actually has. Level 1 (bit 0) is the
// statue; level 2 (bit 1) unlocks blocks AND platforms together.
static u8 Cane_SummonSlotOwned(u8 slot) {
    switch (slot) {
        case 0:
            return Cane_HasSkill(CANE_SKILL_SOMARIA_STATUE);
        case 1:
        case 2:
            return Cane_HasSkill(CANE_SKILL_SOMARIA_BLOCK);
        default:
            return 0;
    }
}

// Step to the next owned summon in `dir`, wrapping and skipping locked ones.
// Somaria only: Pacci has nothing to cycle, so L/R are inert there.
static void Cane_CycleSummon(Player* p, PlayState* play, s8 dir) {
    u8 cur;
    u8 owned = 0;

    // The Trirod cycles its LEARNED ECHOES here instead of Somaria's summons.
    if (Cane_GetType() == CANE_TYPE_TRIROD) {
        Trirod_Cycle(p, play, dir);
        return;
    }
    if (Cane_GetType() != CANE_TYPE_SOMARIA) {
        return;
    }
    for (u8 i = 0; i < 3; i++) {
        owned += Cane_SummonSlotOwned(i);
    }
    if (owned < 2) {
        return; // nothing to cycle between
    }

    cur = Nei_CaneGetSkillSlot(CANE_TYPE_SOMARIA);
    for (u8 step = 1; step <= 3; step++) {
        s8 probe = (s8)(cur + (dir >= 0 ? step : (3 - step)));
        probe = (s8)(probe % 3);
        if (Cane_SummonSlotOwned((u8)probe)) {
            Nei_CaneSetSkillSlot(CANE_TYPE_SOMARIA, (u8)probe);
            Audio_PlaySoundGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            return;
        }
    }
}

// ============================================================================
// EQUIP / UNEQUIP
// ============================================================================

static void Somaria_OnEquip(PlayState* play, Player* p) {
    shSomariaActive = 1;
    somariaState = SOMARIA_STATE_EQUIPPED;
    shSomariaAnimating = 0;
    shSomariaAnimTimer = 0;
    caneHoldTimer = 0;
    canePreviewValid = 0;
    ItemEquip_PlayEquipSFX(play, p);
}

static void Somaria_OnUnequip(PlayState* play, Player* p) {
    shSomariaActive = 0;
    somariaState = SOMARIA_STATE_INACTIVE;
    shSomariaAnimating = 0;
    shSomariaAnimTimer = 0;
    somariaButtonMask = 0;
    caneHoldTimer = 0;
    canePreviewValid = 0;
    PacciFlipVfx_Stop();
    // Putting the cane away lets go of anything Ultrahand was carrying — and NOTHING
    // else. Flipped and petrified enemies keep their state on purpose: the whole
    // point of Flip is to knock an enemy over, swap to a weapon, and hit it while it
    // is down. Summons stay too — they are placed objects.
    Pacci_SetUltrahandArmed(0);
    Pacci_DropUltrahand();
    Pacci_LiftCancel();
    caneSelectTimer = 0;
    ItemEquip_PlayUnequipSFX(play, p);
}

// ============================================================================
// MAIN HANDLER
// ============================================================================

void Handle_CaneOfSomaria(Player* p, PlayState* play) {
    ItemInputState in;
    ItemInput_Update(&in, ITEM_CANE_OF_SOMARIA, p, play);
    somariaButtonMask = in.equippedButton;

    // Not equipped — clean up.
    if (!in.wasEquipped) {
        if (shSomariaActive) {
            Somaria_OnUnequip(play, p);
        }
        sSomariaEquipState.isEquipped = 0;
        return;
    }

    // ---- L / R: step the summon -----------------------------------------
    // No wheel (user-locked): L and R step to the next owned summon. Switching CANE
    // lives on A over the kaleido cell, so these only touch Somaria's summons.
    //
    // The edge is detected HERE from cur.button rather than read from press.button.
    // R is the shield: the player actor handles it long before this item code runs,
    // and by then the press bit is gone — which is why reading press.button never
    // saw R at all. A private previous-state byte is immune to that.
    if (in.wasEquipped && shSomariaActive && !shSomariaAnimating) {
        u16 held = play->state.input[0].cur.button;
        u8 rNow = (held & BTN_R) ? 1 : 0;
        u8 lNow = (held & BTN_L) ? 1 : 0;
        u8 rEdge = rNow && !sCanePrevR;
        u8 lEdge = lNow && !sCanePrevL;

        sCanePrevR = rNow;
        sCanePrevL = lNow;

        if (rEdge) {
            Cane_CycleSummon(p, play, 1);
            return;
        }
        if (lEdge) {
            Cane_CycleSummon(p, play, -1);
            return;
        }
    } else {
        sCanePrevR = 0;
        sCanePrevL = 0;
    }

    // ---- Ultrahand mode owns the whole frame while it is up --------------
    // This sits ABOVE everything: the blocking checks, the damage check and
    // ItemEquip_Update all run below, and every one of them can tear the item down.
    // With the mode update further down, pressing A to grab went through that
    // gauntlet first and the cane was already unequipped by the time the mode saw
    // the button. Inside the mode the ONLY way out is B, which the mode handles
    // itself — nothing else may equip, unequip or interrupt.
    if (Pacci_UltrahandModeUpdate(play, p)) {
        return;
    }

    // The cane is in hand but no skill bit is lit — a save from before the Dual
    // Cane rework, or a bulk "give all" that filled the slot directly. Rather than
    // hand the player a cane that does nothing, self-heal to the base skill.
    if (!Nei_CaneOwned()) {
        Cane_GiveSkill(CANE_SKILL_SOMARIA_STATUE);
    }

    // Blocking checks.
    if (!shSomariaActive) {
        if (ItemInput_IsBlockedEx(p, play, 1)) {
            return;
        }
    } else {
        u32 criticalBlocks = (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                              PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
                              PLAYER_STATE1_DAMAGED | PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE |
                              PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_HOOKSHOT_FALLING);
        if (p->stateFlags1 & criticalBlocks) {
            return;
        }
    }

    // Taking a hit puts the cane away, same as the rods.
    if (ItemInput_CheckDamage(p, &sSomariaPrevInvinc)) {
        if (shSomariaActive) {
            Somaria_OnUnequip(play, p);
        }
        sSomariaEquipState.isEquipped = 0;
        return;
    }

    // The FIRST press draws the cane and nothing else (user-locked): ItemEquip_Update
    // consumes it as the equip press and the cast below never sees it. Only once the
    // cane is out does a press summon.
    //
    // The real Somaria_OnUnequip goes in, exactly like the Fire/Ice/Light Rods pass
    // theirs: drawing the sword or reaching for another item HAS to put the cane
    // away. An earlier pass passed NULL here, which is what left the cane stuck in
    // hand across every other action.
    ItemEquip_Update(&sSomariaEquipState, &in, Somaria_OnEquip, Somaria_OnUnequip, p, play);

    if (!shSomariaActive) {
        return;
    }

    CaneSummon_CleanupPool();
    Pacci_CleanupPool();
    // Before anything else this frame: the arm pose, the draw hook and the offer all read it.
    Pacci_SetUltrahandArmed(Nei_CaneActiveSkill() == CANE_SKILL_PACCI_ULTRAHAND);
    Pacci_UpdateUltrahand(play, p);
    Pacci_LiftUpdate(play, p);
    PacciFlipVfx_Update(play, p); // stub — see pacci_flip_vfx.h

    u8 skill = Nei_CaneActiveSkill();

    // Mid-cast: hold still and let the upper-body action drive the animation.
    if (shSomariaAnimating) {
        // Watchdog. The cast pins Link in place until the animation reports it is
        // finished; if that never happens — a missing animation resource, a scene
        // that interrupts the upper-body action — he would stay frozen with no way
        // out. Past this many frames the cast is abandoned and control returns.
        if (shSomariaAnimTimer > CANE_CAST_TIMEOUT_FRAMES) {
            shSomariaAnimating = 0;
            shSomariaAnimTimer = 0;
            somariaState = SOMARIA_STATE_EQUIPPED;
            return;
        }
        p->actor.speedXZ = 0.0f;
        p->linearVelocity = 0.0f;
        return;
    }

    // Live aim feedback for the skills that use it.
    if (Cane_SkillNeedsAim(skill)) {
        Cane_UpdatePreview(p, play, skill);
    } else {
        canePreviewValid = 0;
        // Live aim feedback: show what the press would act on, before it happens.
        if (skill == CANE_SKILL_PACCI_ULTRAHAND) {
            // Everything Ultrahand offers, offered from the moment it is selected. The mode is
            // no longer the price of admission - it only adds the D-pad controls.
            Pacci_HighlightUltrahandTarget(play);
            if (Pacci_IsHoldingUltrahand()) {
                Pacci_FuseUpdatePreview(play, p);
                Pacci_PlaceUpdatePreview(play);
            }
        } else if (skill == CANE_SKILL_PACCI_FLIP) {
            // Holding the button is about to lift, so show the LIFT candidate (a
            // wider set: props too, and only weak enough enemies). A tap flips, so
            // before the hold threshold the flip target is what matters.
            if (caneHoldTimer > 0) {
                Pacci_HighlightLiftTarget(play);
            } else {
                Pacci_HighlightEnemyTarget(play, false);
            }
        } else if (skill == CANE_SKILL_PACCI_STONE) {
            Pacci_HighlightEnemyTarget(play, true);
        }
    }

    // Trirod aim runs every equipped frame: it owns the learn/dismiss highlight,
    // the ghost marker under the billboard, and the pool housekeeping.
    if (Cane_GetType() == CANE_TYPE_TRIROD) {
        Trirod_Aim(p, play);
    }

    if (p->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        return;
    }
    if (p->meleeWeaponState != 0) {
        return;
    }

    // ---- C: cast ---------------------------------------------------------
    // Somaria does not care how long you hold: a press casts, full stop. Only
    // Pacci's Flip splits the button, because only it has two things to do.
    if (skill != CANE_SKILL_PACCI_FLIP) {
        caneHoldTimer = 0;
        // Ultrahand does not cast: C opens its mode instead.
        if (in.isPressed && (Cane_GetType() == CANE_TYPE_ULTRAHAND)) {
            Pacci_UltrahandModeEnter(play, p);
            return;
        }
        // Trirod: the press may be a LEARN or a DISMISS, both instant and without
        // the swing. Only a real summon falls through to the cast below (its echo
        // is already committed; the animation's spawn frame fires it).
        if (in.isPressed && (Cane_GetType() == CANE_TYPE_TRIROD)) {
            if (Trirod_OnPress(p, play)) {
                return;
            }
        }
        if (in.isPressed) {
            if (!Cane_CanCast(p, play, skill)) {
                Cane_PlayError(p, play);
                return;
            }
            Cane_StartCastAnim(p, play, skill);
        }
        return;
    }

    // ---- C with Pacci + Flip: TAP flips, HOLD lifts -----------------------
    // Holding picks the target up and keeps it in the air for as long as the
    // button is down; letting go throws it (at Link's lock-on target if he has
    // one). A tap that never reaches the hold threshold is the plain flip.
    if (in.isHeld) {
        if (caneHoldTimer < (CANE_LIFT_HOLD_FRAMES + 1)) {
            caneHoldTimer++;
        }
        if (caneHoldTimer == CANE_LIFT_HOLD_FRAMES) {
            if (!Pacci_LiftTryGrab(play, p)) {
                Cane_PlayError(p, play); // nothing liftable in front of him
            }
        }
        return;
    }

    if (caneHoldTimer > 0) {
        s16 held = caneHoldTimer;
        caneHoldTimer = 0;

        if (held >= CANE_LIFT_HOLD_FRAMES) {
            Pacci_LiftThrow(play, p); // no-op if the grab found nothing
            // Let go of the raised pose and swing it forward.
            Cane_StartCastAnimNoFire(p, play);
            return;
        }
        // Tap: the flip.
        if (!Cane_CanCast(p, play, skill)) {
            Cane_PlayError(p, play);
            return;
        }
        Cane_StartCastAnim(p, play, skill);
        return;
    }

    caneHoldTimer = 0;
}

// ============================================================================
// INIT
// ============================================================================

void Player_InitCaneOfSomariaIA(PlayState* play, Player* p) {
    shSomariaActive = 1;
    somariaState = SOMARIA_STATE_EQUIPPED;
    shSomariaAnimating = 0;
    shSomariaAnimTimer = 0;
    somariaButtonMask = 0;
    caneHoldTimer = 0;
    canePreviewValid = 0;
}

// ============================================================================
// UPPER ACTION
// ============================================================================

// Pacci keeps the cane held out: the upper body sits on one frame of the cast
// animation while the lower body is left completely alone, so Link still walks,
// runs and jumps normally with the cane raised. Returning 1 is what claims the
// upper body; the lower half never sees this action.
#define CANE_PACCI_POSE_FRAME 10.0f // midpoint of the 20-frame (2x speed) cast

static u8 sCanePoseHeld = 0;
// Deliberately a second latch rather than a reuse of sCanePoseHeld: the reach and the
// raised pose are different animations, and one shared flag would let a switch from one
// to the other go unnoticed, leaving whichever was already playing.
static u8 sCaneReachHeld = 0;

// While Ultrahand is carrying something Link holds his right arm out toward it, the way
// TotK poses him. gPlayerAnim_link_boom_throw_waitR is the boomerang "arm extended,
// waiting" loop — the only vanilla animation that holds a single arm forward on its own
// — played at speed 0 so it is a pose and not a throw. It claims the upper body only, so
// he still walks, runs and jumps while the object follows him.
#define PACCI_UH_ANIM_REACH &gPlayerAnim_link_boom_throw_waitR
// How far the torso may pitch tracking the object. The aim can point straight up, and
// past roughly this the shoulder tears away from the chest.
#define PACCI_UH_REACH_PITCH 0x1800

s32 Player_UpperAction_CaneOfSomaria(Player* player, PlayState* play) {
    if (!shSomariaActive) {
        sCanePoseHeld = 0;
        sCaneReachHeld = 0;
        return 0;
    }
    if (!shSomariaAnimating) {
        // Somaria has no held pose — it only animates while casting.
        if (Cane_GetType() == CANE_TYPE_SOMARIA) {
            sCanePoseHeld = 0;
            sCaneReachHeld = 0;
            return 0;
        }

        // Carrying outranks every other pose: the outstretched arm is what sells the
        // telekinesis, and it has to survive the hold timer that owns the raised pose.
        // ARMED, not holding. The outstretched arm is what says "this hand is doing telekinesis
        // right now"; making it wait until something was already in it meant the reach only
        // appeared after the grab it was supposed to explain.
        if (Pacci_UltrahandArmed()) {
            if (!sCaneReachHeld) {
                LinkAnimation_PlayOnceSetSpeed(play, &player->upperSkelAnime, PACCI_UH_ANIM_REACH, 0.0f);
                sCaneReachHeld = 1;
            }
            player->upperSkelAnime.curFrame = 0.0f;
            LinkAnimation_Update(play, &player->upperSkelAnime);
            // Pitch the torso with the aim so the arm tracks the object up and down.
            // Without this he points flat at the horizon while it floats over his head.
            player->upperLimbRot.x = CLAMP(player->actor.focus.rot.x, -PACCI_UH_REACH_PITCH, PACCI_UH_REACH_PITCH);
            player->upperLimbRot.y = 0;
            player->upperLimbRot.z = 0;
            sCanePoseHeld = 0;
            return 1;
        }
        sCaneReachHeld = 0;

        // The raised pose belongs to the HOLD, not to merely carrying the cane:
        // holding the button lifts it overhead and freezes there, releasing swings.
        if (caneHoldTimer < CANE_LIFT_HOLD_FRAMES) {
            sCanePoseHeld = 0;
            return 0;
        }
        if (!sCanePoseHeld) {
            // Speed 0 so nothing advances; frame 0 IS the raised pose.
            LinkAnimation_PlayOnceSetSpeed(play, &player->upperSkelAnime, CANE_PACCI_ANIM_RAISED, 0.0f);
            sCanePoseHeld = 1;
        }
        player->upperSkelAnime.curFrame = 0.0f;
        LinkAnimation_Update(play, &player->upperSkelAnime);
        return 1;
    }

    sCanePoseHeld = 0;
    sCaneReachHeld = 0;

    if (LinkAnimation_Update(play, &player->upperSkelAnime)) {
        shSomariaAnimating = 0;
        shSomariaAnimTimer = 0;
        somariaState = SOMARIA_STATE_EQUIPPED;
    } else {
        shSomariaAnimTimer++;
        if (shSomariaAnimTimer == SOMARIA_SPAWN_FRAME) {
            // No sound here: CaneSummon_Spawn plays the single magic shimmer, and
            // the Pacci skills play their own. Firing one from here as well was
            // what made every cast sound doubled.
            // CANE_SKILL_MAX is the throw swing's "fire nothing" sentinel; every
            // other value — the six bits AND the Trirod — lands here.
            if (canePendingSkill != CANE_SKILL_MAX) {
                Cane_FireSkill(player, play, canePendingSkill);
            }
        }
    }

    return 1; // upper body is busy
}

// ============================================================================
// ACCESSORS FOR THE C++ HUD (2s2h/Enhancements/CaneWheelHud.cpp)
// ============================================================================

// Is the Dual Cane in hand right now? Used by ExtEquip_ShouldHideSwordDL to
// suppress the sword model: the cane rides on PLAYER_MODELGROUP_BGS purely for the
// two-handed stance, and without this Link holds the Biggoron blade AND the cane.
u8 Cane_IsActive(void) {
    return shSomariaActive;
}

// The radial wheel is gone — L/R step the summon directly now. These two remain
// only because CaneWheelHud.cpp links against them; SOMARIA_STATE_WHEEL is never
// set any more, so the HUD's wheel branch is simply unreachable.
u8 Cane_IsWheelOpen(void) {
    return 0;
}

s32 Cane_GetWheelSpoke(void) {
    return 0;
}
