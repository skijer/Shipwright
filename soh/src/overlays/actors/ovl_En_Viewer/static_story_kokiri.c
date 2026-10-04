#include "static_story_kokiri.h"

#include <math.h>

#include "z_en_viewer.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "objects/object_fa/object_fa.h"
#include "objects/object_kw1/object_kw1.h"
#include "objects/object_os_anime/object_os_anime.h"
#include "soh/ResourceManagerHelpers.h"
#include "static_story_actor.h"

s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);

static void* sGirlEyes[] = { gKw1EyeOpenTex, gKw1EyeHalfTex, gKw1EyeClosedTex };
static void* sFadoEyes[] = { gFaEyeOpenTex, gFaEyeHalfTex, gFaEyeClosedTex };

static AnimationHeader* StaticStoryKokiri_GetAnimation(uint16_t animation) {
    switch (animation) {
        case STATIC_ANIM_KOKIRI_IDLE:
            return (AnimationHeader*)gKokiriIdleAnim;
        case STATIC_ANIM_KOKIRI_ARMS_BEHIND:
            return (AnimationHeader*)gKokiriStandingArmsBehindBackAnim;
        case STATIC_ANIM_KOKIRI_HANDS_HIPS:
            return (AnimationHeader*)gKokiriStandingHandsOnHipsAnim;
        case STATIC_ANIM_KOKIRI_SITTING_HEAD_HAND:
            return (AnimationHeader*)gKokiriSittingHeadOnHandAnim;
        case STATIC_ANIM_KOKIRI_SITTING_CROSSED_LEGS:
            return (AnimationHeader*)gKokiriSittingCrossedLegsAnim;
        case STATIC_ANIM_KOKIRI_SITTING_CROSSED_ARMS_LEGS:
            return (AnimationHeader*)gKokiriSittingCrossedArmsLegsAnim;
        default:
            return NULL;
    }
}

int StaticStoryKokiri_RequestObjects(EnViewer* this, PlayState* play) {
    int16_t ids[4] = { this->staticState.type == STATIC_STORY_ACTOR_FADO ? OBJECT_FA : OBJECT_KW1, OBJECT_KW1,
                       OBJECT_KW1, OBJECT_OS_ANIME };

    for (int slot = 0; slot < 4; ++slot) {
        this->staticState.objectSlots[slot] = Object_GetIndex(&play->objectCtx, ids[slot]);
        if (this->staticState.objectSlots[slot] < 0) {
            this->staticState.objectSlots[slot] = Object_Spawn(&play->objectCtx, ids[slot]);
        }
        if (this->staticState.objectSlots[slot] < 0) {
            return false;
        }
    }
    return true;
}

void StaticStoryKokiri_Init(EnViewer* this, PlayState* play) {
    const StaticStoryPoseDescriptor* pose =
        StaticStoryActor_ResolvePose((StaticStoryActorType)this->staticState.type, this->staticState.pose);

    gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[3]].segment);
    SkelAnime_InitFlex(play, &this->skin.skelAnime, (FlexSkeletonHeader*)gKw1Skel, NULL, NULL, NULL, 0);
    Animation_PlayLoopSetSpeed(&this->skin.skelAnime, StaticStoryKokiri_GetAnimation(pose->animation),
                               pose->playbackSpeed);
    this->staticState.kokiriLegFrame = 0.0f;
    StaticStoryKokiri_UpdatePose(this, 0.0f);
}

