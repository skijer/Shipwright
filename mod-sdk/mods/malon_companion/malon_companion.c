// Romani Partner concept and animation source: Luv4os, CC0 (see reference/).
// Independent actor: never runs the original Malon quest or audio callbacks.
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "functions.h"
#include "variables.h"
#include "macros.h"
#include "soh/ModApi/ModApi.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_ma1/object_ma1.h"
#include "objects/object_ma2/object_ma2.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "partner_control.h"
#ifdef _MSC_VER
#pragma warning(error: 4013)
__declspec(dllimport) void EnArrow_Fly(EnArrow*, PlayState*);
#else
void EnArrow_Fly(EnArrow*, PlayState*);
#endif

#define MALON_ENABLED "gMods.MalonCompanion.Enabled"
#define MALON_KEY "nei.malon_companion"
#define MALON_JOINTS 19
#define MALON_FOLLOW_DISTANCE 65.0f
#define MALON_ATTACK_RANGE 550.0f
#include "bow_socket.inc.c"

typedef enum { CLIP_IDLE, CLIP_LOOK, CLIP_RUN, CLIP_SIT, CLIP_SHOOT, CLIP_FALL,
               CLIP_JUMP, CLIP_MOUNT, CLIP_RIDE, CLIP_AIM_RUN, CLIP_COUNT } MalonClip;
typedef enum { MALON_FOLLOW, MALON_AIM, MALON_SHOOT, MALON_FALL, MALON_HELD,
               MALON_THROWN, MALON_LAND, MALON_MOUNT, MALON_RIDE } MalonState;
typedef struct {
    Actor actor;
    SkelAnime skel;
    Vec3s pose[MALON_JOINTS], morph[MALON_JOINTS];
    ColliderCylinder collider;
    MalonState state;
    MalonClip clip;
    s16 cooldown, stateTicks, blink, headYaw, headPitch;
    u8 child, ready, colliderReady, shot;
    Actor* target;
    Actor* horse;
    Vec3f hand;
    u8 handReady;
    u8 controlled;
    Input input;
    s16 cameraYaw;
} Malon;

static const SOHModApi* sApi;
static Malon* sMalon;
static s16 sActorId = -1;
static s16 sSpawnDelay;
static bool sRegistered;
static const char* sHooks[] = { "OnGameFrameUpdate", "OnSceneInit", "OnLoadGame", "OnExitGame" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sHooks, ARRAY_COUNT(sHooks) };

#define CLIPS(age) \
    "__OTR__animations/nei_malon/" age "/idle", "__OTR__animations/nei_malon/" age "/lookaround", \
    "__OTR__animations/nei_malon/" age "/run", "__OTR__animations/nei_malon/" age "/sitting", \
    "__OTR__animations/nei_malon/" age "/shootbow", "__OTR__animations/nei_malon/" age "/fall", \
    "__OTR__animations/nei_malon/" age "/jump", "__OTR__animations/nei_malon/" age "/eponamount", \
    "__OTR__animations/nei_malon/" age "/eponaride", "__OTR__animations/nei_malon/" age "/aimbowrun"
static const char* sClipPaths[2][CLIP_COUNT] = { { CLIPS("adult") }, { CLIPS("child") } };
static const ALIGN_ASSET(2) char sBow[] = "__OTR__objects/object_nei_malon/bow_dl";

static ColliderCylinderInit sCollider = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0,0,0 }, { 0,0,0 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 15, 55, 0, { 0,0,0 } },
};

static bool ActorExists(PlayState* play, const Actor* wanted) {
    if (wanted == NULL) return false;
    for (s32 cat = 0; cat < ACTORCAT_MAX; ++cat)
        for (Actor* actor = play->actorCtx.actorLists[cat].head; actor != NULL; actor = actor->next)
            if (actor == wanted) return actor->update != NULL;
    return false;
}

