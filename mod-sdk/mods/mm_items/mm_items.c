/**
 * Majora's Mask items: the Gorons' Powder Keg and the Pictograph Box.
 *
 * The keg is a real En_Bom wearing the MM barrel, so vanilla carries, throws and detonates it; the mod only
 * widens the blast. The pictograph reads the framebuffer, which the renderer answers a frame late, so its
 * capture is a small state machine.
 */

#include <string.h>

#include "z64items.h"
#include "z64aiming.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

#define MM_ITEMS_MOD "mm_items"
#define POWER_KEG_KEY "mm.power_keg"
#define PICTO_BOX_KEY "mm.picto_box"
// Wheels on the OoT grid: the keg next to the bombs, the camera next to the Lens of Truth.
#define MM_ITEMS_PAGE SOH_ITEM_PAGE_VANILLA
#define POWER_KEG_SLOT SLOT_BOMB
#define PICTO_BOX_SLOT SLOT_LENS
#define MM_ITEMS_BUTTON_COUNT 8

#define POWER_KEG_COUNT_FIELD "power_keg_count"
#define PICTO_FLAGS_FIELD "picto_flags"
#define PICTO_PHOTO_FIELD "picto_photo"

void FB_WriteFramebufferSliceToCPU(Gfx** gfxp, void* buffer, u8 byteSwap);
u8 ResourceMgr_FileExists(const char* resourcePath);

static const ALIGN_ASSET(2) char sKegIconTex[] = "__OTR__textures/icon_item_custom/gItemIconPowerKegTex";
static const ALIGN_ASSET(2) char sKegNameTex[] = "__OTR__textures/item_name_custom/gPowerKegNameTex";
static const ALIGN_ASSET(2) char sPictoIconTex[] = "__OTR__textures/icon_item_custom/gItemIconPictoBoxTex";
static const ALIGN_ASSET(2) char sPictoNameTex[] = "__OTR__textures/item_name_custom/gPictoBoxNameTex";

// Majora's Mask models, extracted by the mm_assets mod. Absent without it, which is why every use is gated
// on ResourceMgr_FileExists instead of the manifest's "requires": the items work either way.
static const ALIGN_ASSET(2) char sKegBarrelDL[] = "__OTR__objects/object_gi_bigbomb/gGiPowderKegBarrelDL";
static const ALIGN_ASSET(2) char sKegSkullDL[] = "__OTR__objects/object_gi_bigbomb/gGiPowderKegGoronSkullAndFuseDL";
static const ALIGN_ASSET(2) char sPictoBodyDL[] = "__OTR__objects/object_gi_camera/gGiPictoBoxBodyAndLensDL";
static const ALIGN_ASSET(2) char sPictoFrameDL[] = "__OTR__objects/object_gi_camera/gGiPictoBoxFrameDL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnActorDraw",        "OnItemReceive", "OnLoadFile",
                                              "OnSceneInit",    "OnInterfaceDrawEnd", "OnPlayDrawEnd" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const u16 sItemButtons[MM_ITEMS_BUTTON_COUNT] = { BTN_B,   BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT,
                                                         BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT };

static const SOHModApi* sApi;

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

// The button an item is equipped to, so a mod that reads raw input knows which press was its own.
static u16 GetEquippedButtons(const char* key) {
    u16 mask = 0;

    for (u8 button = 0; button < MM_ITEMS_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);

        if (equipped != NULL && strcmp(equipped, key) == 0) {
            mask |= sItemButtons[button];
        }
    }
    return mask;
}

// --- Powder Keg ---

#define POWER_KEG_MAX 5
#define POWER_KEG_FUSE 100               // half again the bomb's own 70 ticks: set it down and walk away
// A bomb blast tops out at a radius of 72, so the ring sits close enough for its four blasts and the keg's
// own to overlap into one wide one instead of a circle with a hole in it.
#define POWER_KEG_BLAST_RING 110.0f
#define POWER_KEG_OBSTACLE_RADIUS 350.0f // boulders and heavy blocks, enough to clear a wall
#define POWER_KEG_SATELLITES 4
#define POWER_KEG_BURST_SCALE 0.01f
#define POWER_KEG_BLAST_VFX_SCALE 300
#define POWER_KEG_BLAST_VFX_STEP 57
#define POWER_KEG_MODEL_SCALE 0.333f // GI models read too big at their own 1.0
#define HEAVY_BLOCK_BIG_PIECE 2
#define HEAVY_BLOCK_SMALL_PIECE 3

static u8 sKegCount;
static Actor* sKeg;
static Vec3f sKegPos;

static void StoreKegCount(void) {
    sApi->StorageSet(MM_ITEMS_MOD, POWER_KEG_COUNT_FIELD, &sKegCount, sizeof(sKegCount));
}

static void LoadKegCount(void) {
    sKegCount = 0;
    sApi->StorageGet(MM_ITEMS_MOD, POWER_KEG_COUNT_FIELD, &sKegCount, sizeof(sKegCount));
    if (sKegCount > POWER_KEG_MAX) {
        sKegCount = POWER_KEG_MAX;
    }
}

// A keg arrives full: every one found in OoT is a fresh crate of them, and there is no Goron to buy more from.
static void RestockKegs(const char* key) {
    sKegCount = POWER_KEG_MAX;
    StoreKegCount();
}

// Bombs and kegs are the same powder, so a bomb refill tops the crate up by one. Without it a spent keg is
// gone for the rest of the file: OoT has no shop that sells them.
static void RestockFromBombRefill(GetItemEntry entry) {
    if (entry.itemId < ITEM_BOMBS_5 || entry.itemId > ITEM_BOMBS_30 || sKegCount >= POWER_KEG_MAX) {
        return;
    }
    sKegCount++;
    StoreKegCount();
}

