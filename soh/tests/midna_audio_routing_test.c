#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "soh/Enhancements/audio/MidnaAudio.h"

Vec3f gSfxDefaultPos;
f32 gSfxDefaultFreqAndVolScale = 1;
s8 gSfxDefaultReverb;
static u16 sCUpInvisible, sCUpTimer;
static int nativeCalls, customCalls, clipPresent, disableCalls;
static int midnaEnabled = 1;
static u16 nativeId;
static MidnaAudioEvent customEvent;
static int idleUpdates, idleEligible, playerCutscene;

void MidnaAudio_UpdateIdle(bool eligible) {
    ++idleUpdates;
    idleEligible = eligible;
}
s32 Play_InCsMode(PlayState* play) {
    return play->csCtx.state != CS_STATE_IDLE || playerCutscene;
}
u8 Message_GetState(MessageContext* ctx) {
    return ctx->msgMode == MSGMODE_NONE ? TEXT_STATE_NONE : TEXT_STATE_EVENT;
}

int32_t CVarGetInteger(const char* name, int32_t fallback) {
    if (strcmp(name, CVAR_ENHANCEMENT("MidnaCompanion")) == 0) {
        REQUIRE(fallback == 0);
        return midnaEnabled;
    }
    REQUIRE(strcmp(name, CVAR_AUDIO("DisableNaviCallAudio")) == 0);
    return disableCalls;
}
bool MidnaAudio_TryPlay(MidnaAudioEvent event) {
    ++customCalls;
    customEvent = event;
    return clipPresent;
}
void Audio_PlaySoundGeneral(u16 id, Vec3f* pos, u8 token, f32* freq, f32* vol, s8* reverb) {
    REQUIRE(pos == &gSfxDefaultPos && token == 4);
    REQUIRE(freq == &gSfxDefaultFreqAndVolScale && vol == freq && reverb == &gSfxDefaultReverb);
    ++nativeCalls;
    nativeId = id;
}
void func_800F4524(Vec3f* pos, u16 id, s8 duration) {
    REQUIRE(pos == &gSfxDefaultPos && duration == 32);
    ++nativeCalls;
    nativeId = id;
}
void Audio_PlayActorSound2(Actor* actor, u16 id) {
    ++nativeCalls;
    nativeId = id;
}

/* PRODUCTION_MIDNA_AUDIO_ROUTING */

