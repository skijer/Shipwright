// Hum ocarina songs into the microphone using YIN pitch detection.

#include "soh/ModApi/ModApi.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/log/luslog.h>

#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
int32_t CVarGetInteger(const char* name, int32_t defaultValue);
int16_t OTRGetRectDimensionFromLeftEdge(float v);
int16_t OTRGetRectDimensionFromRightEdge(float v);

extern HOST_DATA u16 sAvailOcarinaSongFlags;
extern HOST_DATA u8 sFirstOcarinaSongIndex;
extern HOST_DATA u8 sLastOcarinaSongIndex;
extern HOST_DATA u8 sPlayedOcarinaSongIndexPlusOne;
extern HOST_DATA u8 sIsOcarinaInputEnabled;
extern HOST_DATA u32 sOcarinaFlags;
}

namespace {

// Inside an anonymous namespace OPEN_DISPS's block-scope redeclarations name these, so they need C linkage too.
extern "C" void FrameInterpolation_RecordOpenChild(const void* a, int b);
extern "C" void FrameInterpolation_RecordCloseChild(void);

constexpr const char* kEnabledCvar = "gEnhancements.MicOcarina.Enabled";

constexpr int kSampleRate = 48000;
constexpr int kWindowSize = 2048;
constexpr int kHopSize = 512;
constexpr int kIntegrationSize = kWindowSize / 2;
constexpr int kRingSize = 1 << 15;

constexpr int kTauMin = kSampleRate / 1000;
constexpr int kTauMax = kSampleRate / 80;

constexpr float kYinThreshold = 0.15f;
constexpr int kMedianSize = 5;
constexpr int kMaxPhraseNotes = 8;

constexpr int kReleaseHops = 5;
constexpr int kReanchorHops = 90;

constexpr int kPhraseEndHops = 40;
constexpr float kPhraseEndMaxErrorCents = 180.0f;

constexpr float kDipRatio = 0.45f;
constexpr float kRecoverRatio = 0.70f;

constexpr float kJumpCents = 90.0f;
constexpr int kJumpHops = 3;

constexpr int kAttackHops = 3;

constexpr int kMinNoteHops = 9;

constexpr float kGlideCents = 60.0f;

constexpr float kFloorFallRate = 0.25f;
constexpr float kFloorRiseRate = 0.002f;
constexpr float kGateCloseRatio = 0.5f;
constexpr float kNoiseMargin = 4.0f;
constexpr float kAbsoluteGate = 0.0056f;
constexpr float kClarityMin = 0.55f;

constexpr float kContourDeadbandCents = 90.0f;

constexpr float kMaxPhraseErrorCents = 120.0f;
constexpr float kMatchMarginCents = 40.0f;
constexpr float kNoMatchError = 1.0e9f;

constexpr int kScaleDegrees[5] = { 0, 3, 7, 9, 12 };

constexpr float kHopSeconds = static_cast<float>(kHopSize) / kSampleRate;
constexpr int kMeterSegmentMax = 24;
constexpr float kMeterWindowSeconds = 6.0f;
constexpr int kMeterSemitoneRange = 48;
constexpr int kMeterBarHalfHeight = 1;

constexpr int kOcarinaOutGraceFrames = 4;
constexpr int kMicGraceFrames = 60;
constexpr int kOpenRetryFrames = 300;
constexpr uint32_t kCaptureChunkSamples = 2048;
constexpr int kMaxChunksPerFrame = 16;

struct MeterSegment {
    float startSeconds;
    float endSeconds;
    float cents;
    bool accepted;
};

struct MeterHistory {
    MeterSegment segments[kMeterSegmentMax];
    int count;
    float nowSeconds;
    float anchorCents;
    bool hasAnchor;
};

float sRing[kRingSize];
uint32_t sRingWritePos = 0;
uint32_t sRingReadPos = 0;

std::atomic<uint32_t> sAvailableSongFlags{ 0 };
std::atomic<int> sFirstSongIndex{ 0 };
std::atomic<int> sLastSongIndex{ 0 };

std::atomic<int> sMatchedSong{ -1 };
std::atomic<uint32_t> sMatchSequence{ 0 };
bool sMeterSignalPresent = false;
MeterHistory sMeterHistory;

struct PitchEstimate {
    float hz;
    float clarity;
};

float sDiff[kTauMax + 2];
float sCmndf[kTauMax + 2];

constexpr int kRecognisedSongs[] = {
    OCARINA_SONG_MINUET,   OCARINA_SONG_BOLERO,  OCARINA_SONG_SERENADE, OCARINA_SONG_REQUIEM,
    OCARINA_SONG_NOCTURNE, OCARINA_SONG_PRELUDE, OCARINA_SONG_SARIAS,   OCARINA_SONG_EPONAS,
    OCARINA_SONG_LULLABY,  OCARINA_SONG_SUNS,    OCARINA_SONG_TIME,     OCARINA_SONG_STORMS,
};

constexpr int kCustomSongBase = 32;
struct CustomSong {
    const char* key;
    int length;
    uint8_t buttons[8];
};
constexpr CustomSong kCustomSongs[] = {
#include "mm_song_catalog.inc.h"
};
constexpr int kCustomSongCount = sizeof(kCustomSongs) / sizeof(kCustomSongs[0]);
std::atomic<uint32_t> sCustomOwned{ 0 };
std::atomic<bool> sFreePlay{ false };
std::atomic<int> sPendingCustomMatch{ -1 };
thread_local bool sDispatchingCustomNotes = false;

int SongLength(int song) {
    return song >= kCustomSongBase ? kCustomSongs[song - kCustomSongBase].length : gOcarinaSongButtons[song].numButtons;
}
int SongButton(int song, int note) {
    return song >= kCustomSongBase ? kCustomSongs[song - kCustomSongBase].buttons[note]
                                   : gOcarinaSongButtons[song].buttonsIndex[note];
}

float ComputeRms(const float* window, int count) {
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += window[i] * window[i];
    }
    return sqrtf(sum / count);
}

