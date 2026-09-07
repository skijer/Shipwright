/**
 * byrna_ig_combat.inc.c — Cane of Byrna (ext sword 1) = MHR "Insect Glaive".
 *
 * Everything vanilla already has a pipeline for is served THROUGH that pipeline by
 * swapping clips into the PLAYER_MWA_* rows, exactly like equip_trident.c. Only the
 * AERIAL mode is driven by hand under PAUSE_ACTION_FUNC, because vanilla has no
 * action for it — the same single exception the gunlance makes for its flight.
 *
 *   B, B, B     ground chain (ByrnaIg_NextComboMwa sequences the rows)
 *   fwd + B     the thrust, only while the chain is idle
 *   B (hold)    vanilla's charge, re-skinned as the quick spin
 *   Z + A       jump slash
 *   R           the Kinsect (equip_byrna.c); at FULL charge it launches LINK
 *   in the air  A dash / A+back climb / B spin / R ground pound
 *
 * Anims: gMonsterHunterRise_InsectGlaive_<Motion> in mhr_anims.o2r. All 22 clips
 * were checked to have body_turn = 0, so none of them rotates the skeleton root —
 * the bug that bit the gunlance's *WeaponTransition recoveries cannot happen here.
 *
 * FRAME UNITS: 20 Hz logic ticks (R_UPDATE_RATE = 3). 100 frames = 5 seconds.
 * Included by ext_equip_behavior.c (unity build). Skijer's NEI
 */

// Declared in soh/ResourceManagerHelpers.h, which this TU does not pull in.
// stripY = 1 everywhere: no clip may carry its own root translation, or the swing
// would teleport Link on top of the movement OOT already owns.
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* path);
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlace(const char* animPath, u8 stripY);
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(const char* animPath, u8 stripY,
                                                                               s16 frames);
extern LinkAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(const char* animPath, u8 stripY,
                                                                           s16 firstFrame, s16 lastFrame,
                                                                           s16 targetFrames);

#define BYIG(name) "__OTR__misc/link_animetion/gMonsterHunterRise_InsectGlaive_" name

#define BYIG_COMBO_RESET_FRAMES 40

#define BYIG_LAUNCH_UP 30.0f
#define BYIG_LAUNCH_FWD 6.0f
#define BYIG_AIR_GRAVITY (-1.1f)
#define BYIG_AIR_MIN_VY (-26.0f)
#define BYIG_DASH_SPEED 34.0f
#define BYIG_DASH_FRAMES 10
#define BYIG_CLIMB_UP 22.0f
#define BYIG_CLIMB_BACK (-14.0f)
#define BYIG_POUND_FALL (-30.0f)
#define BYIG_SPIN_HOLD 24

// ---------------------------------------------------------------------------
// Clip table. `frames` is the SPEED knob: ExtPlayer_SetMeleeAnim has no speed
// argument, so a faster swing is the same motion resampled into fewer frames.
// Damage windows are given in SOURCE frames and rescaled at install, so retuning
// `frames` never silently moves the hitbox.
// ---------------------------------------------------------------------------
typedef struct {
    s32 mwa;
    const char* path;
    s16 srcStart;
    s16 srcEnd;
    s16 frames;
    s16 hitStartSrc;
    s16 hitEndSrc;
    const char* recovery;
} ByrnaIgBinding;

#define BYIG_C1 BYIG("ForwardRisingDoubleChargedStaffCombo")               //  56f
#define BYIG_C2 BYIG("ForwardRisingMultiHitAdvancingStaffSweep_Variant08") //  88f
#define BYIG_C3 BYIG("ForwardRisingMultiHitAdvancingStaffSweep")           //  72f
#define BYIG_STAB BYIG("ForwardRisingTripleAerialStaffStrike_Variant05")   //  60f
#define BYIG_SPIN BYIG("StationaryRisingMultiHitChargedStaffCombo")        //  72f
#define BYIG_JUMP BYIG("BackwardRisingMultiHitRetreatingStaffStrike")      //  93f
#define BYIG_REC BYIG("StationaryDoubleStaffTransition")                   //  19f, turn 0

