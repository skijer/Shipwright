// Native A hops stay native: only the second clip changes. The third landing cues a timed B parry.
static const ALIGN_ASSET(2) char sHopLeft[] = "__OTR__misc/link_animetion/nei_pendant_hop_left";
static const ALIGN_ASSET(2) char sHopRight[] = "__OTR__misc/link_animetion/nei_pendant_hop_right";
static Actor* sBoundsActor;
static f32 sBoundsTop;
static f32 sArcHeight;
static f32 sArcImpact;
static bool sIsRolling;
static LinkAnimationHeader* sSecondHopClip;

static bool IsParryTarget(Actor* target) {
    // Bosses, including registered custom bosses, use their own category.
    return target != NULL && target->update != NULL && target->category == ACTORCAT_ENEMY &&
           (target->flags & ACTOR_FLAG_ATTENTION_ENABLED) != 0;
}

static void SampleTargetBounds(void* actorPtr) {
    Actor* actor = (Actor*)actorPtr;
    PlayState* play = gPlayState;
    if (play == NULL || !IsActive() || !IsParryTarget(actor)) return;
    Player* player = GET_PLAYER(play);
    if (actor != player->focusActor && actor != sPendant.target) return;
    f32 top = -1.0e20f;
    for (s32 i = 0; i < play->colChkCtx.colACCount; i++) {
        Collider* col = play->colChkCtx.colAC[i];
        if (col == NULL || col->actor != actor || !(col->acFlags & AC_ON)) continue;
        if (col->shape == COLSHAPE_CYLINDER) {
            Cylinder16* dim = &((ColliderCylinder*)col)->dim;
            top = MAX(top, dim->pos.y + dim->yShift + dim->height);
        } else if (col->shape == COLSHAPE_JNTSPH) {
            ColliderJntSph* sph = (ColliderJntSph*)col;
            for (s32 j = 0; j < sph->count; j++) {
                Sphere16* dim = &sph->elements[j].dim.worldSphere;
                top = MAX(top, dim->center.y + dim->radius);
            }
        } else if (col->shape == COLSHAPE_QUAD) {
            for (s32 j = 0; j < 4; j++) top = MAX(top, ((ColliderQuad*)col)->dim.quad[j].y);
        } else if (col->shape == COLSHAPE_TRIS) {
            ColliderTris* tris = (ColliderTris*)col;
            for (s32 j = 0; j < tris->count; j++)
                for (s32 k = 0; k < 3; k++) top = MAX(top, tris->elements[j].dim.vtx[k].y);
        }
    }
    if (top > -1.0e19f) { sBoundsActor = actor; sBoundsTop = top; }
}

static void SilenceSwordQuads(Player* player) {
    player->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    player->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
    player->meleeWeaponState = 0;
}

static bool MoveAlongArc(PlayState* play, Player* player, Vec3f* next) {
    Vec3f from = player->actor.world.pos, to = *next, hit;
    CollisionPoly* poly = NULL;
    s32 bgId;
    from.y += 20.0f; to.y += 20.0f;
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId)) return false;
    player->actor.world.pos = *next;
    HoldStill(player);
    player->actor.bgCheckFlags &= ~BGCHECK_ON_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    return true;
}

static f32 FindLandingFloor(PlayState* play, Player* player, Vec3f* pos) {
    Vec3f ray = *pos;
    CollisionPoly* poly = NULL;
    s32 bgId;
    ray.y += 150.0f;
    f32 floor = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &player->actor, &ray);
    return floor > BGCHECK_Y_MIN && fabsf(floor - pos->y) < 150.0f ? floor : pos->y;
}

