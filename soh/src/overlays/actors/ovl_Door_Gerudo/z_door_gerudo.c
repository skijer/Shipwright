/*
 * File: z_door_gerudo.c
 * Overlay: ovl_Door_Gerudo
 * Description: Metal grate door
 */

#include "z_door_gerudo.h"
#include "objects/object_door_gerudo/object_door_gerudo.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#define FLAGS 0

void DoorGerudo_Init(Actor* thisx, PlayState* play);
void DoorGerudo_Destroy(Actor* thisx, PlayState* play);
void DoorGerudo_Update(Actor* thisx, PlayState* play);
void DoorGerudo_Draw(Actor* thisx, PlayState* play);

void func_8099485C(DoorGerudo* this, PlayState* play);
s32 func_80994750(DoorGerudo* this, PlayState* play, Player** outPlayer);
void func_8099496C(DoorGerudo* this, PlayState* play);
void func_809949C8(DoorGerudo* this, PlayState* play);

const ActorInit Door_Gerudo_InitVars = {
    ACTOR_DOOR_GERUDO,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_DOOR_GERUDO,
    sizeof(DoorGerudo),
    (ActorFunc)DoorGerudo_Init,
    (ActorFunc)DoorGerudo_Destroy,
    (ActorFunc)DoorGerudo_Update,
    (ActorFunc)DoorGerudo_Draw,
    NULL,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F(scale, 1, ICHAIN_STOP),
};

void DoorGerudo_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    DoorGerudo* this = (DoorGerudo*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_ProcessInitChain(&this->dyna.actor, sInitChain);
    CollisionHeader_GetVirtual(&gGerudoCellDoorCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

    if (Flags_GetSwitch(play, thisx->params & 0x3F)) {
        this->actionFunc = func_8099485C;
        thisx->world.pos.y = thisx->home.pos.y + 200.0f;
    } else {
        this->actionFunc = func_8099485C;
        this->unk_166 = 10;
    }
}

void DoorGerudo_Destroy(Actor* thisx, PlayState* play) {
    DoorGerudo* this = (DoorGerudo*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

f32 func_809946BC(DoorGerudo* this, Player* player, f32 arg2, f32 arg3, f32 arg4) {
    Vec3f playerPos;
    Vec3f sp1C;

    playerPos.x = player->actor.world.pos.x;
    playerPos.y = player->actor.world.pos.y + arg2;
    playerPos.z = player->actor.world.pos.z;
    Actor_WorldToActorCoords(&this->dyna.actor, &sp1C, &playerPos);

    if ((arg3 < fabsf(sp1C.x)) || (arg4 < fabsf(sp1C.y))) {
        return FLT_MAX;
    } else {
        return sp1C.z;
    }
}

s32 func_80994750(DoorGerudo* this, PlayState* play, Player** outPlayer) {
    Actor* actor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;
    f32 bestAbsRelZ = 1000000000.0f;
    s32 bestDirection = 0;
    s32 sanity = 0;

    *outPlayer = NULL;

    if (!Player_InCsMode(play)) {
        while ((actor != NULL) && (sanity < 2000)) {
            if ((actor->id == ACTOR_PLAYER) && (actor->update != NULL)) {
                Player* player = (Player*)actor;
                f32 relZ = func_809946BC(this, player, 0.0f, 20.0f, 15.0f);

                if (fabsf(relZ) < 40.0f) {
                    s16 rotYDiff = player->actor.shape.rot.y - this->dyna.actor.shape.rot.y;

                    if (relZ > 0.0f) {
                        rotYDiff = 0x8000 - rotYDiff;
                    }
                    if (ABS(rotYDiff) < 0x2000) {
                        f32 absRelZ = fabsf(relZ);

                        if (absRelZ < bestAbsRelZ) {
                            bestAbsRelZ = absRelZ;
                            bestDirection = (relZ >= 0.0f) ? 1 : -1;
                            *outPlayer = player;
                        }
                    }
                }
            }

            actor = actor->next;
            sanity++;
        }
    }

    return bestDirection;
}

void func_8099485C(DoorGerudo* this, PlayState* play) {
    if (this->unk_164 != 0) {
        this->actionFunc = func_8099496C;
        gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] -= 1;
        Flags_SetSwitch(play, this->dyna.actor.params & 0x3F);
        Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_CHAIN_KEY_UNLOCK);
        GameInteractor_ExecuteOnDungeonKeyUsedHooks(gSaveContext.mapIndex);
    } else {
        Player* player = NULL;
        s32 direction = func_80994750(this, play, &player);

        if ((direction != 0) && (player != NULL)) {

            if (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] <= 0) {
                player->naviTextId = -0x203;
            } else if (!Flags_GetCollectible(play, (this->dyna.actor.params >> 8) & 0x1F)) {
                player->naviTextId = -0x225;
            } else {
                player->doorType = PLAYER_DOORTYPE_SLIDING;
                player->doorDirection = direction;
                player->doorActor = &this->dyna.actor;
                player->doorTimer = 10;
            }
        }
    }
}

void func_8099496C(DoorGerudo* this, PlayState* play) {
    if (DECR(this->unk_166) == 0) {
        Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_SLIDE_DOOR_OPEN);
        this->actionFunc = func_809949C8;
    }
}

void func_809949C8(DoorGerudo* this, PlayState* play) {
    Math_StepToF(&this->dyna.actor.velocity.y, 15.0f, 3.0f);
    Math_StepToF(&this->dyna.actor.world.pos.y, this->dyna.actor.home.pos.y + 200.0f, this->dyna.actor.velocity.y);
}

void DoorGerudo_Update(Actor* thisx, PlayState* play) {
    DoorGerudo* this = (DoorGerudo*)thisx;

    this->actionFunc(this, play);
}

void DoorGerudo_Draw(Actor* thisx, PlayState* play) {
    DoorGerudo* this = (DoorGerudo*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gGerudoCellDoorDL);

    if (this->unk_166 != 0) {
        Matrix_Scale(0.01f, 0.01f, 0.025f, MTXMODE_APPLY);
        Actor_DrawDoorLock(play, this->unk_166, DOORLOCK_NORMAL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