void StaticStoryKokiri_UpdatePose(EnViewer* this, f32 step) {
    Vec3s* joints = this->skin.skelAnime.jointTable;

    if ((this->staticState.type != STATIC_STORY_ACTOR_KOKIRI_GIRL &&
         this->staticState.type != STATIC_STORY_ACTOR_FADO) ||
        this->staticState.pose != 4 || joints == NULL || this->skin.skelAnime.limbCount < 16) {
        return;
    }

    /* In this native seated pose the root X/Z rotations are -90 degrees.
     * Root Y, torso Z and head Z therefore compose the forward head pitch.
     * Cancel that pitch without changing the seated torso, arms or placement. */
    joints[15].z = (s16)(-0x8000 - (s32)joints[1].y - joints[8].z);

    AnimationHeader* animation = (AnimationHeader*)ResourceMgr_LoadAnimByName(gGreatFairySittingAnim);
    if (animation == NULL || animation->common.frameCount <= 0 || animation->frameData == NULL ||
        animation->jointIndices == NULL) {
        return;
    }

    /* The two skeletons share leg joint order and axes. Transfer rotations only:
     * keep Kokiri proportions and leave the fairy's root, torso and hair out. */
    f32 frameCount = animation->common.frameCount;
    this->staticState.kokiriLegFrame = fmodf(this->staticState.kokiriLegFrame + step, frameCount);
    if (this->staticState.kokiriLegFrame < 0.0f) {
        this->staticState.kokiriLegFrame += frameCount;
    }
    s32 frame = (s32)this->staticState.kokiriLegFrame;
    s32 nextFrame = (frame + 1) % animation->common.frameCount;
    Vec3s current[8];
    Vec3s next[8];
    SkelAnime_GetFrameData(animation, frame, 8, current);
    SkelAnime_GetFrameData(animation, nextFrame, 8, next);
    SkelAnime_InterpFrameTable(8, current, current, next, this->staticState.kokiriLegFrame - frame);
    for (s32 limb = 2; limb <= 7; ++limb) {
        joints[limb] = current[limb];
    }
}

static s32 StaticStoryKokiri_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                              void* thisx, Gfx** gfx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 15) {
        gSPSegment((*gfx)++, 0x06, play->objectCtx.status[this->staticState.objectSlots[0]].segment);
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[0]].segment);
        *dList = this->staticState.type == STATIC_STORY_ACTOR_FADO ? (Gfx*)gFaDL : (Gfx*)object_kw1_DL_002C10;
        gSPSegment((*gfx)++, 0x0A,
                   SEGMENTED_TO_VIRTUAL(this->staticState.type == STATIC_STORY_ACTOR_FADO
                                            ? sFadoEyes[this->staticState.eyeIndex]
                                            : sGirlEyes[this->staticState.eyeIndex]));
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[2]].segment);
    }
    if (StaticStoryActor_CanTrack((StaticStoryActorType)this->staticState.type, this->staticState.pose)) {
        /* Native En_Ko torso/head matrix convention. */
        if (limbIndex == 8) {
            Matrix_RotateX(BINANG_TO_RAD(-this->staticState.interactInfo.torsoRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(this->staticState.interactInfo.torsoRot.x), MTXMODE_APPLY);
        } else if (limbIndex == 15) {
            Matrix_Translate(1200.0f, 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(this->staticState.interactInfo.headRot.y), MTXMODE_APPLY);
            Matrix_RotateZ(BINANG_TO_RAD(this->staticState.interactInfo.headRot.x), MTXMODE_APPLY);
            Matrix_Translate(-1200.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        }
    }
    return false;
}

static void StaticStoryKokiri_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx,
                                           Gfx** gfx) {
    EnViewer* this = (EnViewer*)thisx;

    if (limbIndex == 7) {
        gSPSegment((*gfx)++, 0x06, play->objectCtx.status[this->staticState.objectSlots[1]].segment);
        gSegments[6] = VIRTUAL_TO_PHYSICAL(play->objectCtx.status[this->staticState.objectSlots[1]].segment);
    }
}

static Gfx* StaticStoryKokiri_ColorDL(GraphicsContext* gfxCtx, u8 r, u8 g, u8 b) {
    Gfx* displayList = Graph_Alloc(gfxCtx, sizeof(Gfx) * 2);

    gDPSetEnvColor(displayList, r, g, b, 255);
    gSPEndDisplayList(displayList + 1);
    return displayList;
}

void StaticStoryKokiri_Draw(EnViewer* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, StaticStoryKokiri_ColorDL(play->state.gfxCtx, 70, 190, 60));
    gSPSegment(POLY_OPA_DISP++, 0x09, StaticStoryKokiri_ColorDL(play->state.gfxCtx, 100, 30, 0));
    POLY_OPA_DISP = SkelAnime_DrawFlex(play, this->skin.skelAnime.skeleton, this->skin.skelAnime.jointTable,
                                       this->skin.skelAnime.dListCount, StaticStoryKokiri_OverrideLimbDraw,
                                       StaticStoryKokiri_PostLimbDraw, this, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}
