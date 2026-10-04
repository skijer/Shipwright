/**
 * power_keg.c - Power Keg (MM Goron's big bomb), Skijer's NEI.
 *
 * Shares the Bomb slot via a kaleido wheel (A opens, stick cycles Bomb <-> Power Keg). Equippable to a
 * C-button when owned; USE is gated by form + strength (Fierce Deity / Goron, or Human/Gerudo with
 * Silver Gauntlets+). Its own ammo counter (powerKegCount); each use costs 1 keg.
 *
 * Behavior (user spec): you DROP a keg with a lit fuse (a real En_Bom is the visible, ticking bomb).
 * After the fuse it explodes — destroying every breakable boulder + Golden-Gauntlets heavy block in
 * range and one-shotting the lighter Iron-Knuckle's-Axe props, plus real bomb damage to enemies in a
 * wide radius. Heavy blocks are Actor_Kill'd DIRECTLY (never via their 12-debris SpawnPieces, which
 * overflowed the actor pool and crashed when a whole field was hit at once).
 *
 * #included into custom_items.c AFTER item_spinner.c (DestroyBreakable/IsBreakableRock) and
 * weapon_upgrades.c (WeaponUpgrade_IKAxeStrike), so it shares their unity TU.
 */
#include "../../nei_save.h"                      // Skijer's NEI
#include "../helpers/combat_helper.h"            // Combat_DamageEnemiesInRadius
#include "overlays/actors/ovl_En_Bom/z_en_bom.h" // EnBom (the visible fuse bomb's timer)

// Current transformation form (mm_player_form.cpp, extern "C"). Values mirror MmPlayerTransformation.
extern MmPlayerTransformation MmForm_GetCurrentForm(void);
#define PK_FORM_FIERCE_DEITY 0
#define PK_FORM_GORON 1
#define PK_FORM_HUMAN 4
#define PK_FORM_GERUDO 7

// 3x bomb explosion VFX (z_effect_soft_sprite_old_init.c) — layered on top of the En_Bom's own blast.
extern void EffectSsBomb2_SpawnLayered(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale,
                                       s16 scaleStep);
extern void EffectSsBlast_SpawnWhiteShockwave(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel);

// MM asset loader: pulls the real Powder Keg get-item model out of mm.o2r so the dropped bomb LOOKS
// like the keg barrel instead of an OOT bomb (En_Bom delegates its Draw to PowerKeg_DrawKegModel).
extern void* MmAssets_LoadResource(const char* path);

#define POWER_KEG_RADIUS 250.0f          // enemy-damage reach
#define POWER_KEG_OBSTACLE_RADIUS 350.0f // boulder + heavy-block reach (clears a cluster/wall)
#define POWER_KEG_DAMAGE 8               // real bomb damage amount (z_en_bom.c explosion toucher = 8)
#define POWER_KEG_FUSE 100               // fuse length in frames (~1.6s) — drop it and step back, MM-style
#define PK_KEG_SCALE \
    0.333f // ABSOLUTE draw scale of the GI keg model (1/3 of the original
           // 1.0 that read too big; GI models sit near ~0.4) — tune

static Actor* sKegBomb = NULL; // the En_Bom we spawned as the keg (held or dropped); drives its Draw + blast

// Ownership + ammo (granted via menu / save editor), persisted in the NEI save like the pictobox.
u8 PowerKeg_IsOwned(void) {
    return Nei_Save()->powerKegOwned;
}
void PowerKeg_SetOwned(u8 on) {
    Nei_Save()->powerKegOwned = on ? 1 : 0;
}
u8 PowerKeg_GetCount(void) {
    return Nei_Save()->powerKegCount;
}
// The Infinite-ammo cheat is the "infinite bomb bag": unlimited kegs (no cap, never depletes).
static u8 PowerKeg_Infinite(void) {
    extern s32 CVarGetInteger(const char* name, s32 defaultValue);
    return CVarGetInteger(CVAR_CHEAT("InfiniteAmmo"), 0) != 0;
}
// Max kegs the player can carry: flat 5 (Fleet Ship Combo parity with MM's Extra Powder Kegs);
// the infinite bomb bag makes it effectively unlimited.
u8 PowerKeg_MaxCount(void) {
    if (PowerKeg_Infinite()) {
        return 99;
    }
    return 5;
}
void PowerKeg_SetCount(u8 n) {
    u8 max = PowerKeg_MaxCount();
    Nei_Save()->powerKegCount = (n > max) ? max : n;
}

