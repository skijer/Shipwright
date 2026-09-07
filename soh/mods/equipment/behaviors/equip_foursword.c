/**
 * equip_foursword.c - Four Sword (extended sword slot 2)
 *
 * R + B held for 15 frames summons three clone actors (actors/four_sword_clone.c) that hold
 * formation around Link and mirror the item he just used. Included by ext_equip_behavior.c.
 */

#define FOURSWORD_BLADE_DL "__OTR__objects/object_nei_four_sword/gNeiFourSwordBladeDL"
#define FOURSWORD_HILT_DL "__OTR__objects/object_nei_four_sword/gNeiFourSwordHiltDL"

#define FS_CHARGE_HOLD 15
#define FS_ITEM_COOLDOWN 10

// Long enough that a TAP of L still reaches the vanilla Z-target.
#define FS_WHEEL_HOLD_FRAMES 8

// [formation][variant]; a formation with one variant repeats it, so the lookup is never bounds-checked.
static const char* const sFormationIcons[FSC_FORM_MAX][4] = {
    { "__OTR__textures/four_sword/gFourSwordFormTriangleTex" },
    { "__OTR__textures/four_sword/gFourSwordFormLine0Tex", "__OTR__textures/four_sword/gFourSwordFormLine1Tex" },
    { "__OTR__textures/four_sword/gFourSwordFormGridTex" },
    { "__OTR__textures/four_sword/gFourSwordFormCross0Tex", "__OTR__textures/four_sword/gFourSwordFormCross1Tex",
      "__OTR__textures/four_sword/gFourSwordFormCross2Tex", "__OTR__textures/four_sword/gFourSwordFormCross3Tex" },
    { "__OTR__textures/four_sword/gFourSwordFormTotemTex" },
    { "__OTR__textures/four_sword/gFourSwordFormDisperseTex" },
};

static s16 sFourSwordWheelHoldTimer = 0;
static u8 sWheelVariant[FSC_FORM_MAX];

static void FourSword_OnFormationConfirm(s32 index) {
    FourSwordClone_SetFormation((u8)index, sWheelVariant[index]);
}

// The wheel is one row, so BoxMenu hands us its dead vertical axis: it walks a formation's variants.
static void FourSword_OnFormationSubStep(s32 index, s32 dir) {
    u8 count = FourSwordClone_VariantCount((u8)index);
    u8 next = (u8)(((s32)sWheelVariant[index] + (dir > 0 ? 1 : count - 1)) % count);

    sWheelVariant[index] = next;
    BoxMenu_SetEntryIcon(index, sFormationIcons[index][next]);
}

// Gated on having clones out: with none summoned this is just a sword and L must stay Z-target.
static void FourSword_FormationWheel(PlayState* play) {
    if (FourSwordClone_Count() == 0) {
        sFourSwordWheelHoldTimer = 0;
        return;
    }
    // cur.button, never press: the player actor consumes L's press bit before equipment code runs.
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_L)) {
        sFourSwordWheelHoldTimer = 0;
        return;
    }

    if (sFourSwordWheelHoldTimer < (FS_WHEEL_HOLD_FRAMES + 1)) {
        sFourSwordWheelHoldTimer++;
    }
    if (sFourSwordWheelHoldTimer == FS_WHEEL_HOLD_FRAMES) {
        BoxMenuEntry entries[FSC_FORM_MAX];

        extern u8 FourSword_TotemGrabAllowed(void);

        sWheelVariant[FourSwordClone_GetFormation()] = FourSwordClone_GetVariant();
        for (s32 i = 0; i < FSC_FORM_MAX; i++) {
            entries[i].iconPath = sFormationIcons[i][sWheelVariant[i]];
            entries[i].iconSize = 32;
            entries[i].enabled = (i != FSC_FORM_TOTEM) || FourSword_TotemGrabAllowed();
        }
        if (BoxMenu_Open(play, entries, FSC_FORM_MAX, FourSwordClone_GetFormation(), BTN_L,
                         FourSword_OnFormationConfirm)) {
            BoxMenu_SetSubStep(FourSword_OnFormationSubStep);
        }
    }
}

static void FourSword_ApplyChargeAnim(Player* player, PlayState* play) {
    AnimationContext_SetLoadFrame(play, &gPlayerAnim_link_fighter_power_kiru_wait, 0, player->skelAnime.limbCount,
                                  player->upperJointTable);
    for (s32 j = PLAYER_LIMB_UPPER; j < PLAYER_LIMB_MAX; j++) {
        player->skelAnime.jointTable[j] = player->upperJointTable[j];
    }
}

#define FS_TOTEM_THROW_UP 16.0f
#define FS_TOTEM_THROW_FORWARD 14.0f
#define FS_TOTEM_THROW_ANIM gPlayerAnim_link_normal_jump