static int32_t GetKegCount(const char* key) {
    return sKegCount;
}

static bool CanUsePowerKeg(Player* player, PlayState* play) {
    if (sKegCount == 0 || CUR_UPG_VALUE(UPG_STRENGTH) < 2) {
        return false;
    }
    return (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) == 0;
}

// Heavy blocks die by Actor_Kill and never by their own break: each one spawns twelve debris actors, and a
// field of them at once overflowed the actor pool.
static void ShatterObstacles(PlayState* play, Vec3f* center) {
    static const s16 sBreakableIds[] = { ACTOR_EN_ISHI, ACTOR_OBJ_HAMISHI, ACTOR_OBJ_BOMBIWA, ACTOR_EN_GOROIWA };
    static const s32 sCategories[] = { ACTORCAT_BG, ACTORCAT_PROP };

    for (u32 category = 0; category < ARRAY_COUNT(sCategories); category++) {
        Actor* actor = play->actorCtx.actorLists[sCategories[category]].head;

        while (actor != NULL) {
            Actor* next = actor->next;
            f32 dx = actor->world.pos.x - center->x;
            f32 dz = actor->world.pos.z - center->z;
            bool isInRange = (SQ(dx) + SQ(dz)) <= SQ(POWER_KEG_OBSTACLE_RADIUS);

            if (actor->update != NULL && isInRange) {
                if (actor->id == ACTOR_BG_HEAVY_BLOCK) {
                    s32 type = actor->params & 0xFF;

                    if (type != HEAVY_BLOCK_BIG_PIECE && type != HEAVY_BLOCK_SMALL_PIECE) {
                        PlaySfxAt(NA_SE_EV_WALL_BROKEN, &actor->world.pos);
                        Actor_Kill(actor);
                    }
                } else {
                    for (u32 i = 0; i < ARRAY_COUNT(sBreakableIds); i++) {
                        if (actor->id == sBreakableIds[i]) {
                            PlaySfxAt(NA_SE_EV_WALL_BROKEN, &actor->world.pos);
                            Actor_Kill(actor);
                            break;
                        }
                    }
                }
            }
            actor = next;
        }
    }
}

// Vanilla builds the explosion sphere from the bomb's draw, which never runs when it is born already
// exploding, so its centre is placed here.
static void BurstAt(PlayState* play, Vec3f* pos) {
    EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, pos->x, pos->y, pos->z, 0, 0, 0, BOMB_BODY);

    if (bomb == NULL) {
        return;
    }
    bomb->timer = 1;
    Actor_SetScale(&bomb->actor, POWER_KEG_BURST_SCALE);
    bomb->explosionCollider.elements[0].dim.worldSphere.center.x = (s16)pos->x;
    bomb->explosionCollider.elements[0].dim.worldSphere.center.y = (s16)pos->y;
    bomb->explosionCollider.elements[0].dim.worldSphere.center.z = (s16)pos->z;
}

// The keg's own En_Bom already blew where it stood; a ring of further blasts is what widens its reach to a
// keg's, with real bomb damage rather than a radius check of our own.
static void Explode(PlayState* play, Vec3f* center) {
    static const f32 sRingCos[POWER_KEG_SATELLITES] = { 1.0f, 0.0f, -1.0f, 0.0f };
    static const f32 sRingSin[POWER_KEG_SATELLITES] = { 0.0f, 1.0f, 0.0f, -1.0f };
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    EffectSsBomb2_SpawnLayered(play, center, &zero, &zero, POWER_KEG_BLAST_VFX_SCALE, POWER_KEG_BLAST_VFX_STEP);
    EffectSsBlast_SpawnWhiteShockwave(play, center, &zero, &zero);
    PlaySfxAt(NA_SE_IT_BOMB_EXPLOSION, center);

    for (u32 i = 0; i < POWER_KEG_SATELLITES; i++) {
        Vec3f satellite = { center->x + sRingCos[i] * POWER_KEG_BLAST_RING, center->y,
                            center->z + sRingSin[i] * POWER_KEG_BLAST_RING };

        BurstAt(play, &satellite);
    }
    ShatterObstacles(play, center);
}

// The keg is a real bomb, so vanilla ticks its fuse and blows it up wherever it ended: the extra blast lands
// at its last position, not Link's. That position is copied every frame because a killed actor is freed
// before this is asked where it was.
static void WatchKeg(PlayState* play) {
    if (sKeg == NULL) {
        return;
    }
    if (sKeg->update != NULL && sKeg->params == BOMB_BODY) {
        sKegPos = sKeg->world.pos;
        return;
    }
    Explode(play, &sKegPos);
    sKeg = NULL;
}

// Player_UpperAction_CarryActor and the carry-wait animation, which vanilla reaches through the bomb's item
// action; the keg has its own, so it asks for the carry pose the way lifting a pot does.
void func_80835688(Player* player, PlayState* play);