PitchEstimate EstimatePitch(const float* window) {
    for (int tau = 1; tau <= kTauMax; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < kIntegrationSize; j++) {
            float delta = window[j] - window[j + tau];
            sum += delta * delta;
        }
        sDiff[tau] = sum;
    }

    float runningSum = 0.0f;
    sCmndf[0] = 1.0f;
    for (int tau = 1; tau <= kTauMax; tau++) {
        runningSum += sDiff[tau];
        sCmndf[tau] = (runningSum > 0.0f) ? (sDiff[tau] * tau / runningSum) : 1.0f;
    }

    int best = -1;
    for (int tau = kTauMin; tau < kTauMax; tau++) {
        if (sCmndf[tau] >= kYinThreshold) {
            continue;
        }
        while ((tau + 1 < kTauMax) && (sCmndf[tau + 1] < sCmndf[tau])) {
            tau++;
        }
        best = tau;
        break;
    }
    if (best < 0) {
        return { 0.0f, 0.0f };
    }

    float prev = sCmndf[best - 1];
    float curr = sCmndf[best];
    float next = sCmndf[best + 1];
    float denom = prev - (2.0f * curr) + next;
    float tauStar = static_cast<float>(best);
    if (denom != 0.0f) {
        tauStar += 0.5f * (prev - next) / denom;
    }
    if (tauStar < 1.0f) {
        return { 0.0f, 0.0f };
    }
    return { kSampleRate / tauStar, 1.0f - curr };
}

