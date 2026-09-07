// mm_songs.cpp — Skijer's NEI / Fleet Ship Combo: MM songs RECOGNIZED by OoT's ocarina.
//
// The 7 MM-unique songs (Sonata, Goron Lullaby, New Wave Bossa Nova, Elegy, Oath, Healing,
// Soaring) become "relatives" in OoT: ownership lives in NeiSaveData.mmQuestItems (FC_MMQ_* bits,
// synced cross-game by FleetSync), and playing an owned song's notes on the ocarina is recognized
// with the confirmation chime. NO gameplay effect yet (user decision) — this is the knowledge/
// routing layer the combo rando needs.
//
// Recognition: a rolling buffer of played pitches fed by GameInteractor::OnOcarinaNote (fired per
// note in code_800EC960.c), matched against the authoritative MM button sequences
// (mm code_8019AF00.c gOcarinaSongButtons) translated to OoT pitches:
//   A = D4(2), C-down = F4(5), C-right = A4(9), C-left = B4(11), C-up = D5(14).

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/FleetShipCombo/FleetComboIds.h"

extern "C" {
#include <z64.h>
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "mods/nei_save.h"
extern PlayState* gPlayState;
}

namespace {

struct MmSong {
    uint32_t ownedBit; // FC_MMQ_* gate in NeiSaveData.mmQuestItems
    uint8_t len;
    uint8_t pitches[8]; // OoT ocarina pitch values in play order
    const char* name;
};

constexpr uint8_t kA = 2;   // OCARINA_PITCH_D4
constexpr uint8_t kCD = 5;  // OCARINA_PITCH_F4
constexpr uint8_t kCR = 9;  // OCARINA_PITCH_A4
constexpr uint8_t kCL = 11; // OCARINA_PITCH_B4
constexpr uint8_t kCU = 14; // OCARINA_PITCH_D5

const MmSong kMmSongs[] = {
    { FC_MMQ_SONG_SONATA, 7, { kCU, kCL, kCU, kCL, kA, kCR, kA }, "Sonata of Awakening" },
    { FC_MMQ_SONG_GORON_LULLABY, 8, { kA, kCR, kCL, kA, kCR, kCL, kCR, kA }, "Goron Lullaby" },
    { FC_MMQ_SONG_NEW_WAVE, 7, { kCL, kCU, kCL, kCR, kCD, kCL, kCR }, "New Wave Bossa Nova" },
    { FC_MMQ_SONG_ELEGY, 7, { kCR, kCL, kCR, kCD, kCR, kCU, kCL }, "Elegy of Emptiness" },
    { FC_MMQ_SONG_OATH, 6, { kCR, kCD, kA, kCD, kCR, kCU }, "Oath to Order" },
    { FC_MMQ_SONG_HEALING, 6, { kCL, kCR, kCD, kCL, kCR, kCD }, "Song of Healing" },
    { FC_MMQ_SONG_SOARING, 6, { kCD, kCL, kCU, kCD, kCL, kCU }, "Song of Soaring" },
};

uint8_t sNoteBuf[8];
int sNoteCount = 0;

void OnNote(uint8_t note, float modulator, int8_t bend) {
    (void)modulator;
    (void)bend;
    if (gPlayState == NULL) {
        return;
    }
    // Shift the new note into the rolling buffer.
    for (int i = 0; i < 7; i++) {
        sNoteBuf[i] = sNoteBuf[i + 1];
    }
    sNoteBuf[7] = note;
    if (sNoteCount < 8) {
        sNoteCount++;
    }

    uint32_t owned = Nei_Save()->mmQuestItems;
    for (const MmSong& song : kMmSongs) {
        if (!(owned & song.ownedBit) || sNoteCount < song.len) {
            continue;
        }
        bool match = true;
        for (int i = 0; i < song.len; i++) {
            if (sNoteBuf[8 - song.len + i] != song.pitches[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            // Recognized: confirmation chime, no gameplay effect (yet). Clear the buffer so the
            // tail can't instantly re-match.
            Audio_PlaySoundGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            sNoteCount = 0;
            for (int i = 0; i < 8; i++) {
                sNoteBuf[i] = 0xFF;
            }
            break;
        }
    }
}

void RegisterMmSongs() {
    for (int i = 0; i < 8; i++) {
        sNoteBuf[i] = 0xFF;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOcarinaNote>(OnNote);
}

} // namespace

static RegisterShipInitFunc initMmSongs(RegisterMmSongs, {});