static bool Busy(PlayState* play) {
    Player* player = GET_PLAYER(play);
    return play->pauseCtx.state != 0 || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->csCtx.state != CS_STATE_IDLE || play->msgCtx.msgMode != MSGMODE_NONE ||
        player == NULL || player->actor.update == NULL ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_GETTING_ITEM |
                                PLAYER_STATE1_IN_CUTSCENE));
}

static void PlayClip(Malon* self, MalonClip clip, bool once) {
    if (!self->ready || self->clip == clip) return;
    AnimationHeader* animation = (AnimationHeader*)ResourceMgr_LoadAnimByName(sClipPaths[self->child][clip]);
    if (animation == NULL) return;
    Animation_Change(&self->skel, animation, 1.0f, 0, Animation_GetLastFrame(animation),
        once ? ANIMMODE_ONCE_INTERP : ANIMMODE_LOOP_INTERP, -4.0f);
    self->clip = clip;
}

static void SetState(Malon* self, MalonState state) {
    self->state = state;
    self->stateTicks = 0;
    self->actor.speedXZ = 0;
    self->shot = false;
    self->handReady = false;
}

static bool ClearLine(PlayState* play, Vec3f from, Vec3f to) {
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bg;
    return !BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bg);
}

static bool FloorAt(PlayState* play, Actor* actor, Vec3f* point, f32 depth) {
    Vec3f from = *point;
    CollisionPoly* poly = NULL;
    s32 bg;
    from.y += 70;
    f32 y = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bg, actor, &from);
    if (poly == NULL || y <= BGCHECK_Y_MIN || y > point->y + 55 || y < point->y - depth) return false;
    point->y = y;
    return true;
}

static bool Recall(Malon* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || player->rideActor != NULL ||
        self->actor.parent != NULL) return false;
    Vec3f point = player->actor.world.pos;
    point.x -= Math_SinS(player->actor.shape.rot.y) * 55;
    point.z -= Math_CosS(player->actor.shape.rot.y) * 55;
    Vec3f from = player->actor.world.pos, to = point;
    from.y += 35; to.y += 35;
    if (!FloorAt(play, &self->actor, &point, 65) || !ClearLine(play, from, to)) point = player->actor.world.pos;
    self->actor.world.pos = point;
    self->actor.prevPos = point;
    self->actor.velocity = (Vec3f){0,0,0};
    self->actor.speedXZ = 0;
    self->actor.gravity = -1.0f;
    self->target = NULL;
    self->horse = NULL;
    self->cooldown = 20;
    self->actor.room = player->actor.room;
    SetState(self, MALON_FOLLOW);
    PlayClip(self, CLIP_IDLE, false);
    Actor_UpdateBgCheckInfo(play, &self->actor, 20, 15, 0, 5 /* walls and floor */);
    return true;
}

static bool CanTarget(Malon* self, PlayState* play, Actor* actor) {
    if (!ActorExists(play, actor) || actor->draw == NULL ||
        !(actor->flags & ACTOR_FLAG_HOSTILE) || actor->colChkInfo.health == 0 ||
        (actor->category != ACTORCAT_ENEMY && actor->category != ACTORCAT_BOSS)) return false;
    if (Actor_WorldDistXYZToActor(&self->actor, actor) > MALON_ATTACK_RANGE ||
        Actor_WorldDistXYZToActor(&GET_PLAYER(play)->actor, actor) > 750) return false;
    Vec3f from = self->actor.world.pos, to = actor->focus.pos;
    from.y += self->child ? 32 : 50;
    if (Math_Vec3f_DistXYZ(&to, &actor->world.pos) > 180) { to = actor->world.pos; to.y += 25; }
    return ClearLine(play, from, to);
}

static Actor* FindTarget(Malon* self, PlayState* play) {
    Actor* focused = GET_PLAYER(play)->focusActor;
    if (CanTarget(self, play, focused)) return focused;
    Actor* best = NULL;
    f32 distance = MALON_ATTACK_RANGE;
    for (s32 cat = ACTORCAT_ENEMY; cat <= ACTORCAT_BOSS; cat += ACTORCAT_BOSS-ACTORCAT_ENEMY) {
        for (Actor* actor = play->actorCtx.actorLists[cat].head; actor != NULL; actor = actor->next) {
            f32 d = Actor_WorldDistXYZToActor(&self->actor, actor);
            if (d < distance && CanTarget(self, play, actor)) { best = actor; distance = d; }
        }
    }
    return best;
}