float MedianOf(const float* values, int count) {
    float sorted[kMedianSize];
    memcpy(sorted, values, count * sizeof(float));
    for (int i = 1; i < count; i++) {
        float key = sorted[i];
        int j = i - 1;
        while ((j >= 0) && (sorted[j] > key)) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    return sorted[count / 2];
}

struct NoteTracker {
    float centsHistory[kMedianSize];
    int historyCount;
    float anchorCents;
    bool hasAnchor;
    float phraseCents[kMaxPhraseNotes];
    int phraseCount;
    float noteCents;
    bool noteActive;
    bool noteInPhrase;
    int noteHops;
    float holdStartCents;
    float notePeakRms;
    bool sawDip;
    int silentHops;
    int deviationHops;
    float pendingCents;
    float pendingRms;
    int pendingHops;
    float noiseFloor;
};

NoteTracker sTracker;

void ClearMeter() {
    sMeterHistory.count = 0;
    sMeterHistory.hasAnchor = false;
}

void BeginMeterSegment(float cents) {
    if (sMeterHistory.count >= kMeterSegmentMax) {
        memmove(sMeterHistory.segments, &sMeterHistory.segments[1], (kMeterSegmentMax - 1) * sizeof(MeterSegment));
        sMeterHistory.count = kMeterSegmentMax - 1;
    }
    float now = sMeterHistory.nowSeconds;
    sMeterHistory.segments[sMeterHistory.count++] = { now, now, cents, false };
}

void AcceptMeterSegment(float cents, float anchorCents) {
    if (sMeterHistory.count == 0) {
        return;
    }
    MeterSegment& live = sMeterHistory.segments[sMeterHistory.count - 1];
    live.cents = cents;
    live.accepted = true;
    sMeterHistory.anchorCents = anchorCents;
    sMeterHistory.hasAnchor = true;
}

void AdvanceMeter(bool noteHeld) {
    sMeterHistory.nowSeconds += kHopSeconds;
    if (noteHeld && (sMeterHistory.count > 0)) {
        sMeterHistory.segments[sMeterHistory.count - 1].endSeconds = sMeterHistory.nowSeconds;
    }
}

void ResetTracker() {
    memset(&sTracker, 0, sizeof(sTracker));
    sMatchedSong.store(-1, std::memory_order_relaxed);
    sMatchSequence.fetch_add(1, std::memory_order_release);
    ClearMeter();
}

bool IsSongAccepted(int songIndex) {
    if (songIndex >= kCustomSongBase) {
        const int custom = songIndex - kCustomSongBase;
        return custom < kCustomSongCount && sFreePlay.load(std::memory_order_relaxed) &&
               (sCustomOwned.load(std::memory_order_relaxed) & (1u << custom));
    }
    if ((songIndex < sFirstSongIndex.load(std::memory_order_relaxed)) ||
        (songIndex >= sLastSongIndex.load(std::memory_order_relaxed))) {
        return false;
    }
    return (sAvailableSongFlags.load(std::memory_order_relaxed) & (1u << songIndex)) != 0;
}

float PhraseError(const float* cents, int count, int songIndex) {
    if ((count < 2) || (SongLength(songIndex) != count)) {
        return kNoMatchError;
    }

    float residual[kMaxPhraseNotes];
    float mean = 0.0f;
    for (int i = 0; i < count; i++) {
        int degree = SongButton(songIndex, i);
        if ((degree < OCARINA_BTN_A) || (degree > OCARINA_BTN_C_UP)) {
            return kNoMatchError;
        }
        residual[i] = cents[i] - (100.0f * kScaleDegrees[degree]);
        mean += residual[i];
    }
    mean /= count;

    float sumSquared = 0.0f;
    for (int i = 0; i < count; i++) {
        float error = residual[i] - mean;
        sumSquared += error * error;
    }
    return sqrtf(sumSquared / count);
}

int BuildMergedPhrase(const float* cents, int count, float* merged) {
    int write = 0;
    for (int read = 0; read < count; read++) {
        if ((write > 0) && (fabsf(cents[read] - merged[write - 1]) < kContourDeadbandCents)) {
            continue;
        }
        merged[write++] = cents[read];
    }
    return write;
}

float SuffixError(int songIndex, int length) {
    if ((length < 2) || (length > sTracker.phraseCount)) {
        return kNoMatchError;
    }
    float raw = PhraseError(&sTracker.phraseCents[sTracker.phraseCount - length], length, songIndex);

    int widened = (length + 2 <= sTracker.phraseCount) ? (length + 2) : sTracker.phraseCount;
    float merged[kMaxPhraseNotes];
    int mergedCount = BuildMergedPhrase(&sTracker.phraseCents[sTracker.phraseCount - widened], widened, merged);
    if (mergedCount < length) {
        return raw;
    }
    return fminf(raw, PhraseError(&merged[mergedCount - length], length, songIndex));
}

struct MatchCandidates {
    int bestSong;
    float bestError;
    float runnerUpError;
};

MatchCandidates RankSongs() {
    MatchCandidates ranked = { -1, kNoMatchError, kNoMatchError };
    constexpr int nativeCount = sizeof(kRecognisedSongs) / sizeof(kRecognisedSongs[0]);
    for (int candidate = 0; candidate < nativeCount + kCustomSongCount; ++candidate) {
        const int songIndex =
            candidate < nativeCount ? kRecognisedSongs[candidate] : kCustomSongBase + candidate - nativeCount;
        if (!IsSongAccepted(songIndex)) {
            continue;
        }
        float error = SuffixError(songIndex, SongLength(songIndex));
        if (error < ranked.bestError) {
            ranked.runnerUpError = ranked.bestError;
            ranked.bestError = error;
            ranked.bestSong = songIndex;
        } else if (error < ranked.runnerUpError) {
            ranked.runnerUpError = error;
        }
    }
    return ranked;
}

void PublishMatch(int songIndex, float error) {
    sMatchedSong.store(songIndex, std::memory_order_relaxed);
    sMatchSequence.fetch_add(1, std::memory_order_release);
    sTracker.phraseCount = 0;
    LUSLOG_DEBUG("MicOcarina: matched song %d at %.0f cents RMS", songIndex, error);
}

void TryMatchSuffix() {
    MatchCandidates ranked = RankSongs();
    if ((ranked.bestSong < 0) || (ranked.bestError > kMaxPhraseErrorCents)) {
        return;
    }
    if ((ranked.runnerUpError - ranked.bestError) < kMatchMarginCents) {
        LUSLOG_DEBUG("MicOcarina: ambiguous, %d and runner-up within %.0f cents", ranked.bestSong,
                     ranked.runnerUpError - ranked.bestError);
        return;
    }
    PublishMatch(ranked.bestSong, ranked.bestError);
}

void TryMatchPhraseEnd() {
    MatchCandidates ranked = RankSongs();
    if ((ranked.bestSong < 0) || (ranked.bestError > kPhraseEndMaxErrorCents)) {
        return;
    }
    PublishMatch(ranked.bestSong, ranked.bestError);
}

void PushPhraseNote(float cents) {
    if (!sTracker.hasAnchor) {
        sTracker.anchorCents = cents;
        sTracker.hasAnchor = true;
    }
    if (sTracker.phraseCount >= kMaxPhraseNotes) {
        memmove(sTracker.phraseCents, &sTracker.phraseCents[1], (kMaxPhraseNotes - 1) * sizeof(float));
        sTracker.phraseCount = kMaxPhraseNotes - 1;
    }
    sTracker.phraseCents[sTracker.phraseCount++] = cents;
}

void AcceptHeldNote() {
    sTracker.noteInPhrase = true;
    PushPhraseNote(sTracker.noteCents);
    AcceptMeterSegment(sTracker.noteCents, sTracker.anchorCents);
    TryMatchSuffix();
}

void CommitNote(float cents, float rms) {
    BeginMeterSegment(cents);
    sTracker.noteCents = cents;
    sTracker.noteActive = true;
    sTracker.noteInPhrase = false;
    sTracker.noteHops = 0;
    sTracker.holdStartCents = cents;
    sTracker.notePeakRms = rms;
    sTracker.sawDip = false;
    sTracker.deviationHops = 0;
    sTracker.pendingHops = 0;
}

void ProposeNote(float cents, float rms) {
    if ((sTracker.pendingHops > 0) && (fabsf(cents - sTracker.pendingCents) > kJumpCents)) {
        sTracker.pendingHops = 0;
    }
    if (sTracker.pendingHops == 0) {
        sTracker.pendingCents = cents;
        sTracker.pendingRms = rms;
    }
    sTracker.pendingRms = fmaxf(sTracker.pendingRms, rms);
    sTracker.pendingHops++;
    if (sTracker.pendingHops >= kAttackHops) {
        CommitNote(cents, sTracker.pendingRms);
    }
}

void EndNote() {
    sTracker.noteActive = false;
    sTracker.historyCount = 0;
    sTracker.pendingHops = 0;
}

void ProcessWindow(const float* window) {
    AdvanceMeter(sTracker.noteActive);
    float rms = ComputeRms(window, kWindowSize);

    if (rms < sTracker.noiseFloor) {
        sTracker.noiseFloor += kFloorFallRate * (rms - sTracker.noiseFloor);
    } else if (!sTracker.noteActive) {
        sTracker.noiseFloor += kFloorRiseRate * (rms - sTracker.noiseFloor);
    }

    float openLevel = fmaxf(kAbsoluteGate, sTracker.noiseFloor * kNoiseMargin);
    float level = sTracker.noteActive ? (openLevel * kGateCloseRatio) : openLevel;

    PitchEstimate estimate = { 0.0f, 0.0f };
    if (rms > level) {
        estimate = EstimatePitch(window);
    }
    sMeterSignalPresent = rms > 1.0e-6f;

    bool voiced = (estimate.clarity > kClarityMin) && (estimate.hz > 0.0f);
    if (!voiced) {
        sTracker.silentHops++;
        if (sTracker.noteActive && (sTracker.silentHops >= kReleaseHops)) {
            EndNote();
        }
        if (sTracker.silentHops == kPhraseEndHops) {
            TryMatchPhraseEnd();
        }
        if (sTracker.silentHops >= kReanchorHops) {
            sTracker.hasAnchor = false;
            sTracker.phraseCount = 0;
            ClearMeter();
        }
        return;
    }
    sTracker.silentHops = 0;

    float cents = 1200.0f * log2f(estimate.hz / 440.0f);
    if (sTracker.historyCount < kMedianSize) {
        sTracker.centsHistory[sTracker.historyCount++] = cents;
    } else {
        memmove(sTracker.centsHistory, &sTracker.centsHistory[1], (kMedianSize - 1) * sizeof(float));
        sTracker.centsHistory[kMedianSize - 1] = cents;
    }
    float smoothed = MedianOf(sTracker.centsHistory, sTracker.historyCount);

    if (!sTracker.noteActive) {
        ProposeNote(smoothed, rms);
        return;
    }

    sTracker.notePeakRms = fmaxf(sTracker.notePeakRms, rms);
    if (rms < (kDipRatio * sTracker.notePeakRms)) {
        sTracker.sawDip = true;
    }
    if (sTracker.sawDip && (rms > (kRecoverRatio * sTracker.notePeakRms))) {
        CommitNote(smoothed, rms);
        return;
    }

    if (fabsf(smoothed - sTracker.noteCents) > kJumpCents) {
        sTracker.deviationHops++;
        if (sTracker.deviationHops >= kJumpHops) {
            CommitNote(smoothed, rms);
        }
        return;
    }
    sTracker.deviationHops = 0;

    sTracker.noteCents += 0.15f * (smoothed - sTracker.noteCents);

    if (!sTracker.noteInPhrase && (fabsf(smoothed - sTracker.holdStartCents) > kGlideCents)) {
        sTracker.holdStartCents = smoothed;
        sTracker.noteHops = 0;
    }
    sTracker.noteHops++;
    if (!sTracker.noteInPhrase && (sTracker.noteHops >= kMinNoteHops)) {
        AcceptHeldNote();
    }
}

void PushSamples(const float* samples, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        sRing[(sRingWritePos + i) & (kRingSize - 1)] = samples[i];
    }
    sRingWritePos += count;
}