static u8 sTotemReleasedThisFrame = 0;
static Vec3f sTotemTop;

static void FourSword_TotemRelease(Player* player, Actor* base) {
    base->parent = NULL;
    if (player->heldActor == base) {
        player->heldActor = NULL;
        player->interactRangeActor = NULL;
        player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
    }
}

// The stack is grabbed the instant the formation is picked, and A launches Link off the top. This
// runs after Player_UpdateCommon, so the throw velocity lands on the NEXT frame's move — which is
// exactly what makes the arc read.
static void FourSword_TotemRide(Player* player, PlayState* play) {
    Actor* base = FourSwordClone_TotemBaseActor();

    sTotemReleasedThisFrame = 0;
    if (base == NULL) {
        return;
    }

    if (base->parent == NULL) {
        return;
    }

    if (player->stateFlags1 & PLAYER_STATE1_DAMAGED) {
        FourSword_TotemRelease(player, base);
        sTotemReleasedThisFrame = 1;
        FourSwordClone_KillAll();
        FourSwordClone_SetFormation(FSC_FORM_TRIANGLE, 0);
        return;
    }

    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
        return;
    }

    FourSwordClone_TotemTopPos(&sTotemTop);
    FourSword_TotemRelease(player, base);
    sTotemReleasedThisFrame = 1;

    player->actor.world.pos = sTotemTop;
    player->actor.velocity.y = FS_TOTEM_THROW_UP;
    player->linearVelocity = FS_TOTEM_THROW_FORWARD;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    player->stateFlags1 |= PLAYER_STATE1_FREEFALL;
    LinkAnimation_PlayOnce(play, &player->skelAnime, (LinkAnimationHeader*)FS_TOTEM_THROW_ANIM);
    // Not Sfx_PlaySfxCentered: this one adds ageProperties->unk_92, which is what picks the child
    // or adult voice, and routes through the transformed forms' own banks.
    extern void Player_PlayVoiceSfx(Player * this, u16 sfxId);
    Player_PlayVoiceSfx(player, NA_SE_VO_LI_SWORD_N);
    FourSwordClone_KillAll();
    FourSwordClone_SetFormation(FSC_FORM_TRIANGLE, 0);
}

// Read from the slot itself, not from a flag the behavior refreshes: turning the cheat off never
// runs FourSword_Cleanup, and a stale flag would leave the clones orphaned in the world.
u8 FourSword_IsEquipped(void) {
    return ExtEquip_IsEnabled() && (gExtEquipState.currentExtSword == 2);
}

// Ivan's dispatch, not an actor scan: watch for the frame a shot leaves Link's hands.
static void FourSword_SpawnCloneProjectiles(Player* player, PlayState* play) {
    if (gExtEquipBehavior.fourSwordItemCooldown > 0) {
        gExtEquipBehavior.fourSwordItemCooldown--;
        goto update_prev;
    }
    if (FourSwordClone_Count() == 0) {
        goto update_prev;
    }

    // unk_A73 is set to 4 on the exact frame of fire (z_player.c:3198).
    if ((player->unk_A73 == 4) && (gExtEquipBehavior.fourSwordPrevA73 != 4)) {
        s16 arrowType = ARROW_NORMAL;
        PlayerItemAction ia = player->heldItemAction;

        if ((ia >= PLAYER_IA_BOW) && (ia <= PLAYER_IA_BOW_0E)) {
            arrowType = ARROW_NORMAL + (ia - PLAYER_IA_BOW);
        } else if (ia == PLAYER_IA_SLINGSHOT) {
            arrowType = ARROW_SEED;
        } else if (player->boomerangActor != NULL && gExtEquipBehavior.fourSwordPrevBoomerang == 0) {
            goto skip_arrow; // boomerang raises this same flag; its own branch spawns it
        } else {
            arrowType = ARROW_NUT;
        }

        for (u8 i = 0; i < FSC_MAX; i++) {
            Vec3f cp;
            if (!FourSwordClone_GetPos(i, &cp)) {
                continue;
            }
            // parent stays NULL so EnArrow_Shoot fires it; unk_A73 is already 4 this frame.
            Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, cp.x, cp.y + 7.0f, cp.z,
                        (arrowType == ARROW_NUT) ? 0x1000 : 0, player->actor.shape.rot.y, 0, arrowType);
        }
        gExtEquipBehavior.fourSwordItemCooldown = FS_ITEM_COOLDOWN;
    }
skip_arrow :

