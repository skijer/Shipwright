/**
 * mario_mask_scene.cpp — "I must save the princess..." (Peach's Castle custom scene)
 *
 * A self-contained set piece for the custom Peach's Castle scene shipped in
 * custom-scenes.o2r (it overrides SCENE_TESTROOM). Nothing here edits a vanilla
 * file: the whole thing self-registers through RegisterShipInitFunc, and
 * soh/CMakeLists.txt globs mods/ *.cpp automatically.
 *
 * The beat, in MM's Song of Healing shape:
 *   1. Talk to the Gossip Stone under the Mario painting -> "I must save the
 *      princess..." (the vanilla stone keeps its model and its hint behaviour;
 *      only the text is intercepted, so nothing is hijacked).
 *   2. Play ANY ocarina song while standing near it. Link is held in place, the
 *      camera pulls onto him, the healing chime plays and the screen-flash beat
 *      runs for kCutsceneFrames.
 *   3. Mario Mask is granted, and the Mario painting on the wall above goes
 *      black — the "soul" left the portrait.
 *
 * Ownership persists through RAND_INF_OBTAINED_MARIO_MASK (the Jabber-Nut style
 * flag the user asked for), so it survives a save/load and is queryable by the
 * randomizer later.
 *
 * Scene geometry this depends on (see custom-scenes.o2r):
 *   gossip stone  ACTOR_EN_GS      at (-2054, -1834, -171)
 *   music staff   ACTOR_EN_OKARINA_TAG at (-1859, -1832, -181)  [unused: we
 *                 detect songs ourselves so ANY song works, not just its own]
 *   painting      'cuboid1' slab, front face X=-2027 facing +X,
 *                 Y[-1797..-1545] Z[-308..-54] -- directly above the stone.
 */

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"
#include "soh/SaveManager.h"
#include "soh/ResourceManagerHelpers.h"
// Rando::StaticData::RetrieveItem -> the GetItemEntry that GiveItemEntryWithoutActor
// needs in order to play the get-item animation.
#include "soh/Enhancements/randomizer/static_data.h"

// Fast::Texture -> the painting's real byte size and pixel format, so BlackenPainting
// does not have to assume the dimensions the archive happens to ship with.
#include <fast/resource/type/Texture.h>

#include <cstddef>
#include <cstring>
#include <cmath>
#include <memory>

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
// Host actor for the talk-target hijack (see SpawnTalkTarget).
#include "overlays/actors/ovl_En_Lightbox/z_en_lightbox.h"
extern PlayState* gPlayState;
// Defined extern "C" in OTRGlobals.cpp, but OTRGlobals.h only declares it inside an
// `#ifndef __cplusplus` block, so a .cpp cannot see it through the header. Declared
// here directly, the same way broken_items.c reaches ResourceMgr_FileExists.
void Gfx_TextureCacheDelete(const uint8_t* addr);
}

namespace {

// ---------------------------------------------------------------------------
// Scene constants
// ---------------------------------------------------------------------------

constexpr int16_t kSceneId = SCENE_TESTROOM; // custom-scenes.o2r overrides this slot

// The gossip stone, from the room's actor list.
constexpr float kStoneX = -2054.0f;
constexpr float kStoneY = -1834.0f;
constexpr float kStoneZ = -171.0f;

// How close the player must be for the stone to "hear" the ocarina. The painting
// alcove is ~250 units across, so this comfortably covers standing in front of it
// without reaching into the next room.
constexpr float kSongRangeSq = 400.0f * 400.0f;

constexpr const char* kMessageTableId = "MarioMaskScene";
constexpr const char* kSaveSection = "marioMaskScene";

// The painting texture inside custom-scenes.o2r. This doubles as the marker that
// tells us the custom scene archive is actually installed -- see SceneArchiveLoaded().
constexpr const char* kPaintingTexPath = "custom/prelude/testroom_scene/cuboid1_tex0";

// How long Link is held looking around after the song, before dialogue 2 opens.
// Long enough to read as a deliberate beat, short enough not to feel like a hang.
constexpr int kLookFrames = 70;

// Text slots we open ourselves. Well clear of the vanilla message range.
constexpr uint16_t kStoneTextId = 0x8F00; // dialogue 1, checking the stone
constexpr uint16_t kSongTextId = 0x8F01;  // dialogue 2, after the song

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

enum Phase {
    PHASE_IDLE,     // nothing happening
    PHASE_LOOKING,  // song accepted: Link held still, camera on him, looking around
    PHASE_SPEAKING, // dialogue 2 is up, waiting for the player to dismiss it
    PHASE_DONE,     // mask granted this session
};

struct State {
    Phase phase = PHASE_IDLE;
    int timer = 0;
    bool paintingBlackened = false;
    bool talkedOnce = false; // purely cosmetic: lets the stone change its line afterwards
    bool textOpened = false; // dialogue 2 has actually been seen on screen -- see PHASE_SPEAKING
};

State gState;

// SCENE_TESTROOM is a VANILLA OoT test room -- custom-scenes.o2r merely overrides
// its resources. So the scene id alone is not enough to identify Peach's Castle:
// without the archive in mods/ you would walk into the stock test room and find a
// Gossip Stone talking about saving the princess, with any song handing out the
// Mario Mask. Probe for a resource that only the custom archive provides.
//
// Cached because it is consulted from per-frame paths; re-probed on every scene
// entry so dropping the archive in and warping is enough to pick it up.
int8_t sArchiveState = -1; // -1 unknown, 0 absent, 1 present

bool SceneArchiveLoaded() {
    if (sArchiveState < 0) {
        sArchiveState = ResourceMgr_FileExists(kPaintingTexPath) ? 1 : 0;
    }
    return sArchiveState == 1;
}

bool InScene() {
    return gPlayState != nullptr && gPlayState->sceneNum == kSceneId && SceneArchiveLoaded();
}

bool HasMask() {
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_MARIO_MASK) != 0;
}