void ProcessPendingHops() {
    float window[kWindowSize];
    while ((sRingWritePos - sRingReadPos) >= kWindowSize) {
        if ((sRingWritePos - sRingReadPos) > (kRingSize / 2)) {
            sRingReadPos = sRingWritePos - kWindowSize;
        }
        for (int i = 0; i < kWindowSize; i++) {
            window[i] = sRing[(sRingReadPos + i) & (kRingSize - 1)];
        }
        sRingReadPos += kHopSize;
        ProcessWindow(window);
    }
}

const SOHModApi* sApi = nullptr;
bool sCaptureOpen = false;

SOHMicrophoneStatus OpenCaptureDevice() {
    if (!SOH_MOD_API_HAS(sApi, CloseMicrophone)) {
        return SOH_MICROPHONE_UNAVAILABLE;
    }
    const SOHMicrophoneStatus status = sApi->OpenMicrophone(kSampleRate);
    if (status == SOH_MICROPHONE_OPEN) {
        ResetTracker();
        sRingReadPos = sRingWritePos;
        sCaptureOpen = true;
    }
    return status;
}

void PumpCapture() {
    float chunk[kCaptureChunkSamples];
    for (int i = 0; i < kMaxChunksPerFrame; i++) {
        const uint32_t count = sApi->ReadMicrophone(chunk, kCaptureChunkSamples);
        if (count == 0) {
            return;
        }
        PushSamples(chunk, count);
        ProcessPendingHops();
    }
}