// Windows MEASURED in the lab (sword-hand tip speed, bursts at 42% of peak). C3's
// strike is a 3-frame flick inside a tumble, widened to 10-18 so the resample hits it.
static const ByrnaIgBinding sByrnaIgBindings[] = {
    // The chain. Which row holds which step is arbitrary — ByrnaIg_NextComboMwa
    // picks it, not OOT's stick-angle picker. Both 1H and 2H rows are bound so the
    // chain holds whatever sword is underneath the cane.
    { PLAYER_MWA_FORWARD_SLASH_1H, BYIG_C1, -1, -1, 20, 6, 16, BYIG_REC },
    { PLAYER_MWA_FORWARD_SLASH_2H, BYIG_C1, -1, -1, 20, 6, 16, BYIG_REC },
    { PLAYER_MWA_RIGHT_SLASH_1H, BYIG_C2, -1, -1, 24, 2, 27, BYIG_REC },
    { PLAYER_MWA_RIGHT_SLASH_2H, BYIG_C2, -1, -1, 24, 2, 27, BYIG_REC },
    { PLAYER_MWA_FORWARD_COMBO_1H, BYIG_C3, -1, -1, 26, 10, 18, BYIG_REC },
    { PLAYER_MWA_FORWARD_COMBO_2H, BYIG_C3, -1, -1, 26, 10, 18, BYIG_REC },
    // Left rows are never reached by the chain, but binding them keeps a glaive
    // clip on screen if anything ever does land there.
    { PLAYER_MWA_LEFT_SLASH_1H, BYIG_C2, -1, -1, 24, 2, 27, BYIG_REC },
    { PLAYER_MWA_LEFT_SLASH_2H, BYIG_C2, -1, -1, 24, 2, 27, BYIG_REC },
    { PLAYER_MWA_LEFT_COMBO_1H, BYIG_C3, -1, -1, 26, 10, 18, BYIG_REC },
    { PLAYER_MWA_LEFT_COMBO_2H, BYIG_C3, -1, -1, 26, 10, 18, BYIG_REC },
    { PLAYER_MWA_RIGHT_COMBO_1H, BYIG_C3, -1, -1, 26, 10, 18, BYIG_REC },
    { PLAYER_MWA_RIGHT_COMBO_2H, BYIG_C3, -1, -1, 26, 10, 18, BYIG_REC },

    { PLAYER_MWA_STAB_1H, BYIG_STAB, -1, -1, 22, 7, 15, NULL },
    { PLAYER_MWA_STAB_2H, BYIG_STAB, -1, -1, 22, 7, 15, NULL },
    { PLAYER_MWA_STAB_COMBO_1H, BYIG_STAB, -1, -1, 22, 7, 15, NULL },
    { PLAYER_MWA_STAB_COMBO_2H, BYIG_STAB, -1, -1, 22, 7, 15, NULL },

    // The measured peak (frame 84) is the LANDING impact; the airborne strikes are
    // the 10-26 bursts. JUMPSLASH_FINISH stays vanilla: one clip was named for the
    // jump slash, and vanilla's landing is short and does not rotate.
    { PLAYER_MWA_JUMPSLASH_START, BYIG_JUMP, -1, -1, 30, 10, 26, NULL },

    // B held. A spin hits for its whole rotation, so first burst to last. Both spin
    // rows get it: nothing here needs the two-level split the gunlance's charge uses.
    { PLAYER_MWA_SPIN_ATTACK_1H, BYIG_SPIN, -1, -1, 26, 8, 50, NULL },
    { PLAYER_MWA_SPIN_ATTACK_2H, BYIG_SPIN, -1, -1, 26, 8, 50, NULL },
    { PLAYER_MWA_BIG_SPIN_1H, BYIG_SPIN, -1, -1, 26, 8, 50, NULL },
    { PLAYER_MWA_BIG_SPIN_2H, BYIG_SPIN, -1, -1, 26, 8, 50, NULL },
};

