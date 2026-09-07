/**
 * four_sword_clone.c - the Four Sword clone actor. Unity-#included by ext_equip_behavior.c;
 * registration and the tunic hook live in four_sword_clone_reg.cpp.
 */

#define FSC_MAX 3

typedef enum {
    FSC_FORM_TRIANGLE,
    FSC_FORM_LINE,
    FSC_FORM_GRID,
    FSC_FORM_CROSS,
    FSC_FORM_TOTEM,
    FSC_FORM_DISPERSE,
    FSC_FORM_MAX,
} FourSwordFormation;

typedef struct {
    f32 leftOfLink;
    f32 aheadOfLink;
    f32 aboveLink;
} FourSwordSlot;

// Shoulder to shoulder: two Link radii is 34, so this is barely wider than the pushing huddle.
#define FSC_R 45.0f
#define FSC_STACK 40.0f
#define FSC_VARIANT_MAX 4

// apps/gen_four_sword_icons.py renders the wheel art from this table — edit one, rerun it.
static const FourSwordSlot sFormationSlots[FSC_FORM_MAX][FSC_VARIANT_MAX][FSC_MAX] = {
    { { { 0.0f, FSC_R, 0.0f }, { 69.3f, -40.0f, 0.0f }, { -69.3f, -40.0f, 0.0f } } },
    {
        { { -FSC_R, 0.0f, 0.0f }, { -2 * FSC_R, 0.0f, 0.0f }, { -3 * FSC_R, 0.0f, 0.0f } },
        { { FSC_R, 0.0f, 0.0f }, { 2 * FSC_R, 0.0f, 0.0f }, { 3 * FSC_R, 0.0f, 0.0f } },
    },
    { { { -FSC_R, 0.0f, 0.0f }, { 0.0f, -FSC_R, 0.0f }, { -FSC_R, -FSC_R, 0.0f } } },
    {
        { { FSC_R, -FSC_R, 0.0f }, { -FSC_R, -FSC_R, 0.0f }, { 0.0f, -2 * FSC_R, 0.0f } },
        { { FSC_R, FSC_R, 0.0f }, { -FSC_R, FSC_R, 0.0f }, { 0.0f, 2 * FSC_R, 0.0f } },
        { { -FSC_R, FSC_R, 0.0f }, { -FSC_R, -FSC_R, 0.0f }, { -2 * FSC_R, 0.0f, 0.0f } },
        { { FSC_R, FSC_R, 0.0f }, { FSC_R, -FSC_R, 0.0f }, { 2 * FSC_R, 0.0f, 0.0f } },
    },
    { { { 0.0f, 0.0f, FSC_STACK }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 2 * FSC_STACK } } },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static const u8 sFormationVariantCount[FSC_FORM_MAX] = { 1, 2, 1, 4, 1, 1 };

const Color_RGB8 gFourSwordCloneTints[FSC_MAX] = {
    { 190, 40, 40 },
    { 40, 70, 200 },
    { 140, 40, 190 },
};
static const Color_RGB8 gFourSwordLinkGreen = { 30, 105, 27 };

s32 gFourSwordCloneDrawing = -1;

#define FSC_BLADE_RADIUS 25
#define FSC_PUSH_SPREAD 34.0f
#define FSC_SCALE 0.01f
#define FSC_SUMMON_FRAMES 12
#define FSC_SLOT_PULL 0.45f
#define FSC_SWING_FRAMES 8

#define FSC_WALL_HEIGHT 26.0f
#define FSC_WALL_RADIUS 10.0f
#define FSC_BGCHECK_FLAGS 0x5

typedef struct {
    Actor actor;
    ColliderCylinder hurtbox;
    ColliderCylinder blade;
    ColliderQuad shield;
    Vec3s pose[PLAYER_LIMB_BUF_COUNT];
    s32 trailEffectIndex;
    WeaponInfo trailInfo[3];
    MtxF swordHandMtx;
    u8 swordHandMtxValid;
    u8 index;
    u8 colInit;
    s16 summonTimer;
    s16 swingTimer;
} FourSwordClone;

size_t gFourSwordCloneStructSize = sizeof(FourSwordClone);

// OC excludes TYPE_PLAYER so a formation can never wall Link into a corridor.
static ColliderCylinderInit sHurtboxInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_ENEMY, OC1_ON | OC1_TYPE_1 | OC1_TYPE_2, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFFFFFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_ON },
    { 18, 46, 0, { 0, 0, 0 } },
};

// Copied from the player's own D_808546A0: a clone's shield has to block like the real one.
static ColliderQuadInit sShieldQuadInit = {
    { COLTYPE_METAL, AT_ON | AT_TYPE_PLAYER, AC_ON | AC_HARD | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_PLAYER,
      COLSHAPE_QUAD },
    { ELEMTYPE_UNK2,
      { 0x00100000, 0x00, 0x00 },
      { 0xDFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_ON,
      OCELEM_NONE },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderCylinderInit sBladeInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2,
      { 0x00000100, 0x00, 0x02 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      TOUCH_ON | TOUCH_SFX_NORMAL,
      BUMP_NONE,
      OCELEM_NONE },
    { FSC_BLADE_RADIUS, 60, 30, { 0, 0, 0 } },
};

static FourSwordClone* sLiveClones[FSC_MAX];
static u8 sFormation = FSC_FORM_TRIANGLE;
static u8 sVariant = 0;

s16 gFourSwordCloneId = -1;
extern void FourSwordClone_EnsureRegistered(void);

u8 FourSwordClone_Count(void) {
    u8 n = 0;
    for (int i = 0; i < FSC_MAX; i++) {
        if (sLiveClones[i] != NULL) {
            n++;
        }
    }
    return n;
}

u8 FourSwordClone_GetFormation(void) {
    return sFormation;
}

u8 FourSwordClone_GetVariant(void) {
    return sVariant;
}

u8 FourSwordClone_VariantCount(u8 formation) {
    return (formation < FSC_FORM_MAX) ? sFormationVariantCount[formation] : 1;
}

void FourSwordClone_SetFormation(u8 formation, u8 variant) {
    if (formation >= FSC_FORM_MAX) {
        return;
    }
    sFormation = formation;
    sVariant = (variant < sFormationVariantCount[formation]) ? variant : 0;
}

#define FSC_TOTEM_BASE_INDEX 1
#define FSC_TOTEM_TOP_INDEX (FSC_MAX - 1)
// The base clone is a real carryable: Link lifts it like a rock, and the stack rides on it.
static FourSwordClone* FourSwordClone_TotemBase(void) {
    return (sFormation == FSC_FORM_TOTEM) ? sLiveClones[FSC_TOTEM_BASE_INDEX] : NULL;
}

u8 FourSwordClone_IsTotem(void) {
    FourSwordClone* base = FourSwordClone_TotemBase();

    return (base != NULL) && (base->actor.parent != NULL) &&
           (gExtEquipBehavior.fourSwordCloneMask == ((1 << FSC_MAX) - 1));
}

// In a totem the REAL Link is the base and wears the bottom clone's purple, while the clone on top
// stands in as the green one. Returns 0 when whatever is drawing keeps its vanilla tunic.
u8 FourSwordClone_ActiveTint(Color_RGB8* out) {
    u8 totem = FourSwordClone_IsTotem();

    if (gFourSwordCloneDrawing >= 0) {
        u8 greenOnTop = totem && (gFourSwordCloneDrawing == FSC_TOTEM_TOP_INDEX);

        *out = greenOnTop ? gFourSwordLinkGreen : gFourSwordCloneTints[gFourSwordCloneDrawing];
        return 1;
    }
    if (totem) {
        *out = gFourSwordCloneTints[FSC_TOTEM_TOP_INDEX];
        return 1;
    }
    return 0;
}

// Reads the MASK, not the live actors: a room change kills them and Reconcile rebuilds from the
// mask, so the grant must not blink off in between.
u8 FourSword_GridPushesAnyBlock(void) {
    extern u8 FourSword_IsEquipped(void);

    return FourSword_IsEquipped() && (sFormation == FSC_FORM_GRID) &&
           (gExtEquipBehavior.fourSwordCloneMask == ((1 << FSC_MAX) - 1));
}

u8 FourSwordClone_GetPos(u8 index, Vec3f* out) {
    if ((index >= FSC_MAX) || (sLiveClones[index] == NULL) || (out == NULL)) {
        return 0;
    }
    *out = sLiveClones[index]->actor.world.pos;
    return 1;
}

u8 FourSwordClone_TotemTopPos(Vec3f* out) {
    return FourSwordClone_IsTotem() && FourSwordClone_GetPos(FSC_TOTEM_TOP_INDEX, out);
}

Actor* FourSwordClone_TotemBaseActor(void) {
    FourSwordClone* base = FourSwordClone_TotemBase();

    return (base != NULL) ? &base->actor : NULL;
}

#define FSC_TOTEM_CAMERA_SCALE 1.6f

f32 FourSword_CameraDistanceScale(void) {
    return FourSwordClone_IsTotem() ? FSC_TOTEM_CAMERA_SCALE : 1.0f;
}

void FourSwordClone_KillAll(void) {
    for (int i = 0; i < FSC_MAX; i++) {
        if (sLiveClones[i] != NULL) {
            Actor_Kill(&sLiveClones[i]->actor);
            sLiveClones[i] = NULL;
        }
    }
    gExtEquipBehavior.fourSwordCloneMask = 0;
}

Actor* FourSwordClone_Spawn(PlayState* play, Player* player, u8 index) {
    if ((play == NULL) || (player == NULL) || (index >= FSC_MAX) || (sLiveClones[index] != NULL)) {
        return NULL;
    }
    FourSwordClone_EnsureRegistered();
    if (gFourSwordCloneId < 0) {
        return NULL;
    }
    gExtEquipBehavior.fourSwordCloneMask |= (1 << index);
    return Actor_Spawn(&play->actorCtx, play, gFourSwordCloneId, player->actor.world.pos.x, player->actor.world.pos.y,
                       player->actor.world.pos.z, 0, player->actor.shape.rot.y, 0, index);
}

// Actors die with the scene, the mask does not — this is what carries a summon across a room change.
void FourSwordClone_Reconcile(PlayState* play, Player* player) {
    u8 mask = gExtEquipBehavior.fourSwordCloneMask;

    for (u8 i = 0; i < FSC_MAX; i++) {
        if ((mask & (1 << i)) && (sLiveClones[i] == NULL)) {
            FourSwordClone_Spawn(play, player, i);
        }
    }
}

static u8 FourSwordClone_IsPushing(Player* player) {
    return (sFormation == FSC_FORM_GRID) && (player->stateFlags2 & PLAYER_STATE2_GRABBING_DYNAPOLY);
}

#define FSC_SPIN_RADIUS_SCALE 2.5f
#define FSC_SPIN_STEP 0x0700

static s16 sSpinOrbit;

static u8 FourSwordClone_IsCrossCharging(Player* player) {
    return (sFormation == FSC_FORM_CROSS) && (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK);
}

static u8 FourSwordClone_IsCrossSpinning(Player* player) {
    return (sFormation == FSC_FORM_CROSS) && (player->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING);
}

// The cross puts Link on one arm, so its hub is the average of the four bodies, not his feet.
static void FourSwordClone_CrossHub(f32* leftOut, f32* aheadOut) {
    f32 left = 0.0f;
    f32 ahead = 0.0f;

    for (int i = 0; i < FSC_MAX; i++) {
        left += sFormationSlots[FSC_FORM_CROSS][sVariant][i].leftOfLink;
        ahead += sFormationSlots[FSC_FORM_CROSS][sVariant][i].aheadOfLink;
    }
    *leftOut = left / (FSC_MAX + 1);
    *aheadOut = ahead / (FSC_MAX + 1);
}

// Totem slots hang off the BASE clone: in Link's hands once lifted, at arm's reach before.
// Actor_OfferGetItemNearby reaches 50 units in XZ and only 10 in Y — the base has to park well
// inside both or the offer never fires.
#define FSC_GRAB_REACH 30.0f

// The totem hangs off the BASE clone, not off Link: the base parks itself within grab reach and
// the other two stack on wherever it ends up, which is in his hands once he lifts it.
static Vec3f FourSwordClone_SlotPos(Player* player, u8 index) {
    const FourSwordSlot* slot = &sFormationSlots[sFormation][sVariant][index];
    f32 sin = Math_SinS(player->actor.shape.rot.y);
    f32 cos = Math_CosS(player->actor.shape.rot.y);
    Vec3f out = player->actor.world.pos;

    if (sFormation == FSC_FORM_TOTEM) {
        FourSwordClone* base = sLiveClones[FSC_TOTEM_BASE_INDEX];

        if (index == FSC_TOTEM_BASE_INDEX) {
            out.x += sin * FSC_GRAB_REACH;
            out.z += cos * FSC_GRAB_REACH;
            return out;
        }
        if (base != NULL) {
            out = base->actor.world.pos;
        }
        out.y += slot->aboveLink;
        return out;
    }

    f32 spread = FourSwordClone_IsPushing(player) ? (FSC_PUSH_SPREAD / FSC_R) : 1.0f;
    f32 ahead = slot->aheadOfLink * spread;
    f32 left = slot->leftOfLink * spread;

    // Spinning, the four of them orbit the hub as one wheel: each body walks the next one's post
    // and the whole ring opens up.
    if (sFormation == FSC_FORM_CROSS) {
        f32 hubLeft;
        f32 hubAhead;

        FourSwordClone_CrossHub(&hubLeft, &hubAhead);

        f32 armLeft = left - hubLeft;
        f32 armAhead = ahead - hubAhead;
        f32 reach = FourSwordClone_IsCrossSpinning(player) ? FSC_SPIN_RADIUS_SCALE : 1.0f;
        f32 orbitSin = Math_SinS(sSpinOrbit);
        f32 orbitCos = Math_CosS(sSpinOrbit);

        left = hubLeft + ((armLeft * orbitCos) - (armAhead * orbitSin)) * reach;
        ahead = hubAhead + ((armLeft * orbitSin) + (armAhead * orbitCos)) * reach;
    }

    out.x += (ahead * sin) + (left * cos);
    out.z += (ahead * cos) - (left * sin);
    return out;
}

// Link is one of the four bodies, so the wheel only reads if he rides it too. Called from the
// behavior, after Player_UpdateCommon, which is the only place his position stays overwritten.
void FourSwordClone_SpinTick(Player* player) {
    if (!FourSwordClone_IsCrossSpinning(player)) {
        sSpinOrbit = 0;
        return;
    }

    sSpinOrbit += FSC_SPIN_STEP;

    f32 hubLeft;
    f32 hubAhead;

    FourSwordClone_CrossHub(&hubLeft, &hubAhead);

    f32 armLeft = -hubLeft * FSC_SPIN_RADIUS_SCALE;
    f32 armAhead = -hubAhead * FSC_SPIN_RADIUS_SCALE;
    f32 orbitSin = Math_SinS(sSpinOrbit);
    f32 orbitCos = Math_CosS(sSpinOrbit);
    f32 left = hubLeft + (armLeft * orbitCos) - (armAhead * orbitSin);
    f32 ahead = hubAhead + (armLeft * orbitSin) + (armAhead * orbitCos);
    f32 sin = Math_SinS(player->actor.shape.rot.y);
    f32 cos = Math_CosS(player->actor.shape.rot.y);

    player->actor.world.pos.x += (ahead * sin) + (left * cos);
    player->actor.world.pos.z += (ahead * cos) - (left * sin);
}

static void FourSwordClone_CrossHubWorld(Player* player, Vec3f* out) {
    f32 hubLeft;
    f32 hubAhead;
    f32 sin = Math_SinS(player->actor.shape.rot.y);
    f32 cos = Math_CosS(player->actor.shape.rot.y);

    FourSwordClone_CrossHub(&hubLeft, &hubAhead);
    *out = player->actor.world.pos;
    out->x += (hubAhead * sin) + (hubLeft * cos);
    out->z += (hubAhead * cos) - (hubLeft * sin);
}

// Facing is radial: backs to the hub normally, blades turned inward while the spin charges.
static s16 FourSwordClone_CrossFacing(Player* player, Vec3f* slotPos) {
    Vec3f hub;

    FourSwordClone_CrossHubWorld(player, &hub);

    s16 outward = Math_Atan2S(slotPos->z - hub.z, slotPos->x - hub.x);

    return FourSwordClone_IsCrossCharging(player) ? (outward + 0x8000) : outward;
}

static void FourSwordClone_Sparkle(PlayState* play, Vec3f* at, s32 count, f32 spread, f32 rise) {
    Vec3f vel = { 0.0f, rise, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };

    for (s32 p = 0; p < count; p++) {
        Vec3f pos = {
            at->x + Rand_CenteredFloat(spread),
            at->y + 30.0f + Rand_ZeroFloat(spread),
            at->z + Rand_CenteredFloat(spread),
        };
        vel.x = Rand_CenteredFloat(2.0f);
        vel.z = Rand_CenteredFloat(2.0f);
        EffectSsKiraKira_SpawnSmall(play, &pos, &vel, &accel, &(Color_RGBA8){ 150, 210, 255, 255 },
                                    &(Color_RGBA8){ 60, 110, 255, 0 });
    }
}

// Link's own blureSword init with the livery RGB swapped in; the alphas ARE the fade.
static void FourSwordClone_AllocTrail(PlayState* play, FourSwordClone* self) {
    const Color_RGB8* tint = &gFourSwordCloneTints[self->index];
    EffectBlureInit2 init = {
        0,
        8,
        0,
        { tint->r, tint->g, tint->b, 255 },
        { tint->r, tint->g, tint->b, 64 },
        { tint->r, tint->g, tint->b, 0 },
        { tint->r, tint->g, tint->b, 0 },
        4,
        0,
        2,
        0,
        { tint->r, tint->g, tint->b, 255 },
        { tint->r, tint->g, tint->b, 64 },
        TRAIL_TYPE_SWORDS,
    };

    Effect_Add(play, &self->trailEffectIndex, EFFECT_BLURE2, 0, 0, &init);
    memset(self->trailInfo, 0, sizeof(self->trailInfo));
}

// EffectBlure_Update repaints the strip from the cosmetic Trails CVars every frame.
static void FourSwordClone_StampTrailColor(FourSwordClone* self) {
    EffectBlure* trail = Effect_GetByIndex(self->trailEffectIndex);

    if (trail == NULL) {
        return;
    }

    const Color_RGB8* tint = &gFourSwordCloneTints[self->index];
    Color_RGBA8* strips[4] = { &trail->p1StartColor, &trail->p2StartColor, &trail->p1EndColor, &trail->p2EndColor };

    for (int i = 0; i < 4; i++) {
        strips[i]->r = tint->r;
        strips[i]->g = tint->g;
        strips[i]->b = tint->b;
    }
}

void FourSwordClone_Init(Actor* thisx, PlayState* play) {
    FourSwordClone* self = (FourSwordClone*)thisx;

    // Before any early return: index 0 is EffectSpark 0, so without the sentinel a Destroy that
    // follows the bounds check below would free someone else's effect.
    self->trailEffectIndex = TOTAL_EFFECT_COUNT;

    self->index = (u8)(self->actor.params & 3);
    if (self->index >= FSC_MAX) {
        Actor_Kill(&self->actor);
        return;
    }

    Actor_SetScale(&self->actor, FSC_SCALE);
    ActorShape_Init(&self->actor.shape, 0.0f, ActorShadow_DrawCircle, 30.0f);

    Collider_InitCylinder(play, &self->hurtbox);
    Collider_SetCylinder(play, &self->hurtbox, &self->actor, &sHurtboxInit);
    Collider_InitCylinder(play, &self->blade);
    Collider_SetCylinder(play, &self->blade, &self->actor, &sBladeInit);
    Collider_InitQuad(play, &self->shield);
    Collider_SetQuad(play, &self->shield, &self->actor, &sShieldQuadInit);
    self->colInit = 1;

    self->actor.gravity = -2.0f;
    self->summonTimer = FSC_SUMMON_FRAMES;
    self->swingTimer = 0;
    FourSwordClone_AllocTrail(play, self);

    sLiveClones[self->index] = self;
    FourSwordClone_Sparkle(play, &self->actor.world.pos, 6, 20.0f, 1.5f);
    Audio_PlayActorSound2(&self->actor, NA_SE_SY_LOCK_ON);
}

void FourSwordClone_Destroy(Actor* thisx, PlayState* play) {
    FourSwordClone* self = (FourSwordClone*)thisx;

    // EffectBlure_Update never retires a slot, so re-summoning without this exhausts all 25.
    Effect_Delete(play, self->trailEffectIndex);
    self->trailEffectIndex = TOTAL_EFFECT_COUNT;

    if (self->colInit) {
        Collider_DestroyCylinder(play, &self->hurtbox);
        Collider_DestroyCylinder(play, &self->blade);
        Collider_DestroyQuad(play, &self->shield);
    }
    if ((self->index < FSC_MAX) && (sLiveClones[self->index] == self)) {
        sLiveClones[self->index] = NULL;
    }
}

void FourSwordClone_Update(Actor* thisx, PlayState* play) {
    extern u8 FourSword_IsEquipped(void);
    FourSwordClone* self = (FourSwordClone*)thisx;
    Player* player = GET_PLAYER(play);

    if (!FourSword_IsEquipped()) {
        Actor_Kill(&self->actor);
        return;
    }

    // The four of them are ONE Link: a clone does not die, it passes the hit on to the body that
    // owns the hearts. CollisionCheck_SetAC clears AC_HIT, so it is read before re-registering.
    if (self->hurtbox.base.acFlags & AC_HIT) {
        ColliderInfo* hitBy = self->hurtbox.info.acHitInfo;
        s32 damage = (hitBy != NULL) ? hitBy->toucher.damage : 0;

        self->hurtbox.base.acFlags &= ~AC_HIT;
        if ((damage > 0) && (play->damagePlayer != NULL)) {
            play->damagePlayer(play, -damage);
        }
        FourSwordClone_Sparkle(play, &self->actor.world.pos, 4, 20.0f, 1.5f);
    }

    if (self->summonTimer > 0) {
        self->summonTimer--;
    }

    u8 totemBase = (sFormation == FSC_FORM_TOTEM) && (self->index == FSC_TOTEM_BASE_INDEX);
    u8 carried = totemBase && (self->actor.parent != NULL);
    u8 stacked = (sFormation == FSC_FORM_TOTEM) && !totemBase;

    // The offer has to be renewed EVERY frame: the player clears interactRangeActor at the top of
    // its update and Player_Action_80846050 only reads it on frame 4 of the lift, when it finally
    // attaches. Starting that action more than once restarts the animation and it never reaches
    // frame 4 — which looks exactly like Link miming a lift forever.
    if (totemBase && !carried && (player->heldActor == NULL)) {
        extern u8 FourSword_TotemGrabAllowed(void);
        extern void func_8083A0F4(PlayState * play, Player * this);
        extern void Player_Action_80846050(Player * this, PlayState * play);

        if (FourSword_TotemGrabAllowed()) {
            Actor_OfferCarry(&self->actor, play);
            if ((player->interactRangeActor == &self->actor) && (player->actionFunc != Player_Action_80846050)) {
                player->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
                func_8083A0F4(play, player);
            }
        }
    }

    // Once lifted, the player owns the base's position (it rides in his hands).
    if ((sFormation != FSC_FORM_DISPERSE) && !carried) {
        Vec3f slot = FourSwordClone_SlotPos(player, self->index);
        // The stack is rigid: the base is placed by the player itself, so any easing on the two
        // above it shows up as the tower coming apart while he walks.
        f32 settled = stacked ? 1.0f : FSC_SLOT_PULL;
        f32 pull = (self->summonTimer > 0) ? (1.0f - ((f32)self->summonTimer / FSC_SUMMON_FRAMES)) : settled;

        self->actor.world.pos.x += (slot.x - self->actor.world.pos.x) * pull;
        self->actor.world.pos.z += (slot.z - self->actor.world.pos.z) * pull;
        self->actor.shape.rot.y = (sFormation == FSC_FORM_CROSS)
                                      ? FourSwordClone_CrossFacing(player, &self->actor.world.pos)
                                      : player->actor.shape.rot.y;
        self->actor.world.rot.y = self->actor.shape.rot.y;
        if (stacked) {
            self->actor.world.pos.y += (slot.y - self->actor.world.pos.y) * pull;
            self->actor.velocity.y = 0.0f;
        }
    }

    self->actor.speedXZ = 0.0f;
    if (!stacked && !carried) {
        Actor_MoveXZGravity(&self->actor);
        Actor_UpdateBgCheckInfo(play, &self->actor, FSC_WALL_HEIGHT, FSC_WALL_RADIUS, 0.0f, FSC_BGCHECK_FLAGS);
        if (self->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            self->actor.velocity.y = 0.0f;
        }
    }

    Collider_UpdateCylinder(&self->actor, &self->hurtbox);
    CollisionCheck_SetAC(play, &play->colChkCtx, &self->hurtbox.base);
    // Closed up on a block they overlap by design; OC would shove them back out of formation.
    if (!FourSwordClone_IsPushing(player)) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &self->hurtbox.base);
    }

    if (player->meleeWeaponState > 0) {
        self->swingTimer = FSC_SWING_FRAMES;
    } else if (self->swingTimer > 0) {
        self->swingTimer--;
    }

    // Four blades meeting at the hub are ONE strike, so one clone carries a single fat cylinder
    // there and the rest hold their fire — otherwise the same hit lands four times.
    if (FourSwordClone_IsCrossCharging(player) || FourSwordClone_IsCrossSpinning(player)) {
        if (self->index == 0) {
            Vec3f hub;

            FourSwordClone_CrossHubWorld(player, &hub);
            self->blade.dim.radius = (s16)(FSC_BLADE_RADIUS * FSC_SPIN_RADIUS_SCALE);
            self->blade.dim.pos.x = (s16)hub.x;
            self->blade.dim.pos.y = (s16)hub.y;
            self->blade.dim.pos.z = (s16)hub.z;
            CollisionCheck_SetAT(play, &play->colChkCtx, &self->blade.base);
        }
    } else if (self->swingTimer > 0) {
        self->blade.dim.radius = FSC_BLADE_RADIUS;
        Collider_UpdateCylinder(&self->actor, &self->blade);
        CollisionCheck_SetAT(play, &play->colChkCtx, &self->blade.base);
    }
}

