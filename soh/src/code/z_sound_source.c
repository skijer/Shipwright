#include "global.h"

static f32 SoundSource_DotVec3f(const Vec3f* a, const Vec3f* b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

static void SoundSource_WorldToAudioSpace(const Camera* camera, const Vec3f* worldPos, Vec3f* outAudioPos) {
    Vec3f forward;
    Vec3f right;
    Vec3f up;
    Vec3f delta;
    f32 mag;

    if ((camera == NULL) || (worldPos == NULL) || (outAudioPos == NULL)) {
        if (outAudioPos != NULL) {
            outAudioPos->x = 0.0f;
            outAudioPos->y = 0.0f;
            outAudioPos->z = 0.0f;
        }
        return;
    }

    forward.x = camera->at.x - camera->eye.x;
    forward.y = camera->at.y - camera->eye.y;
    forward.z = camera->at.z - camera->eye.z;

    mag = sqrtf(SQ(forward.x) + SQ(forward.y) + SQ(forward.z));
    if (mag < 0.001f) {
        forward.x = 0.0f;
        forward.y = 0.0f;
        forward.z = 1.0f;
        mag = 1.0f;
    }

    forward.x /= mag;
    forward.y /= mag;
    forward.z /= mag;

    right.x = (forward.y * camera->up.z) - (forward.z * camera->up.y);
    right.y = (forward.z * camera->up.x) - (forward.x * camera->up.z);
    right.z = (forward.x * camera->up.y) - (forward.y * camera->up.x);

    mag = sqrtf(SQ(right.x) + SQ(right.y) + SQ(right.z));
    if (mag < 0.001f) {
        Vec3f fallbackUp = { 0.0f, 1.0f, 0.0f };

        if (fabsf(forward.y) > 0.99f) {
            fallbackUp.x = 1.0f;
            fallbackUp.y = 0.0f;
            fallbackUp.z = 0.0f;
        }

        right.x = (forward.y * fallbackUp.z) - (forward.z * fallbackUp.y);
        right.y = (forward.z * fallbackUp.x) - (forward.x * fallbackUp.z);
        right.z = (forward.x * fallbackUp.y) - (forward.y * fallbackUp.x);
        mag = sqrtf(SQ(right.x) + SQ(right.y) + SQ(right.z));
    }

    if (mag < 0.001f) {
        right.x = 1.0f;
        right.y = 0.0f;
        right.z = 0.0f;
        mag = 1.0f;
    }

    right.x /= mag;
    right.y /= mag;
    right.z /= mag;

    up.x = (right.y * forward.z) - (right.z * forward.y);
    up.y = (right.z * forward.x) - (right.x * forward.z);
    up.z = (right.x * forward.y) - (right.y * forward.x);

    mag = sqrtf(SQ(up.x) + SQ(up.y) + SQ(up.z));
    if (mag < 0.001f) {
        up.x = camera->up.x;
        up.y = camera->up.y;
        up.z = camera->up.z;
        mag = sqrtf(SQ(up.x) + SQ(up.y) + SQ(up.z));
    }

    if (mag < 0.001f) {
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        mag = 1.0f;
    }

    up.x /= mag;
    up.y /= mag;
    up.z /= mag;

    delta.x = worldPos->x - camera->eye.x;
    delta.y = worldPos->y - camera->eye.y;
    delta.z = worldPos->z - camera->eye.z;

    outAudioPos->x = SoundSource_DotVec3f(&delta, &right);
    outAudioPos->y = SoundSource_DotVec3f(&delta, &up);
    outAudioPos->z = SoundSource_DotVec3f(&delta, &forward);
}

static Player* SoundSource_FindClosestActivePlayer(PlayState* play, const Vec3f* worldPos) {
    Actor* playerActor;
    Player* closestPlayer = GET_PLAYER(play);
    f32 bestDistSq = FLT_MAX;
    s32 sanity = 0;

    if ((play == NULL) || (worldPos == NULL)) {
        return closestPlayer;
    }

    playerActor = play->actorCtx.actorLists[ACTORCAT_PLAYER].head;

    while ((playerActor != NULL) && (sanity < 2000)) {
        if ((playerActor->id == ACTOR_PLAYER) && (playerActor->update != NULL)) {
            Player* player = (Player*)playerActor;
            f32 dx = worldPos->x - player->actor.world.pos.x;
            f32 dy = worldPos->y - player->actor.world.pos.y;
            f32 dz = worldPos->z - player->actor.world.pos.z;
            f32 distSq = SQ(dx) + SQ(dy) + SQ(dz);

            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                closestPlayer = player;
            }
        }

        playerActor = playerActor->next;
        sanity++;
    }

    return closestPlayer;
}