#define BYIG_BINDING_COUNT ((s32)(sizeof(sByrnaIgBindings) / sizeof(sByrnaIgBindings[0])))

typedef enum {
    BYIG_GROUND = 0,
    BYIG_LAUNCH,
    BYIG_AIR,
    BYIG_DASH,
    BYIG_CLIMB,
    BYIG_SPIN_AIR,
    BYIG_POUND,
    BYIG_POUND_LAND,
} ByrnaIgState;

static struct {
    u8 inited;
    u8 state;
    u8 animsInstalled;
    u8 locoInstalled;
    s16 timer;
    f32 vy;
    LinkAnimationHeader* savedMelee[BYIG_BINDING_COUNT];
    LinkAnimationHeader* savedMeleeEnd[BYIG_BINDING_COUNT];
    LinkAnimationHeader* savedMeleeEndLock[BYIG_BINDING_COUNT];
    u8 savedHitStart[BYIG_BINDING_COUNT];
    u8 savedHitEnd[BYIG_BINDING_COUNT];
    LinkAnimationHeader* savedWalk[4];
    LinkAnimationHeader* savedRun[4];
    LinkAnimationHeader* savedDraw;
} sByIg = { 0 };

static s32 sByIgComboStep = 0;
static s32 sByIgComboIdle = 0;

// Hitboxes for the aerial states, which have no vanilla row to hang a quad on.
// Written in Link's own frame (right +X, up +Y, fwd +Z) and turned by shape.rot.y,
// same shape as the trident's boxes; `pitch` lays the box toward the floor.
typedef struct {
    f32 right;
    f32 up;
    f32 fwd;
    f32 halfW;
    f32 halfH;
    f32 pitch;
} ByrnaIgQuadBox;

static const ByrnaIgQuadBox sByIgBoxSpin = { 0.0f, 30.0f, 0.0f, 62.0f, 40.0f, 0.0f };
static const ByrnaIgQuadBox sByIgBoxDash = { 0.0f, 34.0f, 54.0f, 24.0f, 26.0f, 0.0f };
static const ByrnaIgQuadBox sByIgBoxPound = { 0.0f, 10.0f, 30.0f, 58.0f, 44.0f, 1.2f };

static ColliderQuad sByIgAtkQuad;
static u8 sByIgQuadInited = 0;

