// Humming to ocarina songs: YIN pitch detection on the game's microphone service. A phrase is scored against
// every song the game currently accepts, with the transposition fitted out, and the winner is handed to the
// native recogniser as already played. Port of NEI's MicOcarina.cpp; the DSP is kept identical.

#include "soh/ModApi/ModApi.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/log/luslog.h>

#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
// OPEN_DISPS redeclares these inside its body, which would give them C++ linkage here.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
// The bridge header drags libultraship's C++ config classes into a C++ file.
int32_t CVarGetInteger(const char* name, int32_t defaultValue);
int16_t OTRGetRectDimensionFromLeftEdge(float v);
int16_t OTRGetRectDimensionFromRightEdge(float v);

// The ocarina recognition state (code_800EC960.c), owned by the audio thread and only touched from it.
extern HOST_DATA u16 sOcarinaAvailSongs;
extern HOST_DATA u8 sOcarinaSongNoteStartIdx;
extern HOST_DATA u8 sOcarinaSongCnt;
extern HOST_DATA u8 D_80131878;
extern HOST_DATA u8 sOcarinaInpEnabled;
extern HOST_DATA u32 D_80130F3C;
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

// Search range, and the only defence against rumble that costs nothing: fans and
// traffic live below a hummed voice, and nothing above the whistle range is a note.
constexpr int kTauMin = kSampleRate / 1000;
constexpr int kTauMax = kSampleRate / 80;

constexpr float kYinThreshold = 0.15f;
constexpr int kMedianSize = 5;
constexpr int kMaxPhraseNotes = 8;

// A note ends after this much silence. A longer silence drops the rolling note
// window, so an abandoned phrase cannot bleed into the next one.
constexpr int kReleaseHops = 5;
constexpr int kReanchorHops = 90;

// Silence this long after the last note means the player stopped, and the
// phrase is judged as it stands: the runner-up margin is waived and the error
// budget loosens, but nonsense still plays nothing.
constexpr int kPhraseEndHops = 40;
constexpr float kPhraseEndMaxErrorCents = 180.0f;

// "da-da" on one pitch is only two notes if the level dips and recovers.
constexpr float kDipRatio = 0.45f;
constexpr float kRecoverRatio = 0.70f;

// Sustained deviation from the note being held, in cents, before it counts as a
// slide into a new note. The scale's tightest interval is 200 cents (A4 to B4).
constexpr float kJumpCents = 90.0f;
constexpr int kJumpHops = 3;

// A note only commits after this many stable hops (~32 ms), so a click or a
// consonant cannot latch a button.
constexpr int kAttackHops = 3;

// A committed note only enters the phrase once held ~100 ms past the attack:
// a glide or a hiccup between two real notes never reaches the matcher.
constexpr int kMinNoteHops = 9;

// Net movement since the hold began that means the pitch is still sliding, so
// the hold restarts from where it is now. Vibrato swings but does not travel.
constexpr float kGlideCents = 60.0f;

// The gate rides a tracked background level instead of a fixed threshold, so a
// noisy room raises the bar rather than producing notes. The floor follows a
// drop fast and a rise slowly, and never rises while a note is held, or a
// sustained note would drag the floor up behind itself and cut its own tail.
constexpr float kFloorFallRate = 0.25f;
constexpr float kFloorRiseRate = 0.002f;
constexpr float kGateCloseRatio = 0.5f;
constexpr float kNoiseMargin = 4.0f;     // 12 dB over the tracked floor
constexpr float kAbsoluteGate = 0.0056f; // -45 dBFS, the floor under the floor
constexpr float kClarityMin = 0.55f;

// Changes smaller than this are the same relative note. Every real adjacent
// ocarina pitch is at least 200 cents apart.
constexpr float kContourDeadbandCents = 90.0f;

// RMS interval error a phrase may carry and still count as the song, and how far
// clear of the runner-up it must land. Two songs within the margin are a coin
// flip, so nothing fires until the phrase ends. Epona and Healing, the closest
// pair, sit 82 cents apart when sung perfectly.
constexpr float kMaxPhraseErrorCents = 120.0f;
constexpr float kMatchMarginCents = 40.0f;
constexpr float kNoMatchError = 1.0e9f;

// Semitones above D4 of each ocarina note index (D4 F4 A4 B4 D5).
constexpr int kScaleDegrees[5] = { 0, 3, 7, 9, 12 };

constexpr float kHopSeconds = static_cast<float>(kHopSize) / kSampleRate;
constexpr int kMeterSegmentMax = 24;
constexpr float kMeterWindowSeconds = 6.0f;
constexpr int kMeterSemitoneRange = 48;
constexpr int kMeterBarHalfHeight = 1;

// The audio thread polls at its own rate: a few missed game frames are not the ocarina put away.
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

// Piano roll of the recent notes.
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

// Which songs the game will accept right now, mirrored off the audio thread so
// the matcher can honour the same gate the native staff check uses.
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

// The scarecrow songs and the memory game are not hummable.
constexpr int kRecognisedSongs[] = {
    OCARINA_SONG_MINUET,  OCARINA_SONG_BOLERO, OCARINA_SONG_SERENADE, OCARINA_SONG_REQUIEM,
    OCARINA_SONG_NOCTURNE, OCARINA_SONG_PRELUDE, OCARINA_SONG_SARIAS, OCARINA_SONG_EPONAS,
    OCARINA_SONG_LULLABY, OCARINA_SONG_SUNS,   OCARINA_SONG_TIME,     OCARINA_SONG_STORMS,
};

float ComputeRms(const float* window, int count) {
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += window[i] * window[i];
    }
    return sqrtf(sum / count);
}