static Camera* SoundSource_FindCameraForPlayer(PlayState* play, Player* player) {
    s32 i;

    if ((play == NULL) || (player == NULL)) {
        return NULL;
    }

    for (i = 0; i < NUM_CAMS; i++) {
        Camera* camera = play->cameraPtrs[i];

        if ((camera != NULL) && (camera->player == player) && (camera->status == CAM_STAT_ACTIVE)) {
            return camera;
        }
    }

    for (i = 0; i < NUM_CAMS; i++) {
        Camera* camera = play->cameraPtrs[i];

        if ((camera != NULL) && (camera->player == player)) {
            return camera;
        }
    }

    return play->cameraPtrs[MAIN_CAM];
}

static void SoundSource_UpdateProjectedPos(PlayState* play, SoundSource* source) {
    Camera* camera;
    Player* closestPlayer;

    if ((play == NULL) || (source == NULL)) {
        return;
    }

    closestPlayer = SoundSource_FindClosestActivePlayer(play, &source->worldPos);
    camera = SoundSource_FindCameraForPlayer(play, closestPlayer);
    SoundSource_WorldToAudioSpace(camera, &source->worldPos, &source->projectedPos);
}

void SoundSource_InitAll(PlayState* play) {
    SoundSource* sources = &play->soundSources[0];
    s32 i;

    // clang-format off
    for (i = 0; i < ARRAY_COUNT(play->soundSources); i++) { sources[i].countdown = 0; }
    // clang-format on
}

void SoundSource_UpdateAll(PlayState* play) {
    SoundSource* source = &play->soundSources[0];
    s32 i;

    for (i = 0; i < ARRAY_COUNT(play->soundSources); i++) {
        if (source->countdown != 0) {
            if (DECR(source->countdown) == 0) {
                Audio_StopSfxByPos(&source->projectedPos);
            } else {
                SoundSource_UpdateProjectedPos(play, source);
            }
        }

        source++;
    }
}

void SoundSource_PlaySfxAtFixedWorldPos(PlayState* play, Vec3f* worldPos, s32 duration, u16 sfxId) {
    s32 countdown;
    SoundSource* source;
    s32 smallestCountdown = 0xFFFF;
    SoundSource* backupSource;
    s32 i;

    source = &play->soundSources[0];
    for (i = 0; i < ARRAY_COUNT(play->soundSources); i++) {
        if (source->countdown == 0) {
            break;
        }

        // Store the sound source with the smallest remaining countdown
        countdown = source->countdown;
        if (countdown < smallestCountdown) {
            smallestCountdown = countdown;
            backupSource = source;
        }
        source++;
    }

    // If no sound source is available, replace the sound source with the smallest remaining countdown
    if (i >= ARRAY_COUNT(play->soundSources)) {
        source = backupSource;
        Audio_StopSfxByPos(&source->projectedPos);
    }

    source->worldPos = *worldPos;
    source->countdown = duration;

    SoundSource_UpdateProjectedPos(play, source);
    Audio_PlaySoundGeneral(sfxId, &source->projectedPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultReverb);
}