// Power-keg mode on the Bomb slot (toggled by the kaleido wheel in z_kaleido_item.c). Persisted in the
// NEI save so the slot stays in keg mode across reloads (instead of reverting to bombs).
u8 PowerKeg_IsOnBombActive(void) {
    return Nei_Save()->powerKegMode;
}
void PowerKeg_SetOnBombActive(u8 on) {
    Nei_Save()->powerKegMode = on ? 1 : 0;
}

// The form/strength requirement: Fierce Deity / Goron / Gerudo can heft it with just Strength 1 (Goron
// Bracelet); base Link (Human) needs Silver Gauntlets (Strength 2). The weaker forms (Deku/Zora/Pikachu/
// Garo) can't lift a keg at all.
u8 PowerKeg_CanUse(void) {
    int form = MmForm_GetCurrentForm();
    int str = CUR_UPG_VALUE(UPG_STRENGTH);
    if (form == PK_FORM_FIERCE_DEITY || form == PK_FORM_GORON || form == PK_FORM_GERUDO) {
        return (str >= 1) ? 1 : 0;
    }
    if (form == PK_FORM_HUMAN) {
        return (str >= 2) ? 1 : 0;
    }
    return 0;
}

// --- IKAxe-prop blast: one-shots the lighter gauntlet props (jya block, Ishi, Haheniron, goroiwa) ---
// WeaponUpgrade_IKAxeStrike normally needs an active hammer swing; this lets it answer for those props
// inside the keg blast. Heavy blocks are NOT handled here (they go through PowerKeg_DestroyObstacles'
// direct Actor_Kill — their SpawnPieces would overflow the pool). Auto-expires by frame.
static Vec3f sPkBlastCenter;
static f32 sPkBlastRadius = 0.0f;
static s32 sPkBlastFrame = -1000;

void PowerKeg_SetBlast(Vec3f* center, f32 radius, s32 frame) {
    sPkBlastCenter = *center;
    sPkBlastRadius = radius;
    sPkBlastFrame = frame;
}

u8 PowerKeg_BlastQuery(Actor* actor, PlayState* play) {
    s32 dframe;
    if (actor == NULL || play == NULL || sPkBlastRadius <= 0.0f) {
        return 0;
    }
    if (actor->id == ACTOR_BG_HEAVY_BLOCK) {
        return 0; // heavy blocks are Actor_Kill'd directly (no 12-debris SpawnPieces overflow)
    }
    dframe = (s32)play->gameplayFrames - sPkBlastFrame;
    if (dframe < 0 || dframe > 3) {
        return 0; // blast active a few frames (covers every in-range actor's update)
    }
    return (Math_Vec3f_DistXYZ(&sPkBlastCenter, &actor->world.pos) <= sPkBlastRadius) ? 1 : 0;
}

// Destroy every breakable obstacle around the keg: heavy blocks (Golden-Gauntlets pillars) via direct
// Actor_Kill — NOT their SpawnPieces (12 debris actors each, which crashed when a field was hit) — and
// the Spinner's breakable boulders via its own light DestroyBreakable. Centered on the keg, not Link.
static void PowerKeg_DestroyObstacles(PlayState* play, Vec3f* center, f32 radius) {
    f32 rSq = radius * radius;
    s32 cat;
    for (cat = 0; cat < 2; cat++) {
        Actor* a = play->actorCtx.actorLists[(cat == 0) ? ACTORCAT_BG : ACTORCAT_PROP].head;
        while (a != NULL) {
            Actor* next = a->next;
            if (a->update != NULL) {
                f32 dx = a->world.pos.x - center->x;
                f32 dz = a->world.pos.z - center->z;
                if (((dx * dx) + (dz * dz)) <= rSq) {
                    if (a->id == ACTOR_BG_HEAVY_BLOCK) {
                        s32 type = a->params & 0xFF;
                        if (type != 2 /*BIG_PIECE*/ && type != 3 /*SMALL_PIECE*/) {
                            Actor_Kill(a); // direct — no SpawnPieces
                        }
                    } else if (IsBreakableRock(a->id)) {
                        DestroyBreakable(play, a); // Spinner's light break (VFX + Actor_Kill)
                    }
                }
            }
            a = next;
        }
    }
}

