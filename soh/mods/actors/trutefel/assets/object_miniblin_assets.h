#ifndef TRUTEFEL_OBJECT_MINIBLIN_ASSETS_H
#define TRUTEFEL_OBJECT_MINIBLIN_ASSETS_H

#define GMINIBLINSKEL_BONE_POS_LIMB 0
#define GMINIBLINSKEL_BONE_ROT_LIMB 1
#define GMINIBLINSKEL_BODY_LIMB 2
#define GMINIBLINSKEL_HEAD_LIMB 3
#define GMINIBLINSKEL_LEFTEAR1_LIMB 4
#define GMINIBLINSKEL_LEFTEAR2_LIMB 5
#define GMINIBLINSKEL_RIGHTEAR1_LIMB 6
#define GMINIBLINSKEL_RIGHTEAR2_LIMB 7
#define GMINIBLINSKEL_HIP_L_LIMB 8
#define GMINIBLINSKEL_UPPERLEG_L_LIMB 9
#define GMINIBLINSKEL_LEG_L_LIMB 10
#define GMINIBLINSKEL_FOOT_L_LIMB 11
#define GMINIBLINSKEL_HIP_R_LIMB 12
#define GMINIBLINSKEL_UPPERLEG_R_LIMB 13
#define GMINIBLINSKEL_LEG_R_LIMB 14
#define GMINIBLINSKEL_FOOT_R_LIMB 15
#define GMINIBLINSKEL_HIPBACK_LIMB 16
#define GMINIBLINSKEL_TAIL1_LIMB 17
#define GMINIBLINSKEL_TAILEND_LIMB 18
#define GMINIBLINSKEL_MOUTH_LIMB 19
#define GMINIBLINSKEL_SHOULDER_L_LIMB 20
#define GMINIBLINSKEL_UPPERARM_L_LIMB 21
#define GMINIBLINSKEL_ARM_L_LIMB 22
#define GMINIBLINSKEL_HAND_L_LIMB 23
#define GMINIBLINSKEL_SHOULDER_R_LIMB 24
#define GMINIBLINSKEL_UPPERARM_R_LIMB 25
#define GMINIBLINSKEL_ARM_R_LIMB 26
#define GMINIBLINSKEL_HAND_R_LIMB 27
#define GMINIBLINSKEL_NUM_LIMBS 28

extern FlexSkeletonHeader gMiniblinSkel;
extern AnimationHeader gMiniblinSkelBombthrowAnim;
extern AnimationHeader gMiniblinSkelDamageAnim;
extern AnimationHeader gMiniblinSkelDeathAnim;
extern AnimationHeader gMiniblinSkelIdleAnim;
extern AnimationHeader gMiniblinSkelJumpAnim;
extern AnimationHeader gMiniblinSkelLaughAnim;
extern AnimationHeader gMiniblinSkelTailattackAnim;
extern const char gMiniblinSkel_eye_normal_rgba16[];
extern const char gMiniblinSkel_eye_halfclosed_rgba16[];
extern const char gMiniblinSkel_eye_closed_rgba16[];
extern const char gMiniblinSkel_eye_laugh_rgba16[];
extern const char gMiniblinSkel_eye_hit_rgba16[];

#endif