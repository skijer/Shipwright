#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "overlays/actors/ovl_Door_Ana/z_door_ana.h"
#include "overlays/actors/ovl_En_Tk/z_en_tk.h"
#include "overlays/actors/ovl_Obj_Makekinsuta/z_obj_makekinsuta.h"

#define SHOVEL_KEY "nei.shovel"
#define SHOVEL_BEAN_RADIUS 100.0f
#define SHOVEL_GROTTO_RADIUS 120.0f
#define SHOVEL_ITEM_DROP_CHANCE 15.0f
#define SHOVEL_FAIRY_CHANCE 1.0f
#define SHOVEL_DIG_FRAME 25
#define SHOVEL_GIVE_SCALE 0.2f

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconShovelTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gShovelNameTex";
static const ALIGN_ASSET(2) char sDigAnim[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_dampe_dig";
// The one mesh for both hand and pedestal. NEI's held copy is the same geometry exported WITHOUT its
// Blender object transform, so it keeps a 0.16 squash on one axis that the archive copy already bakes in.
static const ALIGN_ASSET(2) char sShovelDL[] = "__OTR__objects/object_nei_shovel/gShovelGiveDL_opaque_dl";

static Color_RGBA8 sDustColor = { 139, 90, 43, 255 };

static const char* const sRequiredHooks[] = { "OnActorDrawEnd", "OnPlayerUpdate", "OnPlayerResolveItemActionInit" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

// Measured in game against the dig pose.
#define SHOVEL_HELD_SCALE 0.11f
#define SHOVEL_HELD_OFFSET_X 5.56f
#define SHOVEL_HELD_OFFSET_Y -1.49f
#define SHOVEL_HELD_OFFSET_Z -10.16f
#define SHOVEL_HELD_ROTATION_X 60.98f
#define SHOVEL_HELD_ROTATION_Y 35.6f
#define SHOVEL_HELD_ROTATION_Z 59.02f

static const SOHModApi* sApi;
static s16 sDigFrame;
static bool sIsDigPending;

void DoorAna_WaitOpen(DoorAna* this, PlayState* play);
s32 Player_UpperAction_ChangeHeldItem(Player* this, PlayState* play);
s32 Player_SetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 flags);
void func_80839FFC(Player* this, PlayState* play);
void Player_ZeroSpeedXZ(Player* this);

static void PlaySfxAt(u16 sfxId, Vec3f* pos) {
    Audio_PlaySoundGeneral(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}

static void SpawnDust(PlayState* play, Vec3f* pos, u8 count) {
    Vec3f accel = { 0.0f, -1.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        Vec3f dustPos = { pos->x + Rand_CenteredFloat(20.0f), pos->y + 3.0f, pos->z + Rand_CenteredFloat(20.0f) };
        Vec3f velocity = { Rand_CenteredFloat(6.0f), 6.0f + Rand_ZeroOne() * 4.0f, Rand_CenteredFloat(6.0f) };
        func_8002836C(play, &dustPos, &velocity, &accel, &sDustColor, &sDustColor, 250, 30, 10);
    }
}

static s16 RollDrop(void) {
    f32 roll = Rand_ZeroOne() * 100.0f;

    // Item_DropCollectible turns ITEM00_FLEXIBLE into a healing fairy; NEI's ITEM00_FAIRY was 0x0A, large arrows.
    if (roll < SHOVEL_FAIRY_CHANCE) {
        return ITEM00_FLEXIBLE;
    }
    if (roll < 50.0f) {
        f32 rupeeRoll = Rand_ZeroOne();
        if (rupeeRoll < 0.70f) {
            return ITEM00_RUPEE_GREEN;
        }
        return rupeeRoll < 0.95f ? ITEM00_RUPEE_BLUE : ITEM00_RUPEE_RED;
    }
    if (roll < 75.0f) {
        f32 recoveryRoll = Rand_ZeroOne();
        if (recoveryRoll < 0.50f) {
            return ITEM00_HEART;
        }
        return recoveryRoll < 0.75f ? ITEM00_MAGIC_SMALL : ITEM00_MAGIC_LARGE;
    }
    return Rand_ZeroOne() < 0.6f ? ITEM00_BOMBS_A : ITEM00_NUTS;
}

static void DropTreasure(PlayState* play, Vec3f* digPos) {
    Vec3f dropPos = { digPos->x, digPos->y + 10.0f, digPos->z };

    if (play->sceneNum == SCENE_GRAVEYARD && !Flags_GetItemGetInf(ITEMGETINF_1C)) {
        Item_DropCollectible(play, &dropPos, ((COLLECTFLAG_GRAVEDIGGING_HEART_PIECE & 0x3F) << 8) | ITEM00_HEART_PIECE);
        Flags_SetItemGetInf(ITEMGETINF_1C);
        PlaySfxAt(NA_SE_SY_CORRECT_CHIME, &dropPos);
        return;
    }
    if (Rand_ZeroOne() * 100.0f > SHOVEL_ITEM_DROP_CHANCE) {
        return;
    }
    Item_DropCollectible(play, &dropPos, RollDrop());
    PlaySfxAt(NA_SE_SY_GET_ITEM, &dropPos);
}

static void ActivateSoftSoil(PlayState* play, ObjMakekinsuta* soil) {
    soil->unk_152 = 1;
    PlaySfxAt(NA_SE_SY_PIECE_OF_HEART, &soil->actor.world.pos);
    SpawnDust(play, &soil->actor.world.pos, 6);
}

// Hidden grottos keep their "needs a bomb" or "needs the Song of Storms" bits in params 0x300.
static void RevealGrotto(PlayState* play, DoorAna* grotto) {
    if ((grotto->actor.params & 0x300) == 0) {
        return;
    }
    if ((grotto->actor.params & 0x200) != 0) {
        Collider_DestroyCylinder(play, &grotto->collider);
    }
    grotto->actor.params &= ~0x300;
    grotto->actionFunc = DoorAna_WaitOpen;
    grotto->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    PlaySfxAt(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos);
    SpawnDust(play, &grotto->actor.world.pos, 12);
}

static void UncoverBuriedActors(PlayState* play, Vec3f* digPos) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; actor != NULL; actor = actor->next) {
        if (actor->update == NULL) {
            continue;
        }
        f32 distance = Math_Vec3f_DistXYZ(digPos, &actor->world.pos);
        if (actor->id == ACTOR_OBJ_MAKEKINSUTA && distance < SHOVEL_BEAN_RADIUS) {
            ActivateSoftSoil(play, (ObjMakekinsuta*)actor);
        } else if (actor->id == ACTOR_DOOR_ANA && distance < SHOVEL_GROTTO_RADIUS) {
            RevealGrotto(play, (DoorAna*)actor);
        }
    }
}