static ColliderQuadInit sByIgAtkQuadInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_QUAD },
    {
        ELEMTYPE_UNK2,
        { DMG_SLASH_MASTER, 0x00, 0x01 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static void ByrnaIg_PlaceQuad(PlayState* play, Player* player, const ByrnaIgQuadBox* box) {
    static const f32 sCornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const f32 sCornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
    Vec3f v[4];
    f32 sinY = Math_SinS(player->actor.shape.rot.y);
    f32 cosY = Math_CosS(player->actor.shape.rot.y);
    f32 upY = cosf(box->pitch);
    f32 upZ = sinf(box->pitch);
    s32 i;

    if (!sByIgQuadInited) {
        Collider_InitQuad(play, &sByIgAtkQuad);
        Collider_SetQuad(play, &sByIgAtkQuad, &player->actor, &sByIgAtkQuadInit);
        sByIgQuadInited = 1;
    }
    for (i = 0; i < 4; i++) {
        f32 lx = box->right + (sCornerX[i] * box->halfW);
        f32 ly = box->up + (sCornerY[i] * box->halfH * upY);
        f32 lz = box->fwd + (sCornerY[i] * box->halfH * upZ);

        v[i].x = player->actor.world.pos.x + (lx * cosY) + (lz * sinY);
        v[i].y = player->actor.world.pos.y + ly;
        v[i].z = player->actor.world.pos.z + (lz * cosY) - (lx * sinY);
    }
    Collider_SetQuadVertices(&sByIgAtkQuad, &v[0], &v[1], &v[2], &v[3]);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sByIgAtkQuad.base);
    sByIgAtkQuad.base.atFlags &= ~AT_HIT;
}

// ---------------------------------------------------------------------------
// Install / restore
// ---------------------------------------------------------------------------
static s16 ByrnaIg_InstalledLen(const ByrnaIgBinding* b, LinkAnimationHeader* raw) {
    if (b->frames > 0) {
        return b->frames;
    }
    if ((b->srcStart >= 0) && (b->srcEnd >= b->srcStart)) {
        return (s16)(b->srcEnd - b->srcStart + 1);
    }
    return (raw != NULL) ? (s16)raw->common.frameCount : 0;
}

// Map a marker given in SOURCE frames onto the installed clip.
//   -1 -> 0xFF ("keep vanilla's")    0 -> the installed clip's last frame
static u8 ByrnaIg_ScaleFrame(const ByrnaIgBinding* b, s16 srcFrame, LinkAnimationHeader* raw, s16 outLen) {
    s16 base;
    s16 srcLen;
    s32 scaled;

    if (srcFrame < 0) {
        return 0xFF;
    }
    if (outLen < 1) {
        outLen = 1;
    }
    if (srcFrame == 0) {
        return (u8)((outLen > 255) ? 255 : outLen);
    }
    base = (b->srcStart > 0) ? b->srcStart : 0;
    if ((b->srcStart >= 0) && (b->srcEnd >= b->srcStart)) {
        srcLen = (s16)(b->srcEnd - b->srcStart + 1);
    } else {
        srcLen = (raw != NULL) ? (s16)raw->common.frameCount : outLen;
    }
    if (srcLen < 1) {
        srcLen = 1;
    }
    scaled = ((s32)(srcFrame - base) * (s32)outLen) / (s32)srcLen;
    if (scaled < 0) {
        scaled = 0;
    }
    return (u8)((scaled > 255) ? 255 : scaled);
}

static void ByrnaIg_InstallAnims(void) {
    s32 i;

    if (sByIg.animsInstalled) {
        return;
    }
    for (i = 0; i < BYIG_BINDING_COUNT; i++) {
        const ByrnaIgBinding* b = &sByrnaIgBindings[i];
        LinkAnimationHeader* raw;
        LinkAnimationHeader* anim;
        LinkAnimationHeader* rec;
        s16 outLen;

        ExtPlayer_GetMeleeAnim(b->mwa, &sByIg.savedMelee[i], &sByIg.savedMeleeEnd[i], &sByIg.savedMeleeEndLock[i],
                               &sByIg.savedHitStart[i], &sByIg.savedHitEnd[i]);

        raw = ResourceMgr_LoadPlayerAnimAsHeader(b->path);
        outLen = ByrnaIg_InstalledLen(b, raw);
        anim = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceRange(b->path, 1, b->srcStart, b->srcEnd, b->frames);
        if (anim == NULL) {
            continue;
        }
        rec = (b->recovery != NULL) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlace(b->recovery, 1) : NULL;
        ExtPlayer_SetMeleeAnim(b->mwa, anim, rec, rec, ByrnaIg_ScaleFrame(b, b->hitStartSrc, raw, outLen),
                               ByrnaIg_ScaleFrame(b, b->hitEndSrc, raw, outLen));
    }

    // Drawing the weapon from a standing start.
    sByIg.savedDraw = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_normal2fighter, 0);
    if (ResourceMgr_FileExists(BYIG("ForwardMultiHitStaffStrike"))) {
        LinkAnimationHeader* draw =
            ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(BYIG("ForwardMultiHitStaffStrike"), 1, 20);
        if (draw != NULL) {
            ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_normal2fighter, 0, draw);
        }
    }
    sByIg.animsInstalled = 1;
}