// Mirrors the vanilla bomb pull: an En_Bom spawned as Link's child and handed to his carry state, so he walks
// with it, throws it and sets it down exactly as he would a bomb.
static void PullPowerKeg(PlayState* play, Player* player) {
    Actor* keg = Actor_SpawnAsChild(&play->actorCtx, &player->actor, play, ACTOR_EN_BOM, player->actor.world.pos.x,
                                    player->actor.world.pos.y, player->actor.world.pos.z, 0, player->actor.shape.rot.y,
                                    0, BOMB_BODY);

    if (keg == NULL) {
        return;
    }
    sKegCount--;
    StoreKegCount();
    ((EnBom*)keg)->timer = POWER_KEG_FUSE;
    player->interactRangeActor = keg;
    player->heldActor = keg;
    player->getItemId = GI_NONE;
    player->getItemEntry = (GetItemEntry)GET_ITEM_NONE;
    player->unk_3BC.y = keg->shape.rot.y - player->actor.shape.rot.y;
    player->stateFlags1 |= PLAYER_STATE1_CARRYING_ACTOR;
    func_80835688(player, play);
    sKeg = keg;
    sKegPos = keg->world.pos;
    PlaySfxAt(NA_SE_PL_PULL_UP_BIGROCK, &player->actor.world.pos);
}

// The barrel is opaque and the Goron skull and its fuse translucent, the way MM's own get-item draw splits
// them. The transform is rebuilt from the actor's position so the bomb's collision scale cannot shrink it.
static void DrawKeg(Actor* actor, PlayState* play, bool* drawVanilla) {
    if (actor != sKeg || actor->params != BOMB_BODY || !ResourceMgr_FileExists(sKegBarrelDL)) {
        return;
    }
    *drawVanilla = false;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_SetTranslateRotateYXZ(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, &actor->shape.rot);
    Matrix_Scale(POWER_KEG_MODEL_SCALE, POWER_KEG_MODEL_SCALE, POWER_KEG_MODEL_SCALE, MTXMODE_APPLY);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sKegBarrelDL);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sKegSkullDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

#define POWER_KEG_GIVE_SCALE 0.4f

