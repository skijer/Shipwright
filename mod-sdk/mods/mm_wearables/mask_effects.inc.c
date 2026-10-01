// Effects implemented through existing actor and player callbacks.
#include "overlays/actors/ovl_En_Rd/z_en_rd.h"
#include "overlays/actors/ovl_En_Gs/z_en_gs.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "objects/object_rd/object_rd.h"
extern void func_80AE269C(EnRd* redead);
bool GameInteractor_Should(GIVanillaBehavior flag, uint32_t result, ...);

static void FriendlyRedeadDance(EnRd* rd, PlayState* play) {
    if (!IsWearing(MASK_GIBDO)) {
        rd->collider.base.acFlags |= AC_ON;
        func_80AE269C(rd);
        return;
    }
    SkelAnime_Update(&rd->skelAnime);
    if (!sApi->HasResource("__OTR__objects/object_rd/gGibdoRedeadClappingDanceAnim"))
        rd->actor.shape.rot.y = rd->actor.home.rot.y + (int16_t)(0x1000 * Math_SinS(play->gameplayFrames * 0x800));
    Math_SmoothStepToS(&rd->unk_30E, 0, 1, 0x64, 0);
    Math_SmoothStepToS(&rd->unk_310, 0, 1, 0x64, 0);
}

static void MakeRedeadDance(void* actorRef) {
    EnRd* rd = actorRef;
    static const char* paths[] = {
        "__OTR__objects/object_rd/gGibdoRedeadSquattingDanceAnim",
        "__OTR__objects/object_rd/gGibdoRedeadClappingDanceAnim",
        "__OTR__objects/object_rd/gGibdoRedeadPirouetteAnim",
    };
    if (!IsWearing(MASK_GIBDO) || rd->actionFunc == FriendlyRedeadDance ||
        rd->actor.colChkInfo.health <= 0 || rd->unk_31B == 8 ||
        (GET_PLAYER(gPlayState)->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY)) return;
    const char* path = paths[((uintptr_t)rd >> 4) % 3];
    AnimationHeader* anim = sApi->HasResource(path) ? (AnimationHeader*)ResourceMgr_LoadAnimByName(path) :
                                                    (AnimationHeader*)&gGibdoRedeadIdleAnim;
    Animation_Change(&rd->skelAnime, anim, 1.0f, 0, Animation_GetLastFrame(anim), ANIMMODE_LOOP, -8.0f);
    rd->collider.base.acFlags &= ~(AC_ON | AC_HIT);
    rd->actor.speedXZ = 0;
    rd->unk_31B = 0;
    rd->actionFunc = FriendlyRedeadDance;
}

static Actor* sTruthLastStone;
static bool SpawnTruthFairy(EnGs* stone, PlayState* play, bool big) {
    // Reuse the existing shuffle-fairies behavior, including its location identity and duplicate check.
    int16_t oldSong = play->msgCtx.unk_E3F2;
    int16_t oldMode = play->msgCtx.ocarinaMode;
    play->msgCtx.ocarinaMode = OCARINA_MODE_04;
    play->msgCtx.unk_E3F2 = big ? OCARINA_SONG_STORMS : OCARINA_SONG_SARIAS;
    bool vanilla = GameInteractor_Should(VB_SPAWN_GOSSIP_STONE_FAIRY, true, stone);
    play->msgCtx.ocarinaMode = oldMode;
    play->msgCtx.unk_E3F2 = oldSong;
    if (!vanilla) return true;
    return Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ELF, stone->actor.world.pos.x + (big ? 12 : -12),
        stone->actor.world.pos.y + (big ? 50 : 40), stone->actor.world.pos.z, 0, 0, 0,
        big ? FAIRY_HEAL_BIG : FAIRY_HEAL_TIMED) != NULL;
}
static void TruthStoneFairies(void* actorRef) {
    EnGs* stone = actorRef;
    PlayState* play = gPlayState;
    if (play == NULL) return;
    if (stone->unk_19C != 2 || GET_PLAYER(play)->talkActor != &stone->actor) {
        if (sTruthLastStone == &stone->actor) sTruthLastStone = NULL;
        return;
    }
    if (!IsWearing(MASK_TRUTH) || sTruthLastStone == &stone->actor ||
        (!IS_RANDO && Flags_GetSwitch(play, (stone->actor.params >> 8) & 0x3F))) return;
    bool small = SpawnTruthFairy(stone, play, false);
    bool big = SpawnTruthFairy(stone, play, true);
    if (small || big) {
        sTruthLastStone = &stone->actor;
        Flags_SetSwitch(play, (stone->actor.params >> 8) & 0x3F);
        Audio_PlayActorSound2(&stone->actor, NA_SE_EV_BUTTERFRY_TO_FAIRY);
    }
}

static void RegisterMaskEffects(void) {
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_RD, MakeRedeadDance);
    SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_GS, TruthStoneFairies);
}