void CloseCaptureDevice() {
    if (!sCaptureOpen) {
        return;
    }
    sApi->CloseMicrophone();
    sCaptureOpen = false;
    ResetTracker();
}

std::atomic<bool> sRequested{ false };
int sIdleFrames = kMicGraceFrames;
int sOpenRetryFrames = 0;
bool sOcarinaOut = false;
uint32_t sLastSeenMatch = 0;

void TickMicOcarina() {
    uint32_t owned = 0;
    if (gPlayState != nullptr)
        for (int i = 0; i < kCustomSongCount; ++i) {
            if (sApi->IsCustomItemOwned(kCustomSongs[i].key))
                owned |= 1u << i;
        }
    sCustomOwned.store(owned, std::memory_order_relaxed);
    const int custom = sPendingCustomMatch.exchange(-1, std::memory_order_acquire);
    if (custom >= 0 && custom < kCustomSongCount && (owned & (1u << custom)) && CVarGetInteger(kEnabledCvar, 1) &&
        gPlayState != nullptr && gPlayState->pauseCtx.state == 0 &&
        gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) {
        static const uint8_t pitches[] = { 2, 5, 9, 11, 14 };
        sDispatchingCustomNotes = true;
        for (int n = 0; n < kCustomSongs[custom].length; ++n) {
            GameInteractor_ExecuteOnOcarinaNote(pitches[kCustomSongs[custom].buttons[n]], 1.0f, 0);
            GameInteractor_ExecuteOnOcarinaNote(OCARINA_PITCH_NONE, 1.0f, 0);
        }
        sDispatchingCustomNotes = false;
    }
    if (sRequested.exchange(false, std::memory_order_relaxed)) {
        sIdleFrames = 0;
    } else if (sIdleFrames < kMicGraceFrames) {
        sIdleFrames++;
    }
    sOcarinaOut = sIdleFrames < kOcarinaOutGraceFrames;

    if (sOpenRetryFrames > 0) {
        sOpenRetryFrames--;
    }

    bool wanted = CVarGetInteger(kEnabledCvar, 1) && (sIdleFrames < kMicGraceFrames);
    if (wanted && !sCaptureOpen) {
        if (sOpenRetryFrames > 0) {
            return;
        }
        const SOHMicrophoneStatus status = OpenCaptureDevice();
        if (status == SOH_MICROPHONE_DENIED || status == SOH_MICROPHONE_UNAVAILABLE) {
            sOpenRetryFrames = kOpenRetryFrames;
        }
        return;
    }
    if (!wanted) {
        CloseCaptureDevice();
        return;
    }
    PumpCapture();
}