static void ResolveComboHop(int32_t site, int32_t index, LinkAnimationHeader** anim) {
    PlayState* play = gPlayState;
    if (play == NULL || site != SOH_PLAYER_ANIM_SITE_HOP || index % 3 != 0 || !IsActive()) return;
    Player* player = GET_PLAYER(play);
    s32 dir = index / 3;
    if (sPendant.state != PENDANT_IDLE || IsPlayerBusy(player)) return;
    if ((dir != 1 && dir != 3) || !Player_IsZTargeting(player) || Player_GetMeleeWeaponHeld(player) == 0) {
        sPendant.hops = 0;
        sPendant.isLeapArmed = false;
        sComboLandingPending = false;
        return;
    }
    // This callback names an actual native launch, so consecutive hops cannot disappear between flag polls.
    sPendant.hops = sPendant.hops >= 3 || sPendant.hopTimer > PARRY_HOP_WINDOW ? 1 : sPendant.hops + 1;
    sPendant.hopTimer = 0;
    sPendant.isLeapArmed = false;
    sComboLandingPending = true;
    sComboWasAirborne = false;
    if (sPendant.hops != 2) return;
    LinkAnimationHeader* clip = ResourceMgr_LoadPlayerAnimAsHeader(dir == 3 ? sHopLeft : sHopRight);
    if (clip == NULL) return;
    *anim = clip;
    sSecondHopClip = clip;
    sSecondHopPending = true;
}

static f32 SideHopAirTicks(Player* player) {
    // Match the animation to the jump already launched by vanilla, using the engine's discrete gravity.
    f32 velocity = player->actor.velocity.y;
    f32 height = MAX(player->actor.world.pos.y - player->actor.floorHeight, 0.0f);
    f32 step = MAX(R_UPDATE_RATE * 0.5f, 0.1f);
    s32 ticks;
    for (ticks = 1; ticks < 60; ticks++) {
        velocity += player->actor.gravity;
        height += velocity * step;
        if (height <= 0.0f && velocity <= 0.0f) break;
    }
    return (f32)ticks;
}

static void ConfigureSecondHopAnimation(Player* player) {
    sSecondHopPending = false;
    // The native action already launched this hop and installed the replacement clip.
    // Adjust only its playback rate; never take the action, stop its speed or move its position.
    if (sSecondHopClip == NULL || player->skelAnime.animation != (void*)sSecondHopClip) return;
    f32 last = Animation_GetLastFrame(sSecondHopClip);
    f32 step = MAX(R_UPDATE_RATE * 0.5f, 0.1f);
    player->skelAnime.playSpeed = last / MAX(SideHopAirTicks(player) * step, 1.0f);
}

static void ExtendComboHopTravel(Actor* actor, float* scale) {
    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor || !IsActive()) return;
    Player* player = GET_PLAYER(gPlayState);
    if (sPendant.state != PENDANT_IDLE || IsPlayerBusy(player) || !sComboLandingPending ||
        !(player->stateFlags2 & PLAYER_STATE2_HOPPING) ||
        (IsGrounded(player) && actor->velocity.y <= 0.0f)) return;
    f32 multiplier = sPendant.hops == 2 ? 2.0f : sPendant.hops == 3 ? 1.5f : 1.0f;
    // The engine rebuilt velocity from the native hop speed immediately before Actor_UpdatePos.
    // Extend only XZ travel; keep gravity, airtime, animation and wall collision native.
    actor->velocity.x *= multiplier;
    actor->velocity.z *= multiplier;
}

static void TrackSideHops(Player* player) {
    if (!Player_IsZTargeting(player)) {
        sPendant.hops=0; sPendant.isLeapArmed=false; sComboLandingPending=false;
        return;
    }
    if (sPendant.isLeapArmed) {
        if (--sPendant.hopTimer <= 0) { sPendant.isLeapArmed=false; sPendant.hops=0; }
        return;
    }
    if (++sPendant.hopTimer > PARRY_HOP_WINDOW) {
        sPendant.hops=0; sComboLandingPending=false;
        return;
    }
    if (!sComboLandingPending) return;
    if (!IsGrounded(player)) sComboWasAirborne=true;
    else if (sComboWasAirborne && player->actor.velocity.y <= 0.0f) {
        sComboLandingPending=false;
        if (sPendant.hops==3 && IsParryTarget(player->focusActor)) {
            sPendant.isLeapArmed=true;
            sPendant.hopTimer=PARRY_TIMING_FRAMES;
            Sfx_PlaySfxCentered(NA_SE_VO_NAVY_ENEMY);
        }
    }
}