// Copying the live Player and overriding only transform and pose is what makes the sword, shield,
// sheath and tunic render at all; a bare SkelAnime_DrawFlexOpa draws the naked skeleton.
static Player sCloneDrawTemplate;

void FourSwordClone_Draw(Actor* thisx, PlayState* play) {
    FourSwordClone* self = (FourSwordClone*)thisx;
    Player* player = GET_PLAYER(play);

    if (player->skelAnime.skeleton == NULL) {
        return;
    }

    // The limb override writes back into rot[] (leg IK), so the clone poses from its own copy.
    s32 limbCount = player->skelAnime.limbCount;
    for (s32 j = 0; j <= limbCount; j++) {
        self->pose[j] = player->skelAnime.jointTable[j];
    }

    // Stacked clones stand to attention: Link's walk cycle on a body being carried reads as legs
    // pedalling in mid-air.
    if (sFormation == FSC_FORM_TOTEM) {
        for (s32 j = PLAYER_LIMB_R_THIGH; j <= PLAYER_LIMB_L_FOOT; j++) {
            self->pose[j].x = 0;
            self->pose[j].y = 0;
            self->pose[j].z = 0;
        }
    }

    // The player places the base during its POST-LIMB draw, after every update has run, so reading
    // it in Update always trails a frame. PLAYER draws before MISC, so here it is already final.
    if ((sFormation == FSC_FORM_TOTEM) && (self->index != FSC_TOTEM_BASE_INDEX)) {
        self->actor.world.pos = FourSwordClone_SlotPos(player, self->index);
    }

    Player* dp = &sCloneDrawTemplate;
    *dp = *player;
    dp->actor.world.pos = self->actor.world.pos;
    dp->actor.shape.rot = self->actor.shape.rot;
    dp->skelAnime.jointTable = self->pose;

    dp->meleeWeaponEffectIndex = self->trailEffectIndex;
    memcpy(dp->meleeWeaponInfo, self->trailInfo, sizeof(self->trailInfo));

    // The damage quads and the held actor live in this shared static template and still carry
    // Link's actor, so every clone would drive his. Damage comes from the `blade` cylinder instead.
    dp->meleeWeaponAnimation = PLAYER_MWA_SPIN_ATTACK_1H;
    dp->stateFlags2 &= ~PLAYER_STATE2_SPIN_ATTACKING;
    dp->heldActor = NULL;

    // Lent to the template the same way the trail is, so a raised clone shield blocks for real
    // instead of three clones all registering Link's one quad.
    dp->shieldQuad = self->shield;

    if (Effect_GetByIndex(self->trailEffectIndex) == NULL) {
        dp->meleeWeaponState = 0;
    }

    f32 yOff = dp->actor.shape.yOffset * dp->actor.scale.y;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_SetTranslateRotateYXZ(dp->actor.world.pos.x, dp->actor.world.pos.y + yOff, dp->actor.world.pos.z,
                                 &dp->actor.shape.rot);
    Matrix_Scale(dp->actor.scale.x, dp->actor.scale.y, dp->actor.scale.z, MTXMODE_APPLY);

    // Never write through VB_APPLY_TUNIC_COLOR's Color_RGB8*: it points at the global sTunicColors.
    gFourSwordCloneDrawing = self->index;
    Color_RGB8 tint;
    FourSwordClone_ActiveTint(&tint);
    gDPSetEnvColor(POLY_OPA_DISP++, tint.r, tint.g, tint.b, 0);

    Player_DrawImpl(play, dp->skelAnime.skeleton, dp->skelAnime.jointTable, dp->skelAnime.dListCount, 0,
                    dp->currentTunic, dp->currentBoots, dp->actor.shape.face, Player_OverrideLimbDrawGameplayDefault,
                    Player_PostLimbDrawGameplay, &dp->actor);

    // Without the write-back func_80090480 takes its first-frame branch forever.
    memcpy(self->trailInfo, dp->meleeWeaponInfo, sizeof(self->trailInfo));
    self->shield = dp->shieldQuad;
    FourSwordClone_StampTrailColor(self);

    // Post-limb wrote mf_9E0 on the Player it was HANDED, so this is our own left hand.
    self->swordHandMtx = dp->mf_9E0;
    self->swordHandMtxValid = 1;

    CustomItems_DrawForClone(dp, play);

    gFourSwordCloneDrawing = -1;
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

// One EnMThunder serves the whole charge: its Init consumes magic, inserts a light and dims the
// scene, so three more would fight Link's. The live one is read and its glow replayed per clone.
void FourSwordClone_DrawChargeGlowAll(EnMThunder* thunder, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((thunder->attackStrength == 2) || (thunder->chargeAlpha == 0)) {
        return;
    }

    // In the cross the four blades meet, so the charge is ONE glow at the hub, 2.5x wide, instead
    // of four in four hands. The scale has to include Link's own, since the vanilla placement is
    // written in hand-bone units.
    if (sFormation == FSC_FORM_CROSS) {
        Vec3f hub;
        MtxF hubMtx;

        FourSwordClone_CrossHubWorld(player, &hub);
        Matrix_SetTranslateRotateYXZ(hub.x, hub.y, hub.z, &player->actor.shape.rot);
        Matrix_Scale(FSC_SCALE * FSC_SPIN_RADIUS_SCALE, FSC_SCALE * FSC_SPIN_RADIUS_SCALE,
                     FSC_SCALE * FSC_SPIN_RADIUS_SCALE, MTXMODE_APPLY);
        Matrix_Get(&hubMtx);

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        EnMThunder_DrawChargeGlow(thunder, play, &hubMtx);
        return;
    }

    for (int i = 0; i < FSC_MAX; i++) {
        FourSwordClone* clone = sLiveClones[i];

        if ((clone == NULL) || !clone->swordHandMtxValid || (clone->summonTimer != 0)) {
            continue;
        }
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        EnMThunder_DrawChargeGlow(thunder, play, &clone->swordHandMtx);
    }
}

// The held-sword compound DL re-applies the PLAYER's tunic env after the blade, so every limb the
// walk reaches after L_HAND loses the tint — the torso last of all.
void FourSwordClone_ReapplyTint(PlayState* play) {
    Color_RGB8 tint;

    if (!FourSwordClone_ActiveTint(&tint)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, tint.r, tint.g, tint.b, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}