static void idleGates(void) {
    static PlayState play;
    Player player = { 0 };
    EnElf fairy = { 0 };
    Actor npc = { 0 };
    npc.category = ACTORCAT_NPC;
    play.actorCtx.actorLists[ACTORCAT_PLAYER].head = &player.actor;
    player.actor.bgCheckFlags = 1;
    fairy.actor.params = FAIRY_NAVI;
    fairy.actor.scale.x = .008f;
    fairy.innerColor.a = 255;
    EnElf_UpdateMidnaIdleAudio(&fairy, &play);
    REQUIRE(idleUpdates == 1 && idleEligible);
    play.actorCtx.targetCtx.arrowPointedActor = &npc;
    EnElf_UpdateMidnaIdleAudio(&fairy, &play);
    REQUIRE(idleEligible); // passive NPC proximity keeps Navi out in native mode 0
    player.focusActor = &npc;
    EnElf_UpdateMidnaIdleAudio(&fairy, &play);
    REQUIRE(!idleEligible); // actively locking onto that same NPC pauses idle time
    player.focusActor = NULL;
    play.actorCtx.targetCtx.arrowPointedActor = NULL;

#define BLOCKED(field, value)                      \
    do {                                           \
        __typeof__(field) saved = field;           \
        field = value;                             \
        EnElf_UpdateMidnaIdleAudio(&fairy, &play); \
        REQUIRE(!idleEligible);                    \
        field = saved;                             \
        EnElf_UpdateMidnaIdleAudio(&fairy, &play); \
        REQUIRE(idleEligible);                     \
    } while (0)
    BLOCKED(midnaEnabled, 0);
    BLOCKED(player.actor.speedXZ, 2.0f);
    BLOCKED(player.actor.speedXZ, -2.0f);
    BLOCKED(player.linearVelocity, 2.0f);
    BLOCKED(player.actor.bgCheckFlags, 0);
    BLOCKED(player.stateFlags1, PLAYER_STATE1_ON_HORSE);
    BLOCKED(player.stateFlags1, PLAYER_STATE1_GETTING_ITEM);
    BLOCKED(player.stateFlags1, PLAYER_STATE1_PARALLEL);
    BLOCKED(player.stateFlags1, PLAYER_STATE1_FIRST_PERSON);
    BLOCKED(player.focusActor, &fairy.actor);
    BLOCKED(fairy.unk_2A8, 1);  // native enemy/non-NPC attention
    BLOCKED(fairy.unk_2A8, 7);  // recall
    BLOCKED(fairy.unk_2A8, 8);  // hidden
    BLOCKED(fairy.unk_2A8, 11); // emergence
    BLOCKED(fairy.fairyFlags, 8);
    BLOCKED(fairy.actor.scale.x, .004f);
    BLOCKED(fairy.innerColor.a, 0);
    BLOCKED(fairy.unk_2C7, 1);
    BLOCKED(play.pauseCtx.state, 6);
    BLOCKED(play.msgCtx.msgMode, MSGMODE_TEXT_DISPLAYING);
    BLOCKED(play.csCtx.state, CS_STATE_SKIPPABLE_EXEC);
    BLOCKED(playerCutscene, 1);
    BLOCKED(play.transitionTrigger, TRANS_TRIGGER_START);
    BLOCKED(play.transitionMode, TRANS_MODE_INSTANT);
    BLOCKED(play.gameOverCtx.state, GAMEOVER_DEATH_START);
#undef BLOCKED
    int before = idleUpdates;
    fairy.actor.params = FAIRY_KOKIRI;
    EnElf_UpdateMidnaIdleAudio(&fairy, &play);
    REQUIRE(idleUpdates == before); // ordinary fairies cannot tick/cancel Navi's idle state
}

int main(void) {
    static PlayState play;
    const u16 calls[] = { 0x1E, 0x1D };
    const u16 native[] = { NA_SE_VO_NAVY_CALL, NA_SE_VO_NA_HELLO_2 };
    const MidnaAudioEvent events[] = { MIDNA_AUDIO_CALL, MIDNA_AUDIO_HINT };
    for (int i = 0; i < 2; ++i) {
        for (clipPresent = 0; clipPresent <= 1; ++clipPresent) {
            memset(&play, 0, sizeof(play));
            nativeCalls = customCalls = disableCalls = 0;
            sCUpInvisible = 1;
            Interface_SetNaviCall(&play, calls[i]);
            REQUIRE(customCalls == 1 && customEvent == events[i]);
            REQUIRE(nativeCalls == !clipPresent);
            if (!clipPresent)
                REQUIRE(nativeId == native[i]);
            REQUIRE(play.interfaceCtx.naviCalling && sCUpInvisible == 0 && sCUpTimer == 10);
            Interface_SetNaviCall(&play, calls[i]);
            REQUIRE(customCalls == 1); // repeated HUD polling must not retrigger
            Interface_SetNaviCall(&play, 0x1F);
            REQUIRE(!play.interfaceCtx.naviCalling);
        }
    }
    memset(&play, 0, sizeof(play));
    nativeCalls = customCalls = 0;
    disableCalls = 1;
    Interface_SetNaviCall(&play, 0x1E);
    REQUIRE(customCalls == 0 && nativeCalls == 0 && play.interfaceCtx.naviCalling);
    disableCalls = 0;
    memset(&play, 0, sizeof(play));
    play.csCtx.state = CS_STATE_SKIPPABLE_EXEC;
    Interface_SetNaviCall(&play, 0x1D);
    REQUIRE(customCalls == 0 && nativeCalls == 0 && !play.interfaceCtx.naviCalling);

    /* ACTOR_ROUTING_CHECKS */
    idleGates();
    puts("PASS: Navi actor/HUD routing, native arguments, call mute and cutscene gates");
}