static void PlayRollLoop(Player* player, PlayState* play) {
    ChangeAnim(play, player, gPlayerAnim_link_normal_landing_roll, 3.0f, 0.0f,
               GetLastFrame(gPlayerAnim_link_normal_landing_roll), ANIMMODE_LOOP, -3.0f);
}

static void StartParryLeap(Player* player, PlayState* play) {
    Actor* target = player->focusActor;
    f32 dx=target->world.pos.x-player->actor.world.pos.x, dz=target->world.pos.z-player->actor.world.pos.z;
    f32 dist=MAX(sqrtf(dx*dx+dz*dz),1.0f);
    sPendant.state=PENDANT_PARRY_ARC; sPendant.timer=0; sPendant.target=target;
    sPendant.hasParryHit=false; sIsRolling=false;
    sPendant.isLeapArmed=false; sPendant.hops=0;
    sPendant.arcStart=player->actor.world.pos;
    sPendant.arcEnd=target->world.pos;
    sPendant.arcEnd.x += dx/dist*PARRY_LAND_DIST;
    sPendant.arcEnd.z += dz/dist*PARRY_LAND_DIST;
    sPendant.arcEnd.y = FindLandingFloor(play,player,&sPendant.arcEnd);
    sArcImpact=CLAMP(dist/(dist+PARRY_LAND_DIST),0.1f,0.9f);
    f32 top=sBoundsActor==target?sBoundsTop:MAX(target->focus.pos.y,target->world.pos.y+target->colChkInfo.cylHeight);
    f32 base=sPendant.arcStart.y+(sPendant.arcEnd.y-sPendant.arcStart.y)*sArcImpact;
    sArcHeight=MAX(35.0f,(top-base)/(4.0f*sArcImpact*(1.0f-sArcImpact)));
    sSavedGravity=player->actor.gravity;
    HoldStill(player);
    player->actor.bgCheckFlags &= ~BGCHECK_ON_GROUND;
    player->stateFlags3 |= PLAYER_STATE3_MIDAIR;
    player->actor.shape.rot.y=player->yaw=Actor_WorldYawTowardActor(&player->actor,target);
    ChangeAnim(play,player,gPlayerAnim_link_normal_run_jump,1.0f,0.0f,GetLastFrame(gPlayerAnim_link_normal_run_jump),ANIMMODE_ONCE,-3.0f);
    EnsureParryCollider(play,player);
    sParryCollider.base.atFlags &= ~AT_HIT;
    SilenceSwordQuads(player);
    PlaySfx(player,NA_SE_VO_LI_SWORD_N);
}

static void StrikeParryTarget(Player* player, PlayState* play) {
    SilenceSwordQuads(player);
    if (sPendant.hasParryHit) return;
    if (sParryCollider.base.atFlags & AT_HIT) {
        sPendant.hasParryHit=true;
        sParryCollider.base.atFlags &= ~(AT_HIT|AT_ON);
        PlaySfx(player,NA_SE_IT_SWORD_STRIKE);
        return;
    }
    Sphere16* sphere=&sParryElement.dim.worldSphere;
    sphere->center.x=player->actor.world.pos.x;
    sphere->center.y=player->actor.world.pos.y+18.0f;
    sphere->center.z=player->actor.world.pos.z;
    sphere->radius=36;
    sParryCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play,&play->colChkCtx,&sParryCollider.base);
}