static void Fire(Malon* self, PlayState* play) {
    bool targeted = CanTarget(self, play, self->target);
    if (!targeted && !self->controlled) return;
    Vec3f from = self->actor.world.pos, to;
    from.y += self->child ? 32 : 50;
    if (self->handReady && Math_Vec3f_DistXYZ(&self->hand, &from) < 60) from = self->hand;
    if (targeted) {
        to = self->target->focus.pos;
        if (Math_Vec3f_DistXYZ(&to, &self->target->world.pos) > 180) { to = self->target->world.pos; to.y += 25; }
    } else {
        to = from;
        to.x += Math_SinS(self->actor.shape.rot.y)*500;
        to.z += Math_CosS(self->actor.shape.rot.y)*500;
    }
    EnArrow* arrow = (EnArrow*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW,
        from.x,from.y,from.z,Math_Vec3f_Pitch(&from,&to),Math_Vec3f_Yaw(&from,&to),0,ARROW_NORMAL);
    if (arrow == NULL) return;
    // Native collision/damage and flight, independent from Link's release state and ammunition.
    arrow->actionFunc = EnArrow_Fly;
    arrow->unk_210 = from;
    arrow->timer = 12;
    Actor_SetProjectileSpeed(&arrow->actor, 150);
    Audio_PlayActorSound2(&self->actor, NA_SE_IT_ARROW_SHOT);
}

static void Walk(Malon* self, PlayState* play, Vec3f goal, f32 stop, bool aiming) {
    Actor* actor = &self->actor;
    f32 distance = Math_Vec3f_DistXZ(&actor->world.pos, &goal);
    f32 speed = 0;
    if (distance > stop) {
        s16 yaw = Math_Vec3f_Yaw(&actor->world.pos,&goal);
        Math_SmoothStepToS(&actor->world.rot.y, yaw, 3, 0x1400, 0x40);
        actor->shape.rot.y = actor->world.rot.y;
        Vec3f probe = actor->world.pos;
        probe.x += Math_SinS(actor->world.rot.y)*24;
        probe.z += Math_CosS(actor->world.rot.y)*24;
        Vec3f from = actor->world.pos, to = probe;
        from.y += 30; to.y += 30;
        if (FloorAt(play,actor,&probe,55) && ClearLine(play,from,to))
            speed = aiming ? 2.5f : MIN(8.0f, MAX(3.0f,(distance-stop)*.075f));
        else if (!aiming && distance > 90 && (actor->bgCheckFlags & BGCHECKFLAG_GROUND) &&
                 goal.y <= actor->world.pos.y+35 && goal.y >= actor->world.pos.y-100) {
            Vec3f landing = actor->world.pos;
            landing.x += Math_SinS(actor->world.rot.y)*80;
            landing.z += Math_CosS(actor->world.rot.y)*80;
            to = landing; to.y += 35;
            if (FloorAt(play,actor,&landing,110) && ClearLine(play,from,to)) {
                SetState(self,MALON_FALL); PlayClip(self,CLIP_JUMP,true);
                actor->velocity.y = 7;
                actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
                actor->speedXZ = 5.5f;
                return;
            }
        }
    }
    Math_ApproachF(&actor->speedXZ,speed,.35f,1.5f);
    PlayClip(self, actor->speedXZ > .2f ? (aiming ? CLIP_AIM_RUN : CLIP_RUN) : CLIP_IDLE, false);
    if (self->clip == CLIP_RUN || self->clip == CLIP_AIM_RUN)
        self->skel.playSpeed = CLAMP(actor->speedXZ/5.5f,.6f,1.6f);
    if (actor->speedXZ > .2f) self->cooldown = MAX(self->cooldown, 1);
}