// MUST run on every path out of the slot. These are global engine tables: leaving
// them installed gives plain Link glaive swings.
static void ByrnaIg_RestoreAnims(void) {
    s32 i;

    if (!sByIg.animsInstalled) {
        return;
    }
    for (i = 0; i < BYIG_BINDING_COUNT; i++) {
        ExtPlayer_SetMeleeAnim(sByrnaIgBindings[i].mwa, sByIg.savedMelee[i], sByIg.savedMeleeEnd[i],
                               sByIg.savedMeleeEndLock[i], sByIg.savedHitStart[i], sByIg.savedHitEnd[i]);
    }
    ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_normal2fighter, 0, sByIg.savedDraw);
    sByIg.animsInstalled = 0;
}

// ⚠️ TWO different lengths of the SAME clip, and this is what the gunlance had
// broken: both groups share the unk_868 phase but do not sample it alike —
// walk reads 29 frames, run reads unk_868 * (20/29) = 20 (z_player.c:10542).
// A 29-frame clip in the run group shows only its frames 0..20, so the cycle cuts
// at a third and restarts. All FOUR columns, too: leaving column 2 vanilla swapped
// the legs to Link's own run mid-stride.
static const s32 sByIgLocoCols[] = { 0, 1, 2, 3 };

static void ByrnaIg_InstallLoco(void) {
    LinkAnimationHeader* walk;
    LinkAnimationHeader* run;
    s32 i;

    if (sByIg.locoInstalled) {
        return;
    }
    if (!ResourceMgr_FileExists(BYIG("ForwardStaffRun"))) {
        return;
    }
    walk = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(BYIG("ForwardStaffRun"), 1, 29);
    run = ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(BYIG("ForwardStaffRun"), 1, 20);
    for (i = 0; i < 4; i++) {
        s32 col = sByIgLocoCols[i];

        sByIg.savedWalk[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col);
        sByIg.savedRun[col] = ExtPlayer_GetAnimGroupAnim(PLAYER_ANIMGROUP_run, col);
        if (walk != NULL) {
            ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col, walk);
        }
        if (run != NULL) {
            ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, run);
        }
    }
    sByIg.locoInstalled = 1;
}

static void ByrnaIg_RestoreLoco(void) {
    s32 i;

    if (!sByIg.locoInstalled) {
        return;
    }
    for (i = 0; i < 4; i++) {
        s32 col = sByIgLocoCols[i];

        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_walk, col, sByIg.savedWalk[col]);
        ExtPlayer_SetAnimGroupAnim(PLAYER_ANIMGROUP_run, col, sByIg.savedRun[col]);
    }
    sByIg.locoInstalled = 0;
}

// ---------------------------------------------------------------------------
// Ground chain
// ---------------------------------------------------------------------------
static const s32 sByIgComboRows[] = {
    PLAYER_MWA_FORWARD_SLASH_1H,
    PLAYER_MWA_RIGHT_SLASH_1H,
    PLAYER_MWA_FORWARD_COMBO_1H,
};
#define BYIG_COMBO_STEPS ((s32)(sizeof(sByIgComboRows) / sizeof(sByIgComboRows[0])))

static void ByrnaIg_TickCombo(void) {
    if (sByIgComboStep == 0) {
        return;
    }
    if (++sByIgComboIdle >= BYIG_COMBO_RESET_FRAMES) {
        sByIgComboStep = 0;
        sByIgComboIdle = 0;
    }
}

u8 ByrnaIg_OwnsComboRow(Player* player) {
    return (gExtEquipState.currentExtSword == 1) && ExtEquip_IsEnabled() && (player != NULL) &&
           (Player_GetMeleeWeaponHeld(player) != 0);
}