// The actual blast (when the fuse runs out): a big 3x explosion VFX + real bomb damage to enemies, plus
// the obstacle destruction and the IKAxe-prop blast. The visible En_Bom adds its own blast + sfx.
static void PowerKeg_Explode(PlayState* play, Vec3f* center) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    EffectSsBomb2_SpawnLayered(play, center, &zero, &zero, 300, 57);
    EffectSsBlast_SpawnWhiteShockwave(play, center, &zero, &zero);
    Audio_PlaySoundGeneral(NA_SE_IT_BOMB_EXPLOSION, center, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);

    Combat_DamageEnemiesInRadius(play, center, POWER_KEG_RADIUS, 0, POWER_KEG_DAMAGE);
    PowerKeg_DestroyObstacles(play, center, POWER_KEG_OBSTACLE_RADIUS);
    PowerKeg_SetBlast(center, POWER_KEG_OBSTACLE_RADIUS, (s32)play->gameplayFrames);
}

// Per-frame watcher (hooked from z_player.c, runs in every form). Our keg is a real En_Bom, so it ticks
// its own fuse and explodes; when it does (it may have been thrown), add the Power Keg's extra blast
// (3x VFX + obstacle destruction) at its ACTUAL position, then stop tracking it.
void PowerKeg_Update(PlayState* play) {
    if (sKegBomb == NULL) {
        return;
    }
    if (sKegBomb->update == NULL || sKegBomb->params != BOMB_BODY) {
        PowerKeg_Explode(play, &sKegBomb->world.pos);
        sKegBomb = NULL;
    }
}

// Called from EnBom_Draw: when the bomb is OUR placed keg (and still the intact body, not mid-explosion),
// draw the real MM Powder Keg get-item model (barrel = Opa, Goron-skull + fuse = Xlu, like MM's
// GetItem_DrawOpa0Xlu1) using the actor's already-set transform, and skip the vanilla bomb model.
// Returns 1 if it drew the keg (caller should return), 0 to let the vanilla bomb draw run. Skijer's NEI
u8 PowerKeg_DrawKegModel(Actor* thisx, PlayState* play) {
    static void* sBarrelDL = NULL;
    static void* sSkullFuseDL = NULL;
    static u8 sTried = 0;

    if (thisx != sKegBomb || thisx->params != BOMB_BODY) {
        return 0; // not our keg, or it's already in the explosion state — let the bomb draw run
    }
    if (!sTried) {
        sTried = 1;
        sBarrelDL = MmAssets_LoadResource("__OTR__objects/object_gi_bigbomb/gGiPowderKegBarrelDL");
        sSkullFuseDL = MmAssets_LoadResource("__OTR__objects/object_gi_bigbomb/gGiPowderKegGoronSkullAndFuseDL");
    }
    if (sBarrelDL == NULL) {
        return 0; // mm.o2r missing the model — fall back to the vanilla bomb
    }

    OPEN_DISPS(play->state.gfxCtx);
    // Fresh world transform at the keg's position (decoupled from the actor's collision scale).
    Matrix_SetTranslateRotateYXZ(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, &thisx->shape.rot);
    Matrix_Scale(PK_KEG_SCALE, PK_KEG_SCALE, PK_KEG_SCALE, MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBarrelDL);

    if (sSkullFuseDL != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSkullFuseDL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return 1;
}

// In-game use (hooked into Player_InitExplosiveIA for base Link, and TransformMasks for the MM forms):
// pull the keg out and HOLD it exactly like a bomb.
// Mirrors Player_InitExplosiveIA — spawn-as-child + carry state — so Link grabs it, can walk, and throws
// it; it explodes on its fuse. Returns 1 if it took over the bomb pull (keg mode), 0 for a normal bomb.
u8 PowerKeg_TryPull(PlayState* play, Player* this) {
    Actor* keg;

    if (!PowerKeg_IsOwned() || !PowerKeg_IsOnBombActive()) {
        return 0; // not keg mode — let the real bomb pull run
    }
    if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
        Player_PutAwayHeldItem(play, this); // already holding it — put it away (toggle, like a bomb)
        return 1;
    }
    if (!PowerKeg_CanUse()) {
        return 1; // owns it but wrong form / not enough strength — pull nothing
    }
    if (!PowerKeg_Infinite() && (Nei_Save()->powerKegCount < 1)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR); // out of Power Kegs
        return 1;
    }

    keg = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_BOM, this->actor.world.pos.x,
                             this->actor.world.pos.y, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0, 0);
    if (keg != NULL) {
        if (!PowerKeg_Infinite()) {
            Nei_Save()->powerKegCount--;
        }
        ((EnBom*)keg)->timer = POWER_KEG_FUSE;
        this->interactRangeActor = keg;
        this->heldActor = keg;
        this->getItemId = GI_NONE;
        this->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
        this->unk_3BC.y = keg->shape.rot.y - this->actor.shape.rot.y;
        this->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
        sKegBomb = keg;
    }
    return 1;
}