static void Dig(Player* player, PlayState* play) {
    Vec3f digPos = { player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 30.0f,
                     player->actor.floorHeight + 5.0f,
                     player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 30.0f };

    SpawnDust(play, &digPos, 8);
    PlaySfxAt(NA_SE_PL_WALK_SAND, &digPos);
    UncoverBuriedActors(play, &digPos);
    DropTreasure(play, &digPos);
}

// Vanilla hands first person its own action func; digging out of it would fight over who owns the player.
static bool CanDig(Player* player, PlayState* play) {
    return !(player->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_FIRST_PERSON)) &&
           (player->meleeWeaponState == 0) && (player->actor.bgCheckFlags & 1);
}

static void DigAction(Player* player, PlayState* play);

// The engine owns the dig: anything that takes the player over (damage, a cutscene) replaces this action func.
static bool IsDigging(Player* player) {
    return player->actionFunc == DigAction;
}

static void StopDigging(Player* player, PlayState* play) {
    sIsDigPending = false;
    if (!IsDigging(player)) {
        return;
    }
    sDigFrame = 0;
    // Same exit as a vanilla item action: back to idle, or to the targeting stance if one is held.
    func_80839FFC(player, play);
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

// The dig is timed by the animation, so without the resource the shovel never enters the dig.
static void StartDigging(Player* player, PlayState* play) {
    if (IsDigging(player) || !CanDig(player, play)) {
        return;
    }
    LinkAnimationHeader* digAnim = ResourceMgr_LoadPlayerAnimAsHeader(sDigAnim);
    if (digAnim == NULL) {
        return;
    }
    sDigFrame = 0;
    Player_SetupAction(play, player, DigAction, 0);
    Player_ZeroSpeedXZ(player);
    LinkAnimation_PlayOnce(play, &player->skelAnime, digAnim);
    PlaySfxAt(NA_SE_PL_CHANGE_ARMS, &player->actor.world.pos);
}

// Taking the shovel out runs the item-change animation, and that plays an idle animation over the body until it
// ends. Digging from here would be overwritten mid-swing, so the first dig of a scene waits for the change.
static void TakeOutShovel(PlayState* play, Player* player) {
    sIsDigPending = true;
}

static bool IsChangingHeldItem(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_START_CHANGING_HELD_ITEM) ||
           player->upperActionFunc == Player_UpperAction_ChangeHeldItem;
}