{
    u8 curCarrying = (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) ? 1 : 0;
    if (gExtEquipBehavior.fourSwordPrevCarrying && !curCarrying && !sTotemReleasedThisFrame) {
        for (u8 i = 0; i < FSC_MAX; i++) {
            Vec3f cp;
            if (!FourSwordClone_GetPos(i, &cp)) {
                continue;
            }
            Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, cp.x, cp.y + 7.0f, cp.z, 0, 0, 0, 0);
        }
        gExtEquipBehavior.fourSwordItemCooldown = FS_ITEM_COOLDOWN;
    }
}

    {
        u8 curBoom = (player->boomerangActor != NULL) ? 1 : 0;
        if (curBoom && !gExtEquipBehavior.fourSwordPrevBoomerang) {
            for (u8 i = 0; i < FSC_MAX; i++) {
                Vec3f cp;
                if (!FourSwordClone_GetPos(i, &cp)) {
                    continue;
                }
                f32 px = Math_SinS(player->actor.shape.rot.y) * 1.0f + cp.x;
                f32 pz = Math_CosS(player->actor.shape.rot.y) * 1.0f + cp.z;
                EnBoom* boom = (EnBoom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOOM, px, cp.y + 7.0f, pz,
                                                    player->actor.focus.rot.x, player->actor.shape.rot.y, 0, 0);
                if (boom != NULL) {
                    boom->returnTimer = 20;
                }
            }
            gExtEquipBehavior.fourSwordItemCooldown = FS_ITEM_COOLDOWN;
        }
    }

update_prev:
    gExtEquipBehavior.fourSwordPrevA73 = player->unk_A73;
    gExtEquipBehavior.fourSwordPrevCarrying = (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) ? 1 : 0;
    gExtEquipBehavior.fourSwordPrevBoomerang = (player->boomerangActor != NULL) ? 1 : 0;
}

// Queried by WeaponUpgrade_ApplyHeldSwordDL, the single L_HAND injection point.
u8 FourSword_HeldSwordDL(void** blade, void** handle) {
    extern Gfx* ResourceMgr_LoadGfxByName(const char* path);
    static void* sBlade = NULL;
    static void* sHilt = NULL;
    static u8 sTried = 0;

    if (!FourSword_IsEquipped()) {
        return 0;
    }
    if (!sTried) {
        sTried = 1;
        sBlade = ResourceMgr_LoadGfxByName(FOURSWORD_BLADE_DL);
        sHilt = ResourceMgr_LoadGfxByName(FOURSWORD_HILT_DL);
        // A resource that fails to load comes back as the unresolved PATH STRING, not NULL, and the
        // interpreter would run "__OTR__objects/..." as F3DEX2 opcodes.
        if (sBlade != NULL && ((const char*)sBlade)[0] == '_') {
            sBlade = NULL;
        }
        if (sHilt != NULL && ((const char*)sHilt)[0] == '_') {
            sHilt = NULL;
        }
    }
    if (sBlade == NULL) {
        return 0; // asset missing (stale soh.o2r) → keep the vanilla sword instead of nothing
    }
    *blade = sBlade;
    *handle = sHilt;
    return 1;
}

static void FourSword_Behavior(Player* player, PlayState* play) {
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_LOADING |
                               PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM)) {
        return;
    }

    // The wheel owns the world while it is up (it pauses the game), so nothing below should run.
    if (BoxMenu_IsOpen()) {
        return;
    }
    FourSword_FormationWheel(play);

    u8 isShielding = (player->stateFlags1 & PLAYER_STATE1_SHIELDING) ? 1 : 0;
    u8 bHeld = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B) ? 1 : 0;

    if (isShielding && bHeld) {
        if (!gExtEquipBehavior.fourSwordCharging) {
            gExtEquipBehavior.fourSwordBHoldTimer++;
        }

        if (!gExtEquipBehavior.fourSwordCharging && gExtEquipBehavior.fourSwordBHoldTimer >= FS_CHARGE_HOLD) {
            gExtEquipBehavior.fourSwordCharging = 1;
            Sfx_PlaySfxCentered(NA_SE_SY_ATTENTION_ON);

            for (u8 i = 0; i < FSC_MAX; i++) {
                FourSwordClone_Spawn(play, player, i);
            }
        }
    } else {
        gExtEquipBehavior.fourSwordBHoldTimer = 0;
        gExtEquipBehavior.fourSwordCharging = 0;
    }

    if (gExtEquipBehavior.fourSwordCharging) {
        FourSword_ApplyChargeAnim(player, play);
    }

    FourSwordClone_Reconcile(play, player);
    FourSwordClone_SpinTick(player);
    FourSword_TotemRide(player, play);
    FourSword_SpawnCloneProjectiles(player, play);
}

static void FourSword_Cleanup(void) {
    FourSwordClone_KillAll();
    gExtEquipBehavior.fourSwordCharging = 0;
    gExtEquipBehavior.fourSwordBHoldTimer = 0;
    gExtEquipBehavior.fourSwordItemCooldown = 0;
    gExtEquipBehavior.fourSwordPrevA73 = 0;
    gExtEquipBehavior.fourSwordPrevCarrying = 0;
    gExtEquipBehavior.fourSwordPrevBoomerang = 0;
}