static void EndParryArc(Player* player, PlayState* play) {
    sPendant.state=PENDANT_PARRY_LANDING; sPendant.timer=0;
    player->actor.gravity=sSavedGravity;
    sParryCollider.base.atFlags &= ~AT_ON;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    ChangeAnim(play,player,gPlayerAnim_link_normal_landing,1.5f,0.0f,GetLastFrame(gPlayerAnim_link_normal_landing),ANIMMODE_ONCE,-3.0f);
}

static void UpdateParryArc(Player* player, PlayState* play) {
    if (!IsActorAlive(play,sPendant.target) || !IsParryTarget(sPendant.target)) { EndParryArc(player,play); return; }
    f32 t=MIN((f32)++sPendant.timer/PARRY_ARC_FRAMES,1.0f);
    Vec3f next;
    next.x=sPendant.arcStart.x+(sPendant.arcEnd.x-sPendant.arcStart.x)*t;
    next.z=sPendant.arcStart.z+(sPendant.arcEnd.z-sPendant.arcStart.z)*t;
    next.y=sPendant.arcStart.y+(sPendant.arcEnd.y-sPendant.arcStart.y)*t+sArcHeight*4.0f*t*(1.0f-t);
    if (!MoveAlongArc(play,player,&next)) { EndParryArc(player,play); return; }
    if (!sIsRolling && t >= sArcImpact-0.1f) { PlayRollLoop(player,play); sIsRolling=true; }
    LinkAnimation_Update(play,&player->skelAnime);
    if (sIsRolling) StrikeParryTarget(player,play);
    SilenceSwordQuads(player);
    if (t>=1.0f) EndParryArc(player,play);
}

static void UpdateParryLanding(Player* player) {
    if (!IsGrounded(player) && ++sPendant.timer < GPOUND_TIMEOUT) return;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    ReturnToIdle();
}

static void ReadParryInput(Player* player, Input* input) {
    if (gPlayState==NULL || !IsActive() || IsPlayerBusy(player)) return;
    if (sPendant.isLeapArmed && sPendant.hopTimer>0 && sPendant.state==PENDANT_IDLE &&
        Player_IsZTargeting(player) && IsGrounded(player) && IsParryTarget(player->focusActor) &&
        Player_GetMeleeWeaponHeld(player)!=0 && (input->press.button&BTN_B)) {
        StartParryLeap(player,gPlayState);
        input->press.button &= ~BTN_B; input->cur.button &= ~BTN_B;
    } else if (sPendant.state==PENDANT_PARRY_ARC) {
        input->press.button &= ~(BTN_A|BTN_B); input->cur.button &= ~(BTN_A|BTN_B);
    }
}

static void DrawParryCue(PlayState* play, Player* player, int32_t limbIndex) {
    RemoteVisualState visual;
    bool visible = player == GET_PLAYER(play) ?
                       IsActive() && sPendant.isLeapArmed && sPendant.hopTimer > 0 && Player_GetMeleeWeaponHeld(player) != 0 :
                       ReadRemoteVisual(player, &visual);
    if (!visible || limbIndex != PLAYER_LIMB_L_HAND) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Push();
    // The same hand-space placement as the native charging glow, at a gentler alpha.
    Matrix_Translate(0.0f,LINK_IS_ADULT?300.0f:220.0f,LINK_IS_ADULT?-100.0f:0.0f,MTXMODE_APPLY);
    Matrix_Scale(LINK_IS_ADULT?-1.2f:-0.7f,LINK_IS_ADULT?-1.0f:-0.6f,LINK_IS_ADULT?-0.7f:-0.4f,MTXMODE_APPLY);
    Matrix_RotateX(1.570796327f,MTXMODE_APPLY);
    gDPSetPrimColor(POLY_XLU_DISP++,0,0x80,230,255,180,120);
    gDPSetEnvColor(POLY_XLU_DISP++,70,180,100,80);
    gSPSegment(POLY_XLU_DISP++,0x09,(uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx,0,play->gameplayFrames*5,0,32,32,1,0,play->gameplayFrames*10,8,8));
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)gSpinAttackChargingDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