// Horizontal distance only. Link's Y is wherever the floor puts him, which need
// not match the height the stone was placed at, and folding that difference into
// the test was enough to push a player standing right at the stone out of range.
float DistSqToStone(Player* player) {
    const float dx = player->actor.world.pos.x - kStoneX;
    const float dz = player->actor.world.pos.z - kStoneZ;
    return dx * dx + dz * dz;
}

// ---------------------------------------------------------------------------
// The painting going black
// ---------------------------------------------------------------------------

// The portrait is an ordinary RGBA32 texture resource in custom-scenes.o2r, so
// "turning it black" is just zeroing its colour channels in place. Alpha is left
// alone so the cutout/TEX_EDGE material still behaves. The write happens on the
// live resource, which the ResourceMgr re-reads from the archive on reload — so
// this is a runtime effect, not a permanent edit to the user's .o2r.
void BlackenPainting() {
    if (gState.paintingBlackened) {
        return;
    }
    char* tex = ResourceMgr_LoadTexOrDListByName(kPaintingTexPath);
    if (tex == nullptr) {
        // Scene archive not present (or renamed): silently skip -- the rest of the
        // set piece still works.
        return;
    }
    // Byte count and format come from the resource itself rather than a hardcoded
    // 256x298: re-exporting the painting at another size would otherwise run this
    // loop off the end of the buffer, and at another format it would shred it.
    // (The pointer stays the one ResourceMgr_LoadTexOrDListByName handed us --
    // that is the address Fast3D keyed its upload on, so it is what the cache
    // delete below has to match.)
    auto res = std::dynamic_pointer_cast<Fast::Texture>(ResourceMgr_GetResourceByNameHandlingMQ(kPaintingTexPath));
    if (res == nullptr || res->Type != Fast::TextureType::RGBA32bpp) {
        return;
    }
    const size_t bytes = res->ImageDataSize;
    uint8_t* px = reinterpret_cast<uint8_t*>(tex);
    // RGBA32: 4 bytes per texel. Zero RGB, keep A.
    for (size_t i = 0; i + 3 < bytes; i += 4) {
        px[i + 0] = 0;
        px[i + 1] = 0;
        px[i + 2] = 0;
    }
    // Writing the bytes is not enough: Fast3D has already uploaded this texture to
    // the GPU and keys its cache on the source address, so without dropping the
    // cached copy the portrait keeps drawing the old, un-blackened pixels.
    Gfx_TextureCacheDelete(reinterpret_cast<const uint8_t*>(tex));
    gState.paintingBlackened = true;
}

// ---------------------------------------------------------------------------
// Dialogue
// ---------------------------------------------------------------------------