static void StartPendingDig(void) {
    if (!sIsDigPending || !SOH_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }
    PlayState* play = gPlayState;
    Player* player = gPlayState == NULL ? NULL : GET_PLAYER(gPlayState);

    if (play == NULL || player == NULL) {
        return;
    }
    if (player->heldItemAction != PLAYER_IA_CUSTOM) {
        sIsDigPending = false;
        return;
    }
    if (IsChangingHeldItem(player)) {
        return;
    }
    sIsDigPending = false;
    StartDigging(player, play);
}

static void DigAction(Player* player, PlayState* play) {
    if (LinkAnimation_Update(play, &player->skelAnime)) {
        StopDigging(player, play);
        return;
    }
    Player_ZeroSpeedXZ(player);
    sDigFrame++;
    if (sDigFrame == SHOVEL_DIG_FRAME) {
        Dig(player, play);
    }
}

// Through s32: 180 degrees is 32768, which a float-to-s16 conversion may not represent.
static s16 DegreesToBinang(f32 degrees) {
    return (s16)(s32)(degrees * (32768.0f / 180.0f));
}

static void ApplyHeldTransform(void) {
    Matrix_Translate(SHOVEL_HELD_OFFSET_X, SHOVEL_HELD_OFFSET_Y, SHOVEL_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_RotateZYX(DegreesToBinang(SHOVEL_HELD_ROTATION_X), DegreesToBinang(SHOVEL_HELD_ROTATION_Y),
                     DegreesToBinang(SHOVEL_HELD_ROTATION_Z), MTXMODE_APPLY);
    Matrix_Scale(SHOVEL_HELD_SCALE, SHOVEL_HELD_SCALE, SHOVEL_HELD_SCALE, MTXMODE_APPLY);
}

// A two-handed tool: it sits between both hands and points from the right hand to the left.
static void DrawShovelInHands(Player* player, PlayState* play) {
    if (!IsDigging(player)) {
        return;
    }
    Vec3f leftHand = player->bodyPartsPos[PLAYER_BODYPART_L_HAND];
    Vec3f rightHand = player->bodyPartsPos[PLAYER_BODYPART_R_HAND];
    f32 spanX = leftHand.x - rightHand.x;
    f32 spanY = leftHand.y - rightHand.y;
    f32 spanZ = leftHand.z - rightHand.z;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate((leftHand.x + rightHand.x) * 0.5f, (leftHand.y + rightHand.y) * 0.5f,
                     (leftHand.z + rightHand.z) * 0.5f, MTXMODE_NEW);
    Matrix_RotateY(Math_FAtan2F(spanX, spanZ), MTXMODE_APPLY);
    Matrix_RotateX(-Math_FAtan2F(spanY, sqrtf(spanX * spanX + spanZ * spanZ)), MTXMODE_APPLY);
    ApplyHeldTransform();
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sShovelDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// Same spot as NEI: after the whole skeleton, outside the per-limb matrices frame interpolation records.
static void DrawAfterPlayer(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;
    if (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) {
        return;
    }
    DrawShovelInHands(player, play);
}

static void DrawGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(SHOVEL_GIVE_SCALE, SHOVEL_GIVE_SCALE, SHOVEL_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sShovelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void UseShovel(Player* player, PlayState* play) {
    StartDigging(player, play);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOHCustomItemDefinition shovel = Z64Items_Define(SHOVEL_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&shovel, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&shovel, 0, 22, 0);
    Z64Items_SetTextbox(&shovel, "You got %rDampé's Shovel%w!&The old gravekeeper's tool,&worn smooth by years of work.^"
                                 "Press %y\xA1%w to dig. %ySoft soil%w wakes&what sleeps beneath it, and a&%yhidden "
                                 "grotto%w opens where the&ground rings hollow.^"
                                 "Loose earth turns up %grupees%w and&%gbombs%w, and once in a while a&%cfairy%w. The "
                                 "%ygraveyard%w keeps a&reward for the first to dig there.");
    Z64Items_SetPauseText(&shovel, "%rDampé's Shovel&%wPress %y\xA1%w to dig. Soft soil, grottos&and graves hide the "
                                   "most.");
    Z64Items_SetCanUse(&shovel, CanDig);
    Z64Items_SetAction(&shovel, TakeOutShovel, NULL);
    Z64Items_SetHeldCallbacks(&shovel, UseShovel, StopDigging, NULL);
    shovel.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    shovel.getItemEntry.drawFunc = DrawGetItem;

    if (!Z64Items_Register(sApi, &shovel)) {
        return;
    }
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawAfterPlayer);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, StartPendingDig);
}