// func_80837948 starts every swing with a hard cut, which between three glaive
// sweeps reads as a jump. Vanilla ships the fix (a -6 morph); the chain rows just
// ask for it. Single moves keep their hard start.
u8 ByrnaIg_MorphsRow(Player* player, s32 mwa) {
    s32 i;

    if (!ByrnaIg_OwnsComboRow(player)) {
        return 0;
    }
    for (i = 0; i < BYIG_COMBO_STEPS; i++) {
        if ((mwa == sByIgComboRows[i]) || (mwa == (sByIgComboRows[i] + 1))) {
            return 1;
        }
    }
    return 0;
}

s32 ByrnaIg_NextComboMwa(Player* player, s32 requested) {
    s32 row;

    if (!ByrnaIg_OwnsComboRow(player)) {
        return requested;
    }
    if ((requested >= PLAYER_MWA_SPIN_ATTACK_1H) && (requested <= PLAYER_MWA_BIG_SPIN_2H)) {
        return requested;
    }
    if ((requested >= PLAYER_MWA_FLIPSLASH_START) && (requested <= PLAYER_MWA_JUMPSLASH_FINISH)) {
        return requested;
    }
    // The thrust is only served with the chain STOPPED. func_80837818 returns STAB
    // the moment there is a lock-on and the stick pushes forward, which is exactly
    // what you do to re-aim mid-combo — letting it through ate the next slash.
    if ((requested == PLAYER_MWA_STAB_1H) || (requested == PLAYER_MWA_STAB_2H) ||
        (requested == PLAYER_MWA_STAB_COMBO_1H) || (requested == PLAYER_MWA_STAB_COMBO_2H)) {
        if (sByIgComboStep == 0) {
            return requested;
        }
    }

    if (sByIgComboStep >= BYIG_COMBO_STEPS) {
        sByIgComboStep = 0;
    }
    row = sByIgComboRows[sByIgComboStep];
    sByIgComboStep = (sByIgComboStep + 1) % BYIG_COMBO_STEPS;
    sByIgComboIdle = 0;

    // The table holds 1H rows; the 2H twin is the next id up.
    if (Player_HoldsTwoHandedWeapon(player)) {
        row++;
    }
    return row;
}

// ---------------------------------------------------------------------------
// Aerial mode — the one thing vanilla has no action for
// ---------------------------------------------------------------------------
static void ByrnaIg_PlayClip(PlayState* play, Player* player, const char* path, s16 resample, u8 loop) {
    LinkAnimationHeader* anim;
    f32 last;

    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    anim = (resample > 0) ? ResourceMgr_LoadPlayerAnimAsHeaderInPlaceResampled(path, 1, resample)
                          : ResourceMgr_LoadPlayerAnimAsHeaderInPlace(path, 1);
    if (anim == NULL) {
        return;
    }
    // PAUSE must be stamped the same instant the clip lands on player->skelAnime,
    // or Player_Action_Idle advances it next frame and crashes in
    // AnimationContext_SetLoadFrame.
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    last = Animation_GetLastFrame(anim);
    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, last, loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
}

static void ByrnaIg_EnterAir(PlayState* play, Player* player) {
    sByIg.state = BYIG_LAUNCH;
    sByIg.timer = 0;
    sByIg.vy = BYIG_LAUNCH_UP;
    player->linearVelocity = BYIG_LAUNCH_FWD;
    player->yaw = player->actor.shape.rot.y;
    ByrnaIg_PlayClip(play, player, BYIG("BackwardHighAerialMultiHitSilkbindStaffStrike_Variant15"), 14, 0);
    Sfx_PlaySfxCentered(NA_SE_PL_JUMP);
}

static void ByrnaIg_ExitAir(Player* player) {
    sByIg.state = BYIG_GROUND;
    sByIg.timer = 0;
    sByIg.vy = 0.0f;
    player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    ByrnaOrb_OnLand();
}