static void DrawKegGetItem(PlayState* play, GetItemEntry* entry) {
    if (!ResourceMgr_FileExists(sKegBarrelDL)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(POWER_KEG_GIVE_SCALE, POWER_KEG_GIVE_SCALE, POWER_KEG_GIVE_SCALE, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sKegBarrelDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sKegSkullDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// --- Pictograph Box: photo format (verbatim from MM's z_play.c and z64save.h) ---

#define PICTO_PHOTO_WIDTH 160
#define PICTO_PHOTO_HEIGHT 112
#define PICTO_PHOTO_TOPLEFT_X ((SCREEN_WIDTH - PICTO_PHOTO_WIDTH) / 2)
#define PICTO_PHOTO_TOPLEFT_Y ((SCREEN_HEIGHT - PICTO_PHOTO_HEIGHT) / 2)
#define PICTO_PHOTO_SIZE (PICTO_PHOTO_WIDTH * PICTO_PHOTO_HEIGHT)
#define PICTO_PHOTO_COMPRESSED_SIZE (PICTO_PHOTO_SIZE * 5 / 8)
#define PICTO_PHOTO_STRIP 8

#define PLAY_INTENSITY_RED 2
#define PLAY_INTENSITY_GREEN 4
#define PLAY_INTENSITY_BLUE 1
#define PLAY_INTENSITY_NORM (0x1F * PLAY_INTENSITY_RED + 0x1F * PLAY_INTENSITY_GREEN + 0x1F * PLAY_INTENSITY_BLUE)
#define PLAY_INTENSITY_MIX(r, g, b, m) \
    ((((r) * PLAY_INTENSITY_RED + (g) * PLAY_INTENSITY_GREEN + (b) * PLAY_INTENSITY_BLUE) * (m)) / PLAY_INTENSITY_NORM)
#define PICTO_RGBA16_GET_R(pixel) (((pixel) >> 11) & 0x1F)
#define PICTO_RGBA16_GET_G(pixel) (((pixel) >> 6) & 0x1F)
#define PICTO_RGBA16_GET_B(pixel) (((pixel) >> 1) & 0x1F)

#define PLAY_COMPRESS_BITS 5
#define PLAY_DECOMPRESS_BITS 8

static u16 sFrameRgba16[SCREEN_WIDTH * SCREEN_HEIGHT];
static u8 sPhotoI8[PICTO_PHOTO_SIZE];
static u8 sPhotoI5[PICTO_PHOTO_COMPRESSED_SIZE];

static void ConvertRgba16ToIntensity(u8* dest, const u16* src, s32 left, s32 top, s32 right, s32 bottom) {
    for (s32 y = top; y <= bottom; y++) {
        for (s32 x = left; x <= right; x++) {
            u32 pixel = src[y * SCREEN_WIDTH + x];
            u32 r = PICTO_RGBA16_GET_R(pixel);
            u32 g = PICTO_RGBA16_GET_G(pixel);
            u32 b = PICTO_RGBA16_GET_B(pixel);

            *dest++ = PLAY_INTENSITY_MIX(r, g, b, 255);
        }
    }
}

// Five 5-bit pixels packed into every eight bits, exactly as MM stores a pictograph.
static void CompressI8ToI5(const u8* src, u8* dest, size_t size) {
    s32 bitsLeft = PLAY_DECOMPRESS_BITS;
    u32 destPixel = 0;

    for (size_t i = 0; i < size; i++) {
        u32 srcPixel = (*src++ * 0x1F + 0x80) / 0xFF;
        s32 shift = bitsLeft - PLAY_COMPRESS_BITS;

        if (shift > 0) {
            destPixel |= srcPixel << shift;
        } else {
            destPixel |= srcPixel >> -shift;
            *dest++ = (u8)destPixel;
            shift += PLAY_DECOMPRESS_BITS;
            destPixel = srcPixel << shift;
        }
        bitsLeft = shift;
    }
    if (bitsLeft < PLAY_DECOMPRESS_BITS) {
        *dest = (u8)destPixel;
    }
}

// The save holds I5 only, so showing a stored pictograph means unpacking it back to 8bpp first.
static void DecompressI5ToI8(const u8* src, u8* dest, size_t size) {
    s32 bitsLeft = PLAY_DECOMPRESS_BITS;
    u32 srcPixel = *src++;

    for (size_t i = 0; i < size; i++) {
        s32 shift = bitsLeft - PLAY_COMPRESS_BITS;
        u32 destPixel;

        if (shift > 0) {
            destPixel = srcPixel >> shift;
        } else {
            destPixel = srcPixel << -shift;
            srcPixel = *src++;
            shift += PLAY_DECOMPRESS_BITS;
            destPixel |= srcPixel >> shift;
        }
        *dest++ = (u8)((destPixel & 0x1F) * 0xFF / 0x1F);
        bitsLeft = shift;
    }
}

// --- Pictograph Box: subject validation (ported from MM's z_snap.c) ---

typedef enum {
    PICTO_VALID_0,
    PICTO_VALID_IN_SWAMP,
    PICTO_VALID_MONKEY,
    PICTO_VALID_BIG_OCTO,
    PICTO_VALID_LULU_HEAD,
    PICTO_VALID_LULU_RIGHT_ARM,
    PICTO_VALID_LULU_LEFT_ARM,
    PICTO_VALID_SCARECROW,
    PICTO_VALID_TINGLE,
    PICTO_VALID_PIRATE_GOOD,
    PICTO_VALID_DEKU_KING,
    PICTO_VALID_PIRATE_TOO_FAR,

    PICTO_VALID_BEHIND_COLLISION = 0x3B,
    PICTO_VALID_BEHIND_BG,
    PICTO_VALID_NOT_IN_VIEW,
    PICTO_VALID_BAD_ANGLE,
    PICTO_VALID_BAD_DISTANCE
} PictoValidFlag;

#define PICTO_VALID_WIDTH 150
#define PICTO_VALID_HEIGHT 105
#define PICTO_VALID_TOPLEFT_X ((SCREEN_WIDTH - PICTO_VALID_WIDTH) / 2)
#define PICTO_VALID_TOPLEFT_Y ((SCREEN_HEIGHT - PICTO_VALID_HEIGHT) / 2)
#define PICTO_PARAMS_ANY ((s16)-1)

typedef struct {
    s16 actorId;
    s16 params;
    u8 flag;
    f32 distMin;
    f32 distMax;
    s16 angleRange;
} PictoSubject;

// MM keys validation off a per-actor callback, which OoT actors have no field for, so the subjects are a
// table of ids instead. Each OoT actor stands in for the Terminan one MM would have photographed.
static const PictoSubject sPictoSubjects[] = {
    { ACTOR_EN_SKJ, PICTO_PARAMS_ANY, PICTO_VALID_MONKEY, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_BIGOKUTA, PICTO_PARAMS_ANY, PICTO_VALID_BIG_OCTO, 10.0f, 800.0f, -1 },
    { ACTOR_EN_KO, PICTO_PARAMS_ANY, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_MD, PICTO_PARAMS_ANY, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_SA, PICTO_PARAMS_ANY, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_OSSAN, 0, PICTO_VALID_TINGLE, 10.0f, 400.0f, 0x4000 },
    { ACTOR_EN_OWL, PICTO_PARAMS_ANY, PICTO_VALID_DEKU_KING, 120.0f, 480.0f, 0x38E3 },
    { ACTOR_OBJ_DEKUJR, PICTO_PARAMS_ANY, PICTO_VALID_DEKU_KING, 120.0f, 480.0f, 0x38E3 },
    { ACTOR_EN_RU1, PICTO_PARAMS_ANY, PICTO_VALID_LULU_HEAD, 10.0f, 300.0f, -1 },
    { ACTOR_EN_RU1, PICTO_PARAMS_ANY, PICTO_VALID_LULU_RIGHT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_RU1, PICTO_PARAMS_ANY, PICTO_VALID_LULU_LEFT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_RU2, PICTO_PARAMS_ANY, PICTO_VALID_LULU_HEAD, 10.0f, 300.0f, -1 },
    { ACTOR_EN_RU2, PICTO_PARAMS_ANY, PICTO_VALID_LULU_RIGHT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_RU2, PICTO_PARAMS_ANY, PICTO_VALID_LULU_LEFT_ARM, 50.0f, 160.0f, 0x3000 },
    { ACTOR_EN_KAKASI, PICTO_PARAMS_ANY, PICTO_VALID_SCARECROW, 280.0f, 1800.0f, -1 },
    { ACTOR_EN_KAKASI2, PICTO_PARAMS_ANY, PICTO_VALID_SCARECROW, 280.0f, 1800.0f, -1 },
    { ACTOR_EN_KAKASI3, PICTO_PARAMS_ANY, PICTO_VALID_SCARECROW, 280.0f, 1800.0f, -1 },
    { ACTOR_EN_GE1, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GE1, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
    { ACTOR_EN_GELDB, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GELDB, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
    { ACTOR_EN_GE2, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GE2, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
    { ACTOR_EN_GE3, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_GOOD, 10.0f, 400.0f, -1 },
    { ACTOR_EN_GE3, PICTO_PARAMS_ANY, PICTO_VALID_PIRATE_TOO_FAR, 10.0f, 1200.0f, -1 },
};

static u32 sPictoFlags[2];
static u32 sPictoSavedFlags[2];

static void StorePictoFlags(void) {
    sApi->StorageSet(MM_ITEMS_MOD, PICTO_FLAGS_FIELD, sPictoFlags, sizeof(sPictoFlags));
}

static void SetPictoFlag(s32 flag) {
    sPictoFlags[(flag < 0x20) ? 0 : 1] |= 1u << (flag & 0x1F);
}

static s32 ValidatePictograph(PlayState* play, Actor* actor, const PictoSubject* subject) {
    Camera* camera = GET_ACTIVE_CAM(play);
    Vec3f* pos = &actor->focus.pos;
    Vec3s* rot = &actor->shape.rot;
    Vec3f projectedPos;
    CollisionPoly* poly;
    Actor* occluders[2];
    s32 bgId;
    s16 screenX;
    s16 screenY;
    s32 failures = 0;
    f32 distance = Math_Vec3f_DistXYZ(pos, &camera->eye);
    s16 pitch = ABS((s16)(Camera_GetCamDirPitch(camera) + rot->x));
    s16 yaw = ABS((s16)(Camera_GetCamDirYaw(camera) - BINANG_SUB(rot->y, 0x7FFF)));

    if ((distance < subject->distMin) || (subject->distMax < distance)) {
        SetPictoFlag(PICTO_VALID_BAD_DISTANCE);
        failures |= PICTO_VALID_BAD_DISTANCE;
    }
    if ((subject->angleRange > 0) && ((subject->angleRange < pitch) || (subject->angleRange < yaw))) {
        SetPictoFlag(PICTO_VALID_BAD_ANGLE);
        failures |= PICTO_VALID_BAD_ANGLE;
    }

    Actor_GetScreenPos(play, actor, &screenX, &screenY);
    screenX -= PICTO_VALID_TOPLEFT_X;
    screenY -= PICTO_VALID_TOPLEFT_Y;
    if ((screenX < 0) || (screenX > PICTO_VALID_WIDTH) || (screenY < 0) || (screenY > PICTO_VALID_HEIGHT)) {
        SetPictoFlag(PICTO_VALID_NOT_IN_VIEW);
        failures |= PICTO_VALID_NOT_IN_VIEW;
    }
    if (BgCheck_ProjectileLineTest(&play->colCtx, pos, &camera->eye, &projectedPos, &poly, true, true, true, true,
                                   &bgId)) {
        SetPictoFlag(PICTO_VALID_BEHIND_BG);
        failures |= PICTO_VALID_BEHIND_BG;
    }

    occluders[0] = actor;
    occluders[1] = &GET_PLAYER(play)->actor;
    if (CollisionCheck_LineOCCheck(play, &play->colChkCtx, pos, &camera->eye, occluders, 2)) {
        SetPictoFlag(PICTO_VALID_BEHIND_COLLISION);
        failures |= PICTO_VALID_BEHIND_COLLISION;
    }

    if (failures == 0) {
        SetPictoFlag(subject->flag);
    }
    return failures;
}

static void RecordPictographedActors(PlayState* play) {
    sPictoFlags[0] = 0;
    sPictoFlags[1] = 0;

    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            for (u32 i = 0; i < ARRAY_COUNT(sPictoSubjects); i++) {
                const PictoSubject* subject = &sPictoSubjects[i];

                if (subject->actorId != actor->id) {
                    continue;
                }
                if ((subject->params != PICTO_PARAMS_ANY) && (subject->params != actor->params)) {
                    continue;
                }
                ValidatePictograph(play, actor, subject);
            }
        }
    }
    StorePictoFlags();
}

// --- Pictograph Box: capture and prompt ---

#define PICTO_TIME_PRIORITY 900
#define PICTO_ARM_FRAMES 2
#define PICTO_HUD_HIDE 1
#define PICTO_HUD_SHOW 50
#define PICTO_VIEWFINDER_LEFT 80
#define PICTO_VIEWFINDER_TOP 60
#define PICTO_VIEWFINDER_RIGHT 220
#define PICTO_VIEWFINDER_BOTTOM 160
#define PICTO_VIEWFINDER_CORNER 16
#define PICTO_CROSSHAIR_X 158
#define PICTO_CROSSHAIR_Y 116
#define PICTO_CROSSHAIR_ARM 5
#define PICTO_PANEL_LEFT 70
#define PICTO_PANEL_TOP 22
#define PICTO_PANEL_RIGHT 251
#define PICTO_PANEL_BOTTOM 151
// MM lifts the print to leave the message box its room along the bottom.
#define PICTO_PHOTO_RAISE 33

typedef enum { PICTO_OFF, PICTO_LENS, PICTO_PHOTO } PictoState;
typedef enum { PICTO_CAPTURE_IDLE, PICTO_CAPTURE_EMIT, PICTO_CAPTURE_PROCESS } PictoCaptureState;

static PictoState sPictoState;
static PictoCaptureState sPictoCapture;
static bool sIsLensPending;
static bool sIsShutterArmed;
static u8 sLensFrames;
static bool sIsPhotoStored;
// A freshly shot picture is the only one that writes the save when it is kept; the stored one is only being
// looked at again.
static bool sIsPhotoFresh;
static bool sIsPromptShown;

static void LoadPictoSave(void) {
    sIsPhotoStored = sApi->StorageGet(MM_ITEMS_MOD, PICTO_PHOTO_FIELD, sPhotoI5, sizeof(sPhotoI5)) == sizeof(sPhotoI5);
    sPictoFlags[0] = 0;
    sPictoFlags[1] = 0;
    sApi->StorageGet(MM_ITEMS_MOD, PICTO_FLAGS_FIELD, sPictoFlags, sizeof(sPictoFlags));
}

static void ClearStoredPhoto(void) {
    sApi->StorageRemove(MM_ITEMS_MOD, PICTO_PHOTO_FIELD);
    sIsPhotoStored = false;
}

// The box owns A and B: A is the shutter and B puts it away, so neither may reach Link underneath. The stick
// is only taken away once the picture is on screen — while the lens is up it is what aims the shot.
static void HoldPictoInput(bool blockStick) {
    sApi->BlockPlayerInput(MM_ITEMS_MOD, BTN_A | BTN_B, blockStick);
}

static void ReleasePictoInput(void) {
    sApi->ReleasePlayerInput(MM_ITEMS_MOD);
    sApi->ReleaseTimeControl(MM_ITEMS_MOD);
}

static void EnterLens(void) {
    sPictoState = PICTO_LENS;
    sIsShutterArmed = false;
    sLensFrames = 0;
    sIsLensPending = true;
    HoldPictoInput(false);
    Interface_ChangeAlpha(PICTO_HUD_HIDE);
}

static void LeaveBox(Player* player, PlayState* play) {
    if (sPictoState == PICTO_LENS) {
        Z64Aiming_Release(player, play);
    }
    sPictoState = PICTO_OFF;
    sPictoCapture = PICTO_CAPTURE_IDLE;
    sIsLensPending = false;
    sIsPromptShown = false;
    ReleasePictoInput();
    Interface_ChangeAlpha(PICTO_HUD_SHOW);
}

// The world stops at the shutter, MM's own halt, so the print and the world behind the prompt agree. The
// subjects are judged now, while their positions are still the ones in the frame.
static void PressShutter(PlayState* play) {
    sPictoSavedFlags[0] = sPictoFlags[0];
    sPictoSavedFlags[1] = sPictoFlags[1];
    RecordPictographedActors(play);
    sApi->RequestTimeControl(MM_ITEMS_MOD, PICTO_TIME_PRIORITY, 0.0f, true);
    sPictoCapture = PICTO_CAPTURE_EMIT;
    Sfx_PlaySfxCentered(NA_SE_SY_CAMERA_ZOOM_DOWN);
}

// The readback is deferred to when the frame's list is processed, so it is emitted here, after the world is
// drawn and before the interface, and read in the next update.
static void EmitCapture(void) {
    PlayState* play = gPlayState;

    if (sPictoCapture != PICTO_CAPTURE_EMIT || play == NULL) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    FB_WriteFramebufferSliceToCPU(&OVERLAY_DISP, sFrameRgba16, 0);
    CLOSE_DISPS(play->state.gfxCtx);
    sPictoCapture = PICTO_CAPTURE_PROCESS;
}

static void ShowPhotoPrompt(void) {
    sPictoState = PICTO_PHOTO;
    sIsPromptShown = false;
    Interface_ChangeAlpha(PICTO_HUD_HIDE);
    sApi->RequestTimeControl(MM_ITEMS_MOD, PICTO_TIME_PRIORITY, 0.0f, true);
    HoldPictoInput(true);
}

static void ShowStoredPhoto(void) {
    DecompressI5ToI8(sPhotoI5, sPhotoI8, PICTO_PHOTO_SIZE);
    sIsPhotoFresh = false;
    ShowPhotoPrompt();
}

static void DevelopCapture(Player* player, PlayState* play) {
    ConvertRgba16ToIntensity(sPhotoI8, sFrameRgba16, PICTO_PHOTO_TOPLEFT_X, PICTO_PHOTO_TOPLEFT_Y,
                             PICTO_PHOTO_TOPLEFT_X + PICTO_PHOTO_WIDTH - 1,
                             PICTO_PHOTO_TOPLEFT_Y + PICTO_PHOTO_HEIGHT - 1);
    sPictoCapture = PICTO_CAPTURE_IDLE;
    sIsPhotoFresh = true;
    Z64Aiming_Release(player, play);
    ShowPhotoPrompt();
}

static void KeepPhoto(Player* player, PlayState* play) {
    if (sIsPhotoFresh) {
        CompressI8ToI5(sPhotoI8, sPhotoI5, PICTO_PHOTO_SIZE);
        sApi->StorageSet(MM_ITEMS_MOD, PICTO_PHOTO_FIELD, sPhotoI5, sizeof(sPhotoI5));
        sIsPhotoStored = true;
    }
    LeaveBox(player, play);
    Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
}

// Saying no throws the picture away and drops you back into the lens: that is how MM lets you retake one,
// and why the box never asks whether to replace what it holds. A fresh shot also rolls its subjects back.
static void DiscardPhoto(void) {
    if (sIsPhotoFresh) {
        sPictoFlags[0] = sPictoSavedFlags[0];
        sPictoFlags[1] = sPictoSavedFlags[1];
        StorePictoFlags();
    }
    ClearStoredPhoto();
    sApi->ReleaseTimeControl(MM_ITEMS_MOD);
    EnterLens();
    Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
}

static void UpdatePhotoPrompt(Player* player, PlayState* play) {
    if (!sIsPromptShown) {
        sIsPromptShown = sApi->ShowTextbox(play, "Keep this pictograph?\x1B%g&&Yes&No%w", false);
        return;
    }
    if (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE || !Message_ShouldAdvance(play)) {
        return;
    }
    Message_CloseTextbox(play);
    if (play->msgCtx.choiceIndex == 0) {
        KeepPhoto(player, play);
        return;
    }
    DiscardPhoto();
}

static void UpdateLens(Player* player, PlayState* play) {
    u16 pressed = play->state.input[0].press.button;
    u16 held = play->state.input[0].cur.button;
    u16 shutterMask = (u16)(BTN_A | GetEquippedButtons(PICTO_BOX_KEY));

    Z64Aiming_Update(player, play);
    Player_ZeroSpeedXZ(player);
    if (sLensFrames < PICTO_ARM_FRAMES) {
        sLensFrames++;
    }
    // The press that raised the lens can still be down when this first ticks, and the item's own action runs
    // a step behind, so the shutter waits for every one of its buttons to come back up — and for the
    // first-person view it is supposed to photograph.
    if (!sIsShutterArmed) {
        sIsShutterArmed = Z64Aiming_IsAiming() && (held & shutterMask) == 0 && sLensFrames >= PICTO_ARM_FRAMES;
        return;
    }
    if (pressed & shutterMask) {
        PressShutter(play);
        return;
    }
    if (pressed & BTN_B) {
        LeaveBox(player, play);
    }
}

static bool CanUsePictoBox(Player* player, PlayState* play) {
    if (player->meleeWeaponState != 0) {
        return false;
    }
    return (player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_CUTSCENE |
                                   PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) == 0;
}

// With a picture already inside, the box shows you THAT one first, the way MM's pictograph button does.
static void TakeOutPictoBox(PlayState* play, Player* player) {
    if (sPictoState != PICTO_OFF) {
        return;
    }
    if (sIsPhotoStored) {
        ShowStoredPhoto();
        return;
    }
    EnterLens();
}

// The lens holds the first-person state and the rotations the engine walks back to zero every frame they go
// unclaimed, so it belongs here, inside the player's update. The photo prompt does not: see RunPhotoPrompt.
static int32_t UpdatePictoBox(Player* player, PlayState* play) {
    if (sPictoState != PICTO_LENS) {
        return sPictoState == PICTO_PHOTO;
    }
    if (sPictoCapture == PICTO_CAPTURE_PROCESS) {
        DevelopCapture(player, play);
        return 1;
    }
    if (sPictoCapture == PICTO_CAPTURE_IDLE) {
        UpdateLens(player, play);
    }
    return 1;
}

// The prompt's own textbox keeps the item's upper action from running, and the host reads an upper action
// that stopped as an item that left the hand, so the prompt runs from the end of the player's update and
// says the box is still out.
static void RunPhotoPrompt(Player* player, PlayState* play) {
    if (sPictoState != PICTO_PHOTO) {
        return;
    }
    Z64Items_KeepHeld(sApi);
    UpdatePhotoPrompt(player, play);
}

static void StowPictoBox(Player* player, PlayState* play) {
    if (sPictoState == PICTO_OFF) {
        return;
    }
    LeaveBox(player, play);
}

// The aim claims the camera and the first-person state, both of which the rest of the player's update would
// undo, so it is requested from the end of that update rather than from the item's own action. It also waits
// out the take-out animation, which would otherwise overwrite the arms the moment the lens comes up.
static void StartPendingAim(Player* player, PlayState* play) {
    if (!sIsLensPending || sPictoState != PICTO_LENS) {
        return;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) || player->heldItemAction != PLAYER_IA_CUSTOM) {
        return;
    }
    sIsLensPending = false;
    Z64Aiming_Request(player, play);
}

#define PICTO_GIVE_SCALE 0.5f

static void DrawPictoGetItem(PlayState* play, GetItemEntry* entry) {
    if (!ResourceMgr_FileExists(sPictoBodyDL)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Scale(PICTO_GIVE_SCALE, PICTO_GIVE_SCALE, PICTO_GIVE_SCALE, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sPictoBodyDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sPictoFrameDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// --- Pictograph Box: drawing ---

// MM's own frame: four corner brackets on the R_PICTO_FOCUS_BORDER positions and a crosshair at its icon.
// The message box draws after this, so the cycle type goes back before leaving.
static void DrawViewfinder(PlayState* play) {
    const s32 left = PICTO_VIEWFINDER_LEFT;
    const s32 top = PICTO_VIEWFINDER_TOP;
    const s32 right = PICTO_VIEWFINDER_RIGHT + PICTO_VIEWFINDER_CORNER;
    const s32 bottom = PICTO_VIEWFINDER_BOTTOM + PICTO_VIEWFINDER_CORNER;
    const s32 arm = PICTO_VIEWFINDER_CORNER;
    const s32 crossX = PICTO_CROSSHAIR_X;
    const s32 crossY = PICTO_CROSSHAIR_Y;
    const s32 reach = PICTO_CROSSHAIR_ARM;
    const s32 strokes[][4] = {
        { left, top, left + arm, top + 2 },
        { left, top, left + 2, top + arm },
        { right - arm, top, right, top + 2 },
        { right - 2, top, right, top + arm },
        { left, bottom - 2, left + arm, bottom },
        { left, bottom - arm, left + 2, bottom },
        { right - arm, bottom - 2, right, bottom },
        { right - 2, bottom - arm, right, bottom },
        { crossX - reach, crossY, crossX + reach, crossY + 1 },
        { crossX, crossY - reach, crossX + 1, crossY + reach },
    };

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_FILL);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(OVERLAY_DISP++, 0xFFFFFFFF);
    for (u32 i = 0; i < ARRAY_COUNT(strokes); i++) {
        gDPFillRectangle(OVERLAY_DISP++, strokes[i][0], strokes[i][1], strokes[i][2], strokes[i][3]);
    }
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The print on its grey mount, MM's own layout. Sepia is not a filter: the I8 image runs through
// MODULATEI_PRIM and the prim colour is the tone.
static void DrawPhoto(PlayState* play) {
    s32 top = PICTO_PHOTO_TOPLEFT_Y - PICTO_PHOTO_RAISE;

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 200, 200, 250);
    gDPFillRectangle(OVERLAY_DISP++, PICTO_PANEL_LEFT, PICTO_PANEL_TOP, PICTO_PANEL_RIGHT, PICTO_PANEL_BOTTOM);

    gDPPipeSync(OVERLAY_DISP++);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_POINT);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 250, 160, 160, 255);

    for (s32 y = 0; y < PICTO_PHOTO_HEIGHT; y += PICTO_PHOTO_STRIP, top += PICTO_PHOTO_STRIP) {
        // The buffer never moves, so the renderer's texture cache would keep serving the previous photo
        // unless each strip is invalidated after a new capture overwrites it.
        gSPInvalidateTexCache(OVERLAY_DISP++, (uintptr_t)&sPhotoI8[y * PICTO_PHOTO_WIDTH]);
        gDPLoadTextureBlock(OVERLAY_DISP++, &sPhotoI8[y * PICTO_PHOTO_WIDTH], G_IM_FMT_I, G_IM_SIZ_8b,
                            PICTO_PHOTO_WIDTH, PICTO_PHOTO_STRIP, 0, G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(OVERLAY_DISP++, PICTO_PHOTO_TOPLEFT_X << 2, top << 2,
                            (PICTO_PHOTO_TOPLEFT_X + PICTO_PHOTO_WIDTH) << 2, (top + PICTO_PHOTO_STRIP) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    gDPPipeSync(OVERLAY_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawPictoOverlay(PlayState* play) {
    if (sPictoState == PICTO_LENS && sPictoCapture == PICTO_CAPTURE_IDLE) {
        DrawViewfinder(play);
        return;
    }
    if (sPictoState == PICTO_PHOTO) {
        DrawPhoto(play);
    }
}

// --- Mod frame ---

static void RunMmItems(void) {
    PlayState* play = gPlayState;
    Player* player;

    if (play == NULL) {
        return;
    }
    player = GET_PLAYER(play);
    WatchKeg(play);
    StartPendingAim(player, play);
    RunPhotoPrompt(player, play);
}

static void ForgetSceneState(int16_t sceneNum) {
    sKeg = NULL;
    Z64Aiming_Drop();
    sPictoState = PICTO_OFF;
    sPictoCapture = PICTO_CAPTURE_IDLE;
    sIsLensPending = false;
    sIsPromptShown = false;
    ReleasePictoInput();
}

static void ReloadSave(int32_t fileNum) {
    LoadKegCount();
    LoadPictoSave();
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi, ShowTextbox) || !SOH_MOD_API_HAS(sApi, StorageSet)) {
        return;
    }

    SOHCustomItemDefinition keg = Z64Items_Define(POWER_KEG_KEY, sKegIconTex, sKegNameTex);
    SOHCustomItemDefinition picto = Z64Items_Define(PICTO_BOX_KEY, sPictoIconTex, sPictoNameTex);

    Z64Items_SetButtons(&keg, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&keg, MM_ITEMS_PAGE, POWER_KEG_SLOT, 0);
    Z64Items_SetTextbox(&keg, "You got a %rPowder Keg%w!&The blasting barrel the Gorons&trust to move a mountain.^"
                              "Press %y\xA1%w to set one down, then get&clear: the fuse is short.^It shatters every "
                              "boulder and heavy&stone block around it, but you need&the %rSilver Gauntlets%w to lift "
                              "one.");
    Z64Items_SetPauseText(&keg, "%rPowder Keg&%wPress %y\xA1%w to set one down and run.&Needs the Silver Gauntlets.");
    Z64Items_SetCanUse(&keg, CanUsePowerKeg);
    Z64Items_SetAmmo(&keg, GetKegCount);
    Z64Items_SetAction(&keg, PullPowerKeg, NULL);
    keg.flags |= SOH_CUSTOM_ITEM_INSTANT;
    keg.getItemEntry.drawFunc = DrawKegGetItem;
    keg.onReceive = RestockKegs;

    Z64Items_SetButtons(&picto, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&picto, MM_ITEMS_PAGE, PICTO_BOX_SLOT, 0);
    Z64Items_SetTextbox(&picto, "You got the %rPictograph Box%w!&A curiosity out of Termina that&keeps a moment "
                                "behind glass.^Press %y\xA1%w to raise the lens and&%y\x9F%w to take the picture.^"
                                "Only one pictograph fits inside, so&choose what you keep.");
    Z64Items_SetPauseText(&picto, "%rPictograph Box&%wPress %y\xA1%w to raise the lens,&%y\x9F%w to shoot, %y\xA0%w to "
                                  "put it away.");
    Z64Items_SetCanUse(&picto, CanUsePictoBox);
    Z64Items_SetAction(&picto, TakeOutPictoBox, UpdatePictoBox);
    Z64Items_SetHeldCallbacks(&picto, NULL, StowPictoBox, NULL);
    picto.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    picto.getItemEntry.drawFunc = DrawPictoGetItem;

    if (!Z64Items_Register(sApi, &keg) || !Z64Items_Register(sApi, &picto)) {
        return;
    }
    LoadKegCount();
    LoadPictoSave();

    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, RunMmItems);
    SOH_REGISTER_HOOK(sApi, OnItemReceive, RestockFromBombRefill);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, ReloadSave);
    SOH_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawPictoOverlay);
    SOH_REGISTER_HOOK(sApi, OnPlayDrawEnd, EmitCapture);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_EN_BOM, DrawKeg);
}