void ListenOnAudioThread(uint8_t note, float modulator, int8_t bend) {
    if (sDispatchingCustomNotes)
        return;
    sFreePlay.store(sIsOcarinaInputEnabled && (sOcarinaFlags & 0xC000) == 0xC000, std::memory_order_relaxed);
    sRequested.store(true, std::memory_order_relaxed);
    sAvailableSongFlags.store(sAvailOcarinaSongFlags, std::memory_order_relaxed);
    sFirstSongIndex.store(sFirstOcarinaSongIndex, std::memory_order_relaxed);
    sLastSongIndex.store(sLastOcarinaSongIndex, std::memory_order_relaxed);

    uint32_t sequence = sMatchSequence.load(std::memory_order_acquire);
    if (sequence == sLastSeenMatch) {
        return;
    }
    sLastSeenMatch = sequence;

    int song = sMatchedSong.load(std::memory_order_relaxed);
    if (song >= kCustomSongBase) {
        if (IsSongAccepted(song)) {
            sIsOcarinaInputEnabled = 0;
            sPendingCustomMatch.store(song - kCustomSongBase, std::memory_order_release);
        }
        return;
    }
    if ((song < sFirstOcarinaSongIndex) || (song >= sLastOcarinaSongIndex) ||
        !(sAvailOcarinaSongFlags & (1u << song))) {
        return;
    }

    sPlayedOcarinaSongIndexPlusOne = static_cast<u8>(song + 1);
    sIsOcarinaInputEnabled = 0;
    sOcarinaFlags = 0;
}

float SemisToY(float originY, float semis, float pixelsPerSemi) {
    return originY - (semis * pixelsPerSemi);
}