// OOT integrates linearVelocity and gravity every frame even under
// PAUSE_ACTION_FUNC (the flag only skips actionFunc), so ONLY the vertical axis is
// stepped here. Adding a horizontal step too would move Link twice.
static void ByrnaIg_AirStep(Player* player) {
    sByIg.vy += BYIG_AIR_GRAVITY;
    if (sByIg.vy < BYIG_AIR_MIN_VY) {
        sByIg.vy = BYIG_AIR_MIN_VY;
    }
    player->actor.world.pos.y += sByIg.vy;
    player->actor.velocity.y = 0.0f;
}

static void ByrnaIg_FaceTarget(Player* player) {
    if (player->focusActor == NULL) {
        return;
    }
    player->actor.shape.rot.y = Math_Vec3f_Yaw(&player->actor.world.pos, &player->focusActor->world.pos);
    player->yaw = player->actor.shape.rot.y;
}

static void ByrnaIg_TickAir(PlayState* play, Player* player) {
    Input* in = &play->state.input[0];
    u8 aPress = CHECK_BTN_ALL(in->press.button, BTN_A) != 0;
    u8 bPress = CHECK_BTN_ALL(in->press.button, BTN_B) != 0;
    u8 rPress = CHECK_BTN_ALL(in->press.button, BTN_R) != 0;
    u8 stickBack = (player->controlStickDirections[player->controlStickDataIndex] == PLAYER_STICK_DIR_BACKWARD);

    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    sByIg.timer++;

    // Touchdown ends everything, whatever state we were in.
    if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (sByIg.state != BYIG_LAUNCH)) {
        if (sByIg.state == BYIG_POUND) {
            sByIg.state = BYIG_POUND_LAND;
            sByIg.timer = 0;
            player->linearVelocity = 0.0f;
            ByrnaIg_PlayClip(play, player, BYIG("BackwardDoubleChargedStaffCombo"), 22, 0);
            Sfx_PlaySfxCentered(NA_SE_IT_HAMMER_HIT);
            return;
        }
        if (sByIg.state != BYIG_POUND_LAND) {
            ByrnaIg_ExitAir(player);
            return;
        }
    }

    switch (sByIg.state) {
        case BYIG_LAUNCH:
            ByrnaIg_AirStep(player);
            if (sByIg.timer >= 14) {
                sByIg.state = BYIG_AIR;
                sByIg.timer = 0;
                ByrnaIg_PlayClip(play, player, BYIG("ForwardTripleAdvancingStaffSweep_Variant14"), 12, 0);
            }
            break;

        case BYIG_AIR:
            ByrnaIg_AirStep(player);
            player->linearVelocity *= 0.9f;
            if (sByIg.timer == 12) {
                ByrnaIg_PlayClip(play, player, BYIG("StationarySingleStaffTransition"), 20, 1);
            }
            if (rPress) {
                sByIg.state = BYIG_POUND;
                sByIg.timer = 0;
                sByIg.vy = BYIG_POUND_FALL;
                player->linearVelocity = 0.0f;
                ByrnaIg_PlayClip(play, player, BYIG("ForwardRisingTripleAerialStaffStrike"), 16, 0);
                break;
            }
            if (bPress) {
                sByIg.state = BYIG_SPIN_AIR;
                sByIg.timer = 0;
                ByrnaIg_FaceTarget(player);
                ByrnaIg_PlayClip(play, player, BYIG("LeftHighAerialMultiHitSilkbindStaffStrike"), 14, 1);
                break;
            }
            if (aPress && stickBack) {
                sByIg.state = BYIG_CLIMB;
                sByIg.timer = 0;
                sByIg.vy = BYIG_CLIMB_UP;
                player->linearVelocity = BYIG_CLIMB_BACK;
                ByrnaIg_PlayClip(play, player, BYIG("LeftRisingTripleChargedStaffCombo"), 18, 0);
                break;
            }
            if (aPress) {
                sByIg.state = BYIG_DASH;
                sByIg.timer = 0;
                sByIg.vy = 0.0f;
                ByrnaIg_FaceTarget(player);
                player->linearVelocity = BYIG_DASH_SPEED;
                ByrnaIg_PlayClip(play, player, BYIG("ForwardDoubleAdvancingStaffSweep_Variant13"), 8, 0);
            }
            break;

        case BYIG_DASH:
            // The dash carries Link at the locked-on enemy, so no gravity while it
            // runs — that is what makes it read as a closing move.
            player->actor.world.pos.y += sByIg.vy;
            ByrnaIg_PlaceQuad(play, player, &sByIgBoxDash);
            if (sByIg.timer >= BYIG_DASH_FRAMES) {
                sByIg.state = BYIG_AIR;
                sByIg.timer = 12;
                ByrnaIg_PlayClip(play, player, BYIG("ForwardSingleStaffTransition_Variant05"), 22, 1);
            }
            break;

        case BYIG_CLIMB:
            ByrnaIg_AirStep(player);
            if (sByIg.timer >= 16) {
                sByIg.state = BYIG_AIR;
                sByIg.timer = 12;
            }
            break;

        case BYIG_SPIN_AIR:
            // Hangs while it spins: the spin is the reason to be up here.
            sByIg.vy = 0.0f;
            player->linearVelocity *= 0.85f;
            ByrnaIg_PlaceQuad(play, player, &sByIgBoxSpin);
            if (sByIg.timer >= BYIG_SPIN_HOLD) {
                sByIg.state = BYIG_AIR;
                sByIg.timer = 12;
                ByrnaIg_PlayClip(play, player, BYIG("BackwardRisingTripleChargedStaffCombo"), 20, 0);
            }
            break;

        case BYIG_POUND:
            player->actor.world.pos.y += sByIg.vy;
            ByrnaIg_PlaceQuad(play, player, &sByIgBoxPound);
            if (sByIg.timer == 8) {
                ByrnaIg_PlayClip(play, player, BYIG("StationaryStaffReadyIdle_Variant05"), 12, 1);
            }
            break;

        case BYIG_POUND_LAND:
            player->linearVelocity = 0.0f;
            if (sByIg.timer >= 22) {
                ByrnaIg_ExitAir(player);
            }
            break;

        default:
            ByrnaIg_ExitAir(player);
            break;
    }
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------
u8 ByrnaIg_IsAirborne(void) {
    return sByIg.state != BYIG_GROUND;
}