// The Gossip Stone opens its own vanilla hint textbox and we cannot choose the
// textId it picks, so match on "a textbox is opening, in this scene, next to the
// stone" rather than on a textId of our own. That keeps the actor completely
// untouched — model, targeting and all — and only swaps what it says.
//
// Skipped mid-cutscene so the get-item message is not clobbered.
void ShowMessage(const char* body, bool* loadFromMessageTable) {
    CustomMessage msg(body);
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void OnOpenTextDispatch(uint16_t* textId, bool* loadFromMessageTable) {
    if (!InScene() || gPlayState == nullptr) {
        return;
    }

    // Dialogue 2 — the line after the song. We opened this box ourselves, so it is
    // matched by id and needs no proximity check.
    if (*textId == kSongTextId) {
        ShowMessage("Take my strength...&Save the princess!", loadFromMessageTable);
        return;
    }

    // Dialogue 1 — checking the Gossip Stone. We open this box ourselves too (see
    // OnPlayerUpdateDispatch): waiting for En_Gs to open its own never fired, so
    // rather than keep guessing at its conditions we drive the interaction.
    if (*textId == kStoneTextId) {
        ShowMessage(HasMask() ? "...thank you, Link.&Let's a save the princess." : "I must save the princess...",
                    loadFromMessageTable);
        gState.talkedOnce = true;
    }
}

// Hands the mask over through the normal get-item path, so Link plays the
// hold-it-overhead animation and the item box appears.
//
// Item_Give() alone does NOT do this: it only drops the item into the inventory,
// silently, which is why the first version looked like nothing happened. The
// animation lives in GiveItemEntryWithoutActor, and that needs a GetItemEntry --
// hence RG_MARIO_MASK in the randomizer item table, which is also where the
// randomizer will pick it up later.
void GrantMask() {
    Flags_SetRandomizerInf(RAND_INF_OBTAINED_MARIO_MASK);
    BlackenPainting();
    GetItemEntry entry = Rando::StaticData::RetrieveItem(RG_MARIO_MASK).GetGIEntry_Copy();
    GiveItemEntryWithoutActor(gPlayState, entry);
}

void OnPlayerUpdateDispatch() {
    if (!InScene() || gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }

    switch (gState.phase) {
        case PHASE_LOOKING: {
            // Hold Link still while the camera sits on him.
            player->actor.speedXZ = 0.0f;
            player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
            if (--gState.timer <= 0) {
                Message_StartTextbox(gPlayState, kSongTextId, nullptr);
                gState.textOpened = false;
                gState.phase = PHASE_SPEAKING;
            }
            break;
        }
        case PHASE_SPEAKING: {
            player->actor.speedXZ = 0.0f;
            // Wait for the player to close dialogue 2, then release him and hand
            // the mask over -- the get-item animation needs the cutscene flag gone.
            //
            // Message_StartTextbox does not raise the box on the update it is called
            // from, so the state is still TEXT_STATE_NONE the first time we get here.
            // Testing for "none" directly therefore matched immediately and granted
            // the mask a frame after the song, skipping dialogue 2 entirely. Latch
            // that the box was really on screen first, then wait for it to close.
            const u8 msgState = Message_GetState(&gPlayState->msgCtx);
            if (msgState != TEXT_STATE_NONE) {
                gState.textOpened = true;
            }
            if (gState.textOpened && msgState == TEXT_STATE_NONE) {
                player->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
                GrantMask();
                gState.phase = PHASE_DONE;
            } else {
                player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
            }
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// The talk target on the painting
// ---------------------------------------------------------------------------

// Talking to the Gossip Stone never fired -- whatever gate En_Gs applies in this
// scene, its textbox never opened, so there was nothing to intercept. Instead we
// put an invisible talk target on the painting itself, which is what the player
// actually wants to check: walk up to Mario, press A, Mario answers.
//
// Built with the actor-hijack pattern this fork uses everywhere (see
// mods/actors/spiritual_stone_statue.c and mods/items/helpers/mailbox_actor.c):
// spawn a trivially-behaved vanilla actor and overwrite its function pointers.
// EN_LIGHTBOX is the usual host. Actor_OfferTalk handles the whole "in range and
// facing and not locked onto something else" offer, so we get real targeting and
// the normal A prompt for free.

constexpr float kTalkRadius = 90.0f;

// Just off the painting's front face (X=-2027, facing +X), centred on it in Z and
// low enough that Link is looking at it rather than over it.
constexpr float kTalkX = -2010.0f;
constexpr float kTalkY = -1780.0f;
constexpr float kTalkZ = -181.0f;

ActorFunc sTalkUpdateFunc = nullptr;

void Talk_Update(Actor* thisx, PlayState* play) {
    if (Actor_ProcessTalkRequest(thisx, play)) {
        return; // the textbox we set below is already up
    }
    // Point the offer at our own message. Unlike the mailbox (which zeroes textId
    // to suppress the box) we want the box, so Player_SetupTalk starts it for us.
    thisx->textId = kStoneTextId;
    Actor_OfferTalk(thisx, play, kTalkRadius);
}

void Talk_Draw(Actor* thisx, PlayState* play) {
    (void)thisx;
    (void)play; // invisible: the painting itself is the visual
}

bool TalkTargetExists() {
    if (gPlayState == nullptr || sTalkUpdateFunc == nullptr) {
        return false;
    }
    Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_PROP].head;
    for (; a != nullptr; a = a->next) {
        if (a->update == sTalkUpdateFunc) {
            return true;
        }
    }
    return false;
}

void SpawnTalkTarget() {
    if (gPlayState == nullptr || TalkTargetExists()) {
        return;
    }
    Actor* a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_LIGHTBOX, kTalkX, kTalkY, kTalkZ, 0, 0, 0, 0);
    if (a == nullptr) {
        return;
    }
    EnLightbox* lightbox = reinterpret_cast<EnLightbox*>(a);
    if (lightbox->dyna.bgId != BGACTOR_NEG_ONE) {
        // Drop EnLightbox's DynaPoly or the player would collide with an invisible box.
        DynaPoly_DeleteBgActor(gPlayState, &gPlayState->colCtx.dyna, lightbox->dyna.bgId);
        lightbox->dyna.bgId = BGACTOR_NEG_ONE;
    }
    a->update = Talk_Update;
    a->draw = Talk_Draw;
    sTalkUpdateFunc = Talk_Update;

    a->gravity = 0.0f;
    a->minVelocityY = 0.0f;
    a->shape.shadowDraw = nullptr;
    a->shape.shadowScale = 0.0f;
    a->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    a->flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
}

// ---------------------------------------------------------------------------
// The song
// ---------------------------------------------------------------------------

// Fires when the player completes ANY recognised ocarina song (z_message_PAL.c).
// Deliberately not gated on Song of Healing: mm_songs.cpp only recognises MM songs
// the player already OWNS (FC_MMQ_SONG_HEALING), which a fresh save will not have,
// and this is meant to be reachable.
void OnOcarinaSongActionDispatch() {
    if (!InScene() || gPlayState == nullptr) {
        return;
    }
    if (gState.phase != PHASE_IDLE || HasMask()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || DistSqToStone(player) > kSongRangeSq) {
        return;
    }

    gState.phase = PHASE_LOOKING;
    gState.timer = kLookFrames;

    // MM healing-shape beat: pull the camera onto Link and hold him.
    player->actor.speedXZ = 0.0f;
    player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
    OnePointCutscene_Attention(gPlayState, &player->actor);
    Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// ---------------------------------------------------------------------------
// Scene entry / save glue
// ---------------------------------------------------------------------------

void OnSceneSpawnActorsDispatch() {
    sArchiveState = -1; // re-probe: the archive may have been added since last warp
    gState.phase = HasMask() ? PHASE_DONE : PHASE_IDLE;
    gState.timer = 0;
    gState.paintingBlackened = false;
    gState.textOpened = false;
    if (!InScene()) {
        return;
    }
    // Called from OnSceneSpawnActors, so the actor list is ready to accept spawns.
    SpawnTalkTarget();
    if (HasMask()) {
        // Returning to the room after the fact: the portrait stays empty.
        BlackenPainting();
    }
}

void SaveSection(SaveContext* saveContext, int sectionID, bool fullSave) {
    SaveManager::Instance->SaveData("talkedOnce", gState.talkedOnce);
}

void LoadSection() {
    SaveManager::Instance->LoadData("talkedOnce", gState.talkedOnce);
}

void InitFile(bool isDebug) {
    (void)isDebug;
    gState = State{};
}

void Register() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    SaveManager::Instance->AddInitFunction(InitFile);
    SaveManager::Instance->AddSaveFunction(kSaveSection, 1, SaveSection, true, -1);
    SaveManager::Instance->AddLoadFunction(kSaveSection, 1, LoadSection);
    CustomMessageManager::Instance->AddCustomMessageTable(kMessageTableId);

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenTextDispatch);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateDispatch);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOcarinaSongAction>(OnOcarinaSongActionDispatch);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(OnSceneSpawnActorsDispatch);
}

} // namespace

static RegisterShipInitFunc gMarioMaskSceneInit(Register, {});