// d(tau) = e(0) + e(tau) - 2 r(tau), evaluated directly: at this window size the
// O(W * tau) loop runs in well under a millisecond, cheaper than owning an FFT.
PitchEstimate EstimatePitch(const float* window) {
    for (int tau = 1; tau <= kTauMax; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < kIntegrationSize; j++) {
            float delta = window[j] - window[j + tau];
            sum += delta * delta;
        }
        sDiff[tau] = sum;
    }

    // Cumulative mean normalisation: dividing by the running mean of d makes the
    // curve dimensionless (so kYinThreshold is absolute) and removes the bias
    // that otherwise picks tau too small.
    float runningSum = 0.0f;
    sCmndf[0] = 1.0f;
    for (int tau = 1; tau <= kTauMax; tau++) {
        runningSum += sDiff[tau];
        sCmndf[tau] = (runningSum > 0.0f) ? (sDiff[tau] * tau / runningSum) : 1.0f;
    }

    // First local minimum below the threshold, not the global one: the true
    // period dips below it and so do its multiples, so taking the first dip is
    // what keeps the fundamental instead of an octave-down answer.
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

// Once per hop, silence included, so the roll keeps scrolling and the held
// note's bar keeps stretching.
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
    if ((songIndex < sFirstSongIndex.load(std::memory_order_relaxed)) ||
        (songIndex >= sLastSongIndex.load(std::memory_order_relaxed))) {
        return false;
    }
    return (sAvailableSongFlags.load(std::memory_order_relaxed) & (1u << songIndex)) != 0;
}

// RMS distance in cents between what was heard and the song, after removing the
// transposition that best fits. Subtracting the mean residual IS that best fit,
// and it is what makes the score independent of the key the player hums in.
float PhraseError(const float* cents, int count, int songIndex) {
    const OcarinaSongInfo& song = gOcarinaSongNotes[songIndex];
    if ((count < 2) || (song.len != count)) {
        return kNoMatchError;
    }

    float residual[kMaxPhraseNotes];
    float mean = 0.0f;
    for (int i = 0; i < count; i++) {
        int degree = song.notesIdx[i];
        if ((degree < OCARINA_NOTE_D4) || (degree > OCARINA_NOTE_D5)) {
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

// Vibrato or a breath can split one held note in two. Merging adjacent notes
// closer than a real interval recovers that, but it also destroys the genuine
// repeats in Serenade and Nocturne, so this is scored as an alternative
// hypothesis rather than applied to the phrase.
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

// Scores the song against the newest notes only. Two hypotheses: the notes as
// segmented, and a slightly wider window with split notes merged, which recovers
// a note the vibrato guard cut in half.
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
    for (int songIndex : kRecognisedSongs) {
        if (!IsSongAccepted(songIndex)) {
            continue;
        }
        float error = SuffixError(songIndex, gOcarinaSongNotes[songIndex].len);
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

// Run after every accepted note rather than after a silence, so recognition
// lands as the phrase ends. It also makes a spurious note survivable: it only
// shifts the window instead of ruining a whole fixed-length phrase.
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

// A rolling window of the last notes, never cleared on overflow: the oldest note
// is dropped so the newest phrase always stays matchable.
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

// The key is anchored by the first note held long enough, never by a candidate
// or a blip: neither may decide the whole phrase's key.
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

// Holds the candidate until it survives kAttackHops without wandering, then
// commits it as a real note.
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

    // Hysteresis: a note already running only has to clear half the opening
    // level, so a wavering hum is not chopped into pieces at the threshold.
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

    // Follow slow drift within the note so a long vowel does not accumulate into
    // a false slide, but keep the published button pinned to the committed pitch.
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

// The game asks the player the first time and shows a microphone icon for as long as this records.
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

// A chunk at a time, each analysed before the next lands, so a long frame cannot overrun the ring.
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

// Main thread owns the device and the CVar; the audio-thread hook only trades atomics.
// A stale request means the ocarina was put away, so the mic is freed.
void TickMicOcarina() {
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

// Fires on the audio thread every tick the ocarina takes input: the thread that owns the globals below.
void ListenOnAudioThread(uint8_t note, float modulator, int8_t bend) {
    sRequested.store(true, std::memory_order_relaxed);
    sAvailableSongFlags.store(sOcarinaAvailSongs, std::memory_order_relaxed);
    sFirstSongIndex.store(sOcarinaSongNoteStartIdx, std::memory_order_relaxed);
    sLastSongIndex.store(sOcarinaSongCnt, std::memory_order_relaxed);

    uint32_t sequence = sMatchSequence.load(std::memory_order_acquire);
    if (sequence == sLastSeenMatch) {
        return;
    }
    sLastSeenMatch = sequence;

    int song = sMatchedSong.load(std::memory_order_relaxed);
    if ((song < sOcarinaSongNoteStartIdx) || (song >= sOcarinaSongCnt) || !(sOcarinaAvailSongs & (1u << song))) {
        return;
    }

    // The three writes the native staff check makes on a match: the game runs the fanfare and the effect.
    D_80131878 = static_cast<u8>(song + 1);
    sOcarinaInpEnabled = 0;
    D_80130F3C = 0;
}

float SemisToY(float originY, float semis, float pixelsPerSemi) {
    return originY - (semis * pixelsPerSemi);
}

// A texture rectangle rather than a fill: the fill cycle carries one bit of alpha,
// and these bars are translucent over live gameplay.
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

// Bars are born at the left edge and stream right as they age, so a held note
// draws itself left to right. White until it counts, green once it does.
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