// The aerial attacks are what carry Fierce-Deity-class damage, and only at a full
// Kinsect bar. Scoped to the states that actually swing, so an ordinary glaive
// slash on the ground can never paralyse a boss.
u8 ByrnaIg_AirSuperDamage(void) {
    if (!ByrnaOrb_IsCharged()) {
        return 0;
    }
    return (sByIg.state == BYIG_SPIN_AIR) || (sByIg.state == BYIG_DASH) || (sByIg.state == BYIG_POUND);
}

static void ByrnaIg_Behavior(Player* player, PlayState* play) {
    if (!sByIg.inited) {
        sByIg.inited = 1;
        sByIg.state = BYIG_GROUND;
    }

    // Idempotent, so re-running every frame costs a flag test — and it self-heals
    // if anything else stomped the tables.
    ByrnaIg_InstallAnims();
    if (Player_GetMeleeWeaponHeld(player) != 0) {
        ByrnaIg_InstallLoco();
    } else {
        ByrnaIg_RestoreLoco();
    }

    if (sByIg.state != BYIG_GROUND) {
        ByrnaIg_TickAir(play, player);
        return;
    }
    ByrnaIg_TickCombo();
}

static void ByrnaIg_Cleanup(void) {
    // Unconditional: these are global engine tables, so they come back even if the
    // behavior never ran this session.
    ByrnaIg_RestoreAnims();
    ByrnaIg_RestoreLoco();
    sByIg.state = BYIG_GROUND;
    sByIg.timer = 0;
    sByIgComboStep = 0;
    sByIgComboIdle = 0;
    sByIgQuadInited = 0;
}