void DrawSolidRect(PlayState* play, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t rgba) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF);
    gSPWideTextureRectangle(OVERLAY_DISP++, x1 << 2, y1 << 2, x2 << 2, y2 << 2, G_TX_RENDERTILE, 0, 0, 0, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

void SetupSolidRects(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetOtherMode(OVERLAY_DISP++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PRIM | G_RM_CLD_SURF | G_RM_CLD_SURF2);
    CLOSE_DISPS(play->state.gfxCtx);
}

void DrawHint(PlayState* play, const char* hint) {
    int x = (SCREEN_WIDTH - static_cast<int>(strlen(hint)) * 8) / 2;
    int y = SCREEN_HEIGHT - (SCREEN_HEIGHT * 8 / 100);
    OPEN_DISPS(play->state.gfxCtx);
    GfxPrint printer;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, OVERLAY_DISP);
    GfxPrint_SetColor(&printer, 0xFF, 0xD5, 0x66, 220);
    GfxPrint_SetPosPx(&printer, x, y);
    GfxPrint_Printf(&printer, "%s", hint);
    OVERLAY_DISP = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    CLOSE_DISPS(play->state.gfxCtx);
}

void DrawMeter(PlayState* play) {
    if (!sCaptureOpen || !sOcarinaOut) {
        return;
    }

    const MeterHistory& history = sMeterHistory;

    int16_t left = OTRGetRectDimensionFromLeftEdge(0);
    int16_t right = OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH);
    float pixelsPerSecond = (right - left) / kMeterWindowSeconds;
    float pixelsPerSemi = static_cast<float>(SCREEN_HEIGHT) / kMeterSemitoneRange;
    float originY = SCREEN_HEIGHT * 0.5f;

    SetupSolidRects(play);
    for (int i = 0; i < history.count; i++) {
        const MeterSegment& segment = history.segments[i];
        float newestOffset = (history.nowSeconds - segment.endSeconds) * pixelsPerSecond;
        if (left + newestOffset >= right) {
            continue;
        }
        float oldestOffset = (history.nowSeconds - segment.startSeconds) * pixelsPerSecond;
        int16_t x1 = static_cast<int16_t>(left + newestOffset);
        int16_t x2 = static_cast<int16_t>(fminf(right, left + oldestOffset));
        if (x2 <= x1) {
            x2 = x1 + 1;
        }

        float semis = history.hasAnchor ? ((segment.cents - history.anchorCents) / 100.0f) : 0.0f;
        semis = fminf(fmaxf(semis, -12.5f), 12.5f);
        int16_t y = static_cast<int16_t>(SemisToY(originY, semis, pixelsPerSemi));
        bool live = (i == history.count - 1) && (segment.endSeconds >= history.nowSeconds);
        uint32_t color = segment.accepted ? (live ? 0x7CE88AC8 : 0x7CE88A78) : (live ? 0xFFFFFF96 : 0xFFFFFF3C);
        DrawSolidRect(play, x1, y - kMeterBarHalfHeight, x2, y + kMeterBarHalfHeight, color);
    }

    if (!history.hasAnchor) {
        DrawHint(play, sMeterSignalPresent ? "hold a note to set the key" : "no mic signal");
    }
}

void RegisterMenuToggle(const SOHModApi* api) {
    if (!SOH_MOD_API_HAS(api, RegisterMenuWidgetAt)) {
        return;
    }
    SOHModMenuWidget widget = {};
    widget.structSize = sizeof(widget);
    widget.section = "Enhancements";
    widget.sidebar = "Items";
    widget.column = 0;
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = "Play Ocarina by Humming";
    widget.cvar = kEnabledCvar;
    widget.tooltip = "Hum, sing or whistle a song into the microphone while the ocarina is out and it plays "
                     "as if entered on the buttons. The first note held sets the key.";
    widget.defaultInt = 1;
    api->RegisterMenuWidgetAt(&widget, "Fast Ocarina Playback", true);
}

const char* const kRequiredHooks[] = { "OnOcarinaNote", "OnGameFrameUpdate", "OnInterfaceDrawEnd" };
const SOHModRequirements kRequirements = { sizeof(SOHModRequirements), kRequiredHooks,
                                           static_cast<uint32_t>(sizeof(kRequiredHooks) / sizeof(kRequiredHooks[0])) };

} // namespace

extern "C" SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

extern "C" SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &kRequirements;
}

extern "C" SOH_MOD_EXPORT void ModInit(void) {
    RegisterMenuToggle(sApi);
    SOH_REGISTER_HOOK(sApi, OnOcarinaNote, ListenOnAudioThread);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, TickMicOcarina);
    SOH_REGISTER_HOOK(sApi, OnInterfaceDrawEnd, DrawMeter);
}