static void RidePosition(Malon* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* horse = player->rideActor;
    if (!ActorExists(play,horse)) return;
    self->actor.world.pos = player->actor.world.pos;
    self->actor.world.pos.x -= Math_SinS(player->actor.shape.rot.y)*22;
    self->actor.world.pos.z -= Math_CosS(player->actor.shape.rot.y)*22;
    // The seated retarget already contributes pelvis height; do not add it twice.
    self->actor.world.pos.y += self->child ? 8 : -5;
    self->actor.world.rot.y = self->actor.shape.rot.y = player->actor.shape.rot.y;
    self->actor.velocity = (Vec3f){0,0,0};
    self->actor.gravity = 0;
    self->actor.speedXZ = 0;
    self->actor.room = player->actor.room;
    self->horse = horse;
}

static void DetachPlayer(Malon* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    if (player != NULL && player->heldActor == &self->actor) {
        player->heldActor = NULL;
        player->stateFlags1 &= ~PLAYER_STATE1_CARRYING_ACTOR;
    }
    if (player != NULL && player->actor.child == &self->actor) player->actor.child = NULL;
    if (player != NULL && player->interactRangeActor == &self->actor) player->interactRangeActor = NULL;
    self->actor.parent = NULL;
}

static void ControlledUpdate(Malon* self, PlayState* play) {
    Actor* actor = &self->actor;
    Input* input = &self->input;
    bool ground = (actor->bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    if (self->target && !CanTarget(self,play,self->target)) self->target = NULL;
    if (self->state == MALON_SHOOT) {
        actor->speedXZ = 0;
        if (self->target) actor->world.rot.y = actor->shape.rot.y = Actor_WorldYawTowardActor(actor,self->target);
        if (!self->shot && self->skel.curFrame >= 23) { Fire(self,play); self->shot = true; }
        if (self->skel.curFrame >= self->skel.endFrame) {
            SetState(self,ground ? MALON_FOLLOW : MALON_FALL);
            self->cooldown = 12;
            PlayClip(self,ground ? CLIP_IDLE : CLIP_FALL,false);
        }
    } else {
        f32 x = input->cur.stick_x, y = input->cur.stick_y;
        f32 magnitude = sqrtf(x*x+y*y);
        if (magnitude > 10) {
            s16 yaw = self->cameraYaw+(s16)(atan2f(x,y)*(32768.0f/M_PI));
            Math_SmoothStepToS(&actor->world.rot.y,yaw,2,0x1800,0x40);
            actor->shape.rot.y = actor->world.rot.y;
        }
        Math_ApproachF(&actor->speedXZ,magnitude > 10 ? MIN(7,(magnitude-10)*.115f) : 0,.4f,1.5f);
        if (ground) {
            if (self->state != MALON_FOLLOW) {
                f32 speed = actor->speedXZ;
                SetState(self,MALON_FOLLOW);
                actor->speedXZ = speed;
            }
            PlayClip(self,actor->speedXZ > .2f ? CLIP_RUN : CLIP_IDLE,false);
            if (input->press.button & BTN_A) {
                SetState(self,MALON_FALL); PlayClip(self,CLIP_JUMP,true);
                actor->velocity.y = 7;
            actor->speedXZ = magnitude > 10 ? MIN(7,(magnitude-10)*.115f) : 0;
                actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
                Audio_PlayActorSound2(actor,NA_SE_PL_JUMP);
            }
        } else if (actor->velocity.y <= 0) PlayClip(self,CLIP_FALL,false);
        if ((input->press.button & BTN_B) && self->cooldown == 0) {
            self->target = FindTarget(self,play);
            SetState(self,MALON_SHOOT); PlayClip(self,CLIP_SHOOT,true);
            Audio_PlayActorSound2(actor,NA_SE_IT_BOW_DRAW);
        }
    }
    actor->gravity = -1;
    Actor_MoveXZGravity(actor);
    Actor_UpdateBgCheckInfo(play,actor,20,15,self->child ? 40 : 65,7);
    if ((actor->bgCheckFlags & BGCHECKFLAG_CEILING) && actor->velocity.y > 0) actor->velocity.y = 0;
    SkelAnime_Update(&self->skel);
    Collider_UpdateCylinder(actor,&self->collider);
    CollisionCheck_SetOC(play,&play->colChkCtx,&self->collider.base);
    input->press.button = input->rel.button = 0;
}

static Actor* PartnerActor(PlayState* play) {
    return sMalon && ActorExists(play,&sMalon->actor) && CVarGetInteger(MALON_ENABLED,0) ? &sMalon->actor : NULL;
}
static bool PartnerBegin(PlayState* play) {
    if (!PartnerActor(play) || Busy(play) || sMalon->actor.parent || sMalon->horse ||
        !(sMalon->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) return false;
    sMalon->controlled = true; sMalon->target = NULL;
    memset(&sMalon->input,0,sizeof(sMalon->input));
    SetState(sMalon,MALON_FOLLOW); PlayClip(sMalon,CLIP_IDLE,false);
    return true;
}
static void PartnerEnd(PlayState* play) {
    if (!PartnerActor(play)) return;
    sMalon->controlled = false; sMalon->target = NULL; sMalon->cooldown = 20;
    SetState(sMalon,MALON_FOLLOW); PlayClip(sMalon,CLIP_IDLE,false);
}
static void PartnerInput(PlayState* play, const Input* input, s16 cameraYaw) {
    if (!PartnerActor(play) || !sMalon->controlled) return;
    sMalon->input = *input; sMalon->cameraYaw = cameraYaw;
}
static void RegisterPartner(void) {
    if (sRegistered || !SOH_MOD_API_HAS(sApi,FindService)) return;
    NeiRegisterPartnerV1 registerPartner = (NeiRegisterPartnerV1)sApi->FindService("nei.partner.register.v1");
    if (!registerPartner) return;
    static const NeiPartnerV1 partner = {sizeof(NeiPartnerV1),MALON_KEY,"Malon",PartnerActor,PartnerBegin,PartnerEnd,PartnerInput};
    sRegistered = registerPartner(&partner);
}

static void InitActor(Actor* actor, PlayState* play) {
    Malon* self = (Malon*)actor;
    self->child = LINK_IS_ADULT ? 0 : 1;
    Actor_SetScale(actor,.01f);
    actor->gravity = -1;
    actor->minVelocityY = -20;
    actor->room = -1; // Preserve the partner across room changes, including while carried.
    ActorShape_Init(&actor->shape,0,ActorShadow_DrawCircle,18);
    Collider_InitCylinder(play,&self->collider);
    Collider_SetCylinder(play,&self->collider,actor,&sCollider);
    self->collider.dim.height = self->child ? 40 : 65;
    self->colliderReady = true;
    const char* skeleton = self->child ? gMalonChildSkel : gMalonAdultSkel;
    if (!sApi->HasResource(skeleton) || !sApi->HasResource(sClipPaths[self->child][0])) { Actor_Kill(actor); return; }
    SkelAnime_InitFlex(play,&self->skel,(FlexSkeletonHeader*)skeleton,
        (AnimationHeader*)sClipPaths[self->child][CLIP_IDLE],self->pose,self->morph,MALON_JOINTS);
    self->ready = true;
    self->clip = CLIP_IDLE;
    SetState(self,MALON_FOLLOW);
    PlayClip(self,CLIP_IDLE,false);
    self->cooldown = 25;
    sMalon = self;
    Actor_UpdateBgCheckInfo(play,actor,20,15,0,5 /* walls and floor */);
}

static void DestroyActor(Actor* actor, PlayState* play) {
    Malon* self = (Malon*)actor;
    DetachPlayer(self,play);
    if (self->colliderReady) Collider_DestroyCylinder(play,&self->collider);
    // Pose arrays are embedded, not allocated by SkelAnime_InitFlex.
    if (self->ready) ResourceMgr_UnregisterSkeleton(&self->skel);
    if (sMalon == self) sMalon = NULL;
}

static void UpdateActor(Actor* actor, PlayState* play) {
    Malon* self = (Malon*)actor;
    Player* player = GET_PLAYER(play);
    if (!CVarGetInteger(MALON_ENABLED,0) || self->child != !LINK_IS_ADULT) { Actor_Kill(actor); return; }
    if (!self->ready || Busy(play)) return;
    ++self->stateTicks;
    if (self->cooldown > 0) --self->cooldown;
    if (++self->blink >= 74) self->blink = 0;
    actor->room = -1;
    if (self->controlled) { ControlledUpdate(self,play); return; }

    if (actor->parent != NULL) {
        if (self->state != MALON_HELD) { SetState(self,MALON_HELD); PlayClip(self,CLIP_SIT,false); }
        actor->speedXZ = actor->velocity.y = 0;
        actor->shape.shadowAlpha = 0;
        SkelAnime_Update(&self->skel);
        return;
    }
    actor->shape.shadowAlpha = 160;
    if (self->state == MALON_HELD) {
        SetState(self,MALON_THROWN);
        PlayClip(self,CLIP_SIT,false);
        actor->gravity = -1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    }

    bool riding = ActorExists(play,player->rideActor);
    if (riding && self->state != MALON_THROWN && self->state != MALON_LAND) {
        if (self->horse != player->rideActor) { SetState(self,MALON_MOUNT); PlayClip(self,CLIP_MOUNT,true); }
        RidePosition(self,play);
        if (self->state == MALON_MOUNT && self->stateTicks >= 27) {
            SetState(self,MALON_RIDE); PlayClip(self,CLIP_RIDE,false);
        }
    } else if (self->horse != NULL) {
        self->horse = NULL;
        actor->gravity = -1;
        SetState(self,MALON_FALL); PlayClip(self,CLIP_FALL,false);
    }

    if (!riding && self->state != MALON_THROWN && self->state != MALON_LAND &&
        (Actor_WorldDistXZToActor(actor,&player->actor) > 650 ||
         fabsf(actor->world.pos.y-player->actor.world.pos.y) > 240 ||
         actor->world.pos.y <= BGCHECK_Y_MIN+100 || (actor->bgCheckFlags & BGCHECKFLAG_WATER))) Recall(self,play);

    if (self->state == MALON_THROWN || self->state == MALON_FALL) {
        actor->gravity = -1;
        Actor_MoveXZGravity(actor);
        Actor_UpdateBgCheckInfo(play,actor,20,15,0,5 /* walls and floor */);
        if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
            actor->velocity.y = 0;
            actor->speedXZ = 0;
            SetState(self,self->state == MALON_THROWN ? MALON_LAND : MALON_FOLLOW);
            PlayClip(self,self->state == MALON_LAND ? CLIP_LOOK : CLIP_IDLE,false);
        }
    } else if (self->state == MALON_LAND) {
        if (self->stateTicks >= 24) SetState(self,MALON_FOLLOW);
    } else if (self->state != MALON_MOUNT) {
        if (self->target != NULL && !CanTarget(self,play,self->target)) {
            self->target = NULL;
            SetState(self,riding ? MALON_RIDE : MALON_FOLLOW);
            PlayClip(self,riding ? CLIP_RIDE : CLIP_IDLE,false);
        }
        if (self->state != MALON_SHOOT && self->cooldown == 0) self->target = FindTarget(self,play);
        if (self->target != NULL) {
            Vec3f goal = self->target->world.pos;
            s16 yaw = Math_Vec3f_Yaw(&actor->world.pos,&goal);
            Math_SmoothStepToS(&actor->shape.rot.y,yaw,3,0x1200,0x40);
            actor->world.rot.y = actor->shape.rot.y;
            if (self->state != MALON_SHOOT) {
                if (!riding) Walk(self,play,goal,180,true);
                if (self->cooldown == 0 && (riding || Actor_WorldDistXZToActor(actor,self->target) < 350)) {
                    SetState(self,MALON_SHOOT); PlayClip(self,CLIP_SHOOT,true);
                    Audio_PlayActorSound2(actor,NA_SE_IT_BOW_DRAW);
                }
            } else {
                if (!self->shot && self->skel.curFrame >= 23) { Fire(self,play); self->shot = true; }
                if (self->skel.curFrame >= self->skel.endFrame) {
                    self->cooldown = 35;
                    SetState(self,riding ? MALON_RIDE : MALON_FOLLOW);
                    PlayClip(self,riding ? CLIP_RIDE : CLIP_IDLE,false);
                }
            }
        } else if (!riding) {
            Walk(self,play,player->actor.world.pos,MALON_FOLLOW_DISTANCE,false);
            if (actor->speedXZ < .2f && actor->xzDistToPlayer < 85)
                Math_SmoothStepToS(&actor->shape.rot.y,actor->yawTowardsPlayer,5,0x700,0x20);
        }
        if (!riding) {
            actor->gravity = -1;
            Actor_MoveXZGravity(actor);
            Actor_UpdateBgCheckInfo(play,actor,20,15,0,5 /* walls and floor */);
            if (!(actor->bgCheckFlags & BGCHECKFLAG_GROUND) && self->state != MALON_FALL) {
                SetState(self,MALON_FALL); PlayClip(self,CLIP_FALL,false);
            }
            if (self->state != MALON_SHOOT && self->target == NULL) Actor_OfferGetItem(actor,play,GI_NONE,55,30);
        }
    }
    Vec3f look = player->actor.focus.pos;
    if (self->target != NULL && ActorExists(play,self->target)) look = self->target->focus.pos;
    Vec3f eye = actor->world.pos; eye.y += self->child ? 38 : 62;
    s16 yaw = Math_Vec3f_Yaw(&eye,&look)-actor->shape.rot.y;
    s16 pitch = Math_Vec3f_Pitch(&eye,&look);
    Math_SmoothStepToS(&self->headYaw,CLAMP(yaw,-0x1800,0x1800),5,0x500,0);
    Math_SmoothStepToS(&self->headPitch,CLAMP(pitch,-0x1000,0x1000),5,0x400,0);
    SkelAnime_Update(&self->skel);
    Collider_UpdateCylinder(actor,&self->collider);
    if (!riding && self->state != MALON_HELD) CollisionCheck_SetOC(play,&play->colChkCtx,&self->collider.base);
}

static s32 OverrideLimb(PlayState* play, s32 limb, Gfx** dl, Vec3f* pos, Vec3s* rot, void* arg) {
    Malon* self = arg;
    if (limb == (self->child ? 15 : 18)) { rot->x += self->headYaw; rot->z += self->headPitch; }
    return false;
}

static void PostLimb(PlayState* play, s32 limb, Gfx** dl, Vec3s* rot, void* arg) {
    Malon* self = arg;
    bool bow = self->target != NULL || self->state == MALON_SHOOT;
    if (limb == (self->child ? 15 : 18)) {
        Vec3f zero = {0,0,0}; Matrix_MultVec3f(&zero,&self->actor.focus.pos);
    }
    if ((bow && limb == (self->child ? 11 : 14)) || (!bow && limb == (self->child ? 8 : 11))) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        if (!bow) {
            Matrix_Translate(350,-640,50,MTXMODE_APPLY);
            Matrix_RotateZYX(0x4000,0x1388,0x1700,MTXMODE_APPLY);
        }
        // Apply the same global rest-pose correction used by the baked retarget.
        // Bow socket is generated from source/target hand rest orientations.
        if (bow) Matrix_RotateZYX(sBowSocket[self->child][0],sBowSocket[self->child][1],
                                  sBowSocket[self->child][2],MTXMODE_APPLY);
        Matrix_Scale(self->child ? 1.0f : 1.45f,self->child ? 1.0f : 1.45f,self->child ? 1.0f : 1.45f,MTXMODE_APPLY);
        if (bow) { Vec3f zero = {0,0,0}; Matrix_MultVec3f(&zero,&self->hand); self->handReady = true; }
        gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sBow);
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static void DrawActor(Actor* actor, PlayState* play) {
    Malon* self = (Malon*)actor;
    if (!self->ready) return;
    const char* eyes;
    if (self->child) eyes = self->blink >= 72 ? gMalonChildEyeClosedTex :
        self->blink >= 70 ? gMalonChildEyeHalfTex : gMalonChildEyeOpenTex;
    else eyes = self->blink >= 72 ? gMalonAdultEyeClosedTex :
        self->blink >= 70 ? gMalonAdultEyeHalfTex : gMalonAdultEyeOpenTex;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++,8,(uintptr_t)eyes);
    gSPSegment(POLY_OPA_DISP++,9,(uintptr_t)(self->child ? gMalonChildSmilingMouthTex : gMalonAdultMouthHappyTex));
    SkelAnime_DrawSkeletonOpa(play,&self->skel,OverrideLimb,PostLimb,self);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void Scene(s16 scene) { sMalon = NULL; sSpawnDelay = 5; }
static void Load(s32 file) { sMalon = NULL; sSpawnDelay = 5; }
static void Exit(s32 file) { sMalon = NULL; sSpawnDelay = 5; }

static void Tick(void) {
    RegisterPartner();
    PlayState* play = gPlayState;
    if (play == NULL || sActorId < 0) return;
    if (sMalon != NULL && !ActorExists(play,&sMalon->actor)) sMalon = NULL;
    if (!CVarGetInteger(MALON_ENABLED,0) || Busy(play) || sMalon != NULL) return;
    if (sSpawnDelay > 0) { --sSpawnDelay; return; }
    Player* player = GET_PLAYER(play);
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && player->rideActor == NULL) return;
    Vec3f pos = player->actor.world.pos;
    Actor_Spawn(&play->actorCtx,play,sActorId,pos.x,pos.y,pos.z,0,player->actor.shape.rot.y,0,0);
    sSpawnDelay = 20;
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) { sApi = api; }
SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) { return &sRequirements; }
SOH_MOD_EXPORT void ModInit(void) {
    if (!SOH_MOD_API_HAS(sApi,RegisterActor) || !SOH_MOD_API_HAS(sApi,HasResource)) return;
    SOHActorDefinition actor = {0};
    actor.structSize = sizeof(actor); actor.key = MALON_KEY; actor.description = "Malon companion";
    actor.category = ACTORCAT_NPC; actor.objectId = OBJECT_GAMEPLAY_KEEP;
    actor.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_CAN_PRESS_SWITCHES |
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY;
    actor.instanceSize = sizeof(Malon); actor.init = InitActor; actor.destroy = DestroyActor;
    actor.update = UpdateActor; actor.draw = DrawActor;
    sActorId = sApi->RegisterActor(&actor);
    if (SOH_MOD_API_HAS(sApi,RegisterMenuSidebar) && SOH_MOD_API_HAS(sApi,RegisterMenuWidget)) {
        sApi->RegisterMenuSidebar("Mods","Partners","gMods.Menu.Page",1);
        SOHModMenuWidget option = {sizeof(option)};
        option.section = "Mods"; option.sidebar = "Partners"; option.column = 0;
        option.type = SOH_MOD_MENU_CHECKBOX; option.label = "Malon Companion";
        option.cvar = MALON_ENABLED;
        option.tooltip = "Malon follows Link, shoots enemies, rides Epona and can be carried. Her age matches Link.";
        sApi->RegisterMenuWidget(&option);
    }
    SOH_REGISTER_HOOK(sApi,OnGameFrameUpdate,Tick);
    SOH_REGISTER_HOOK(sApi,OnSceneInit,Scene);
    SOH_REGISTER_HOOK(sApi,OnLoadGame,Load);
    SOH_REGISTER_HOOK(sApi,OnExitGame,Exit);
}
