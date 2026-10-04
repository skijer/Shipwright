#include "test_require.h"
#include <vector>

#include "audio_sequence_stubs/global.h"
#include "soh/Enhancements/audio/HyruleFieldNightMusicInternal.h"

static uint16_t sReplacementSequence = 0x35;
static uint16_t sReplacementInput = 0;
static int sReplacementCalls = 0;
static uint16_t sPlayedSequence = 0xFFFF;
static std::vector<uint16_t> sPlayedSequences;
static u32 sLastStartCommand = 0;
static s32 sLastStartFade = 0;

extern "C" {
AudioContext gAudioContext = {};
u8 gSfxChannelLayout = 0;
s8 D_80133390[] = "SEQ H";
s8 D_80133398[] = "    L";

void Audio_QueueCmdS32(u32 command, s32 fade) {
    if ((command >> 24) == 0x82 || (command >> 24) == 0x85) {
        sPlayedSequence = command & 0xFFFF;
        sPlayedSequences.push_back(sPlayedSequence);
        sLastStartCommand = command;
        sLastStartFade = fade;
    }
}
void Audio_QueueCmdF32(u32, f32) {
}
void Audio_QueueCmdS8(u32, s8) {
}
void Audio_QueueCmdU16(u32, u16) {
}
void AudioDebug_ScrPrt(const s8*, u16) {
}
void func_800E5F88(u8) {
}
void func_800F71BC(u8) {
}
s32 func_800E5E20(u32*) {
    return 0;
}
s32 func_800E5EDC(void) {
    return 1;
}
void func_800F7170(void) {
}
}

static bool IsKnownSequence(uint16_t sequence) {
    return sequence == 0x02 || sequence == 0x21 || sequence == 0x35 || sequence == 0x36 || sequence == 0x8135;
}

static uint16_t GetReplacementSequence(uint16_t sequence) {
    sReplacementInput = sequence;
    ++sReplacementCalls;
    return sReplacementSequence;
}

static void ResetReplacementStub(uint16_t replacement) {
    sReplacementSequence = replacement;
    sReplacementInput = 0;
    sReplacementCalls = 0;
    HyruleFieldNightMusic_ClearSequence();
}

static uint16_t GetChainedReplacementSequence(uint16_t sequence) {
    sReplacementInput = sequence;
    ++sReplacementCalls;
    if (sequence == 0x02 || sequence == 0x21) {
        return 0x35;
    }
    if (sequence == 0x35) {
        return 0x36;
    }
    return sequence;
}

static uint16_t GetWideReplacementSequence(uint16_t sequence) {
    sReplacementInput = sequence;
    ++sReplacementCalls;
    return sequence == 0x21 ? 0x8135 : sequence;
}

extern "C" u16 AudioEditor_GetReplacementSeq(u16 sequence) {
    return GetChainedReplacementSequence(sequence);
}

static void ResetPlaybackBoundary() {
    Audio_ProcessSeqCmds();
    gAudioContext = {};
    gAudioContext.audioBufferParameters.updatesPerFrame = 4;
    sPlayedSequence = 0xFFFF;
    sPlayedSequences.clear();
}

static HyruleFieldNightMusicState BaseState() {
    return {
        .enabled = true,
        .inHyruleField = true,
        .isNight = true,
        .ownsNightBgm = false,
        .nightBgmPlaying = false,
        .fanfarePlaying = false,
        .explicitAudioOverride = false,
    };
}

int main() {
    // Runtime diagnostics are transition-bounded: the first Hyrule Field
    // snapshot and meaningful state changes log, identical frames do not.
    HyruleFieldNightMusicDiagnosticSnapshot diagnostic = {
        .state = BaseState(),
        .decision = HyruleFieldNightMusicDecision::StartNight,
        .mainSequence = 0x02,
        .subSequence = 0xFFFF,
        .fanfareSequence = 0xFFFF,
        .nightPlaybackSequence = 0xFFFF,
    };
    REQUIRE(HyruleFieldNightMusic_ShouldLogDiagnostic(nullptr, diagnostic));
    REQUIRE(!HyruleFieldNightMusic_ShouldLogDiagnostic(&diagnostic, diagnostic));
    auto changedDiagnostic = diagnostic;
    changedDiagnostic.state.isNight = false;
    changedDiagnostic.decision = HyruleFieldNightMusicDecision::NoChange;
    REQUIRE(HyruleFieldNightMusic_ShouldLogDiagnostic(&diagnostic, changedDiagnostic));
    auto outsideDiagnostic = changedDiagnostic;
    outsideDiagnostic.state.inHyruleField = false;
    REQUIRE(HyruleFieldNightMusic_ShouldLogDiagnostic(&changedDiagnostic, outsideDiagnostic));
    REQUIRE(!HyruleFieldNightMusic_ShouldLogDiagnostic(&outsideDiagnostic, outsideDiagnostic));
    auto unrelatedSceneAudio = outsideDiagnostic;
    unrelatedSceneAudio.mainSequence = 0x35;
    REQUIRE(!HyruleFieldNightMusic_ShouldLogDiagnostic(&outsideDiagnostic, unrelatedSceneAudio));

    // Sequence arguments live above the low ID byte. Nature ambience with
    // those flags is still Hyrule Field's native lifecycle, not an override.
    REQUIRE(HyruleFieldNightMusic_IsFieldLifecycleSequence(NA_BGM_NATURE_AMBIENCE | 0x0800, NA_BGM_FIELD_LOGIC,
                                                           NA_BGM_NATURE_AMBIENCE, NA_BGM_DISABLED));
    // Hyrule Field owns MAIN. SUB belongs to native enemy/proximity overlays,
    // whose lifecycle repeatedly stops that player when an encounter ends.
    REQUIRE(HyruleFieldNightMusic_GetPlaybackPlayer() == 0);
    REQUIRE(HyruleFieldNightMusic_IsNightSequencePlaying(true, 0x8135, 0x35));
    REQUIRE(!HyruleFieldNightMusic_IsNightSequencePlaying(false, 0x8135, 0x35));
    REQUIRE(!HyruleFieldNightMusic_IsNightSequencePlaying(true, NA_BGM_FIELD_LOGIC, 0x35));

    auto state = BaseState();
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);

    state.ownsNightBgm = true;
    state.nightBgmPlaying = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.nightBgmPlaying = false;
    state.fanfarePlaying = false;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::RestoreNight);

    state.isNight = false;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNightRestoreDay);

    // A second complete cycle must make the same transitions instead of
    // leaving the main player silent after the first dawn.
    state = BaseState();
    state.ownsNightBgm = false;
    state.nightBgmPlaying = false;
    state.isNight = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StartNight);
    state.ownsNightBgm = true;
    state.nightBgmPlaying = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);
    state.isNight = false;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNightRestoreDay);

    state = BaseState();
    state.ownsNightBgm = true;
    state.fanfarePlaying = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state = BaseState();
    state.inHyruleField = false;
    state.ownsNightBgm = true;
    state.explicitAudioOverride = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::StopNightRestoreDay);

    state = BaseState();
    state.enabled = false;
    state.ownsNightBgm = true;
    state.fanfarePlaying = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state = BaseState();
    state.explicitAudioOverride = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.ownsNightBgm = true;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    state.isNight = false;
    REQUIRE(HyruleFieldNightMusic_Select(state) == HyruleFieldNightMusicDecision::NoChange);

    REQUIRE(HyruleFieldNightMusic_ValidateSequence(0x02, true, 0x21) == 0x02);
    REQUIRE(HyruleFieldNightMusic_ValidateSequence(0x7FFF, false, 0x21) == 0x21);
    REQUIRE(HyruleFieldNightMusic_ResolveSequence(0x21, 0x35) == 0x35);

    // A valid configured sequence enters the Audio Editor mapping exactly once,
    // and its valid replacement becomes the cached playback sequence.
    ResetReplacementStub(0x35);
    REQUIRE(HyruleFieldNightMusic_StartSequence(0x21, 0x21, IsKnownSequence, GetReplacementSequence) == 0x35);
    REQUIRE(sReplacementInput == 0x21);
    REQUIRE(sReplacementCalls == 1);

    // Invalid configuration falls back before lookup, so the Audio Editor maps
    // the known default rather than receiving an invalid sequence ID.
    ResetReplacementStub(0x35);
    REQUIRE(HyruleFieldNightMusic_StartSequence(0x7FFF, 0x21, IsKnownSequence, GetReplacementSequence) == 0x35);
    REQUIRE(sReplacementInput == 0x21);
    REQUIRE(sReplacementCalls == 1);

    // A defensive validation failure from the replacement lookup keeps the
    // already-validated configured/default sequence.
    ResetReplacementStub(0x7FFF);
    REQUIRE(HyruleFieldNightMusic_StartSequence(0x21, 0x21, IsKnownSequence, GetReplacementSequence) == 0x21);

    // Fanfare recovery restores the cached result. A changed mapping must not
    // be consulted until a later StartNight lifecycle.
    ResetReplacementStub(0x35);
    REQUIRE(HyruleFieldNightMusic_StartSequence(0x21, 0x21, IsKnownSequence, GetReplacementSequence) == 0x35);
    sReplacementSequence = 0x36;
    REQUIRE(HyruleFieldNightMusic_RestoreSequence() == 0x35);
    REQUIRE(HyruleFieldNightMusic_RestoreSequence() == 0x35);
    REQUIRE(sReplacementCalls == 1);

    // Dawn and feature-disable while still in Hyrule Field restore Field Logic
    // directly. Scene exit must leave the destination MAIN sequence untouched.
    state = BaseState();
    state.ownsNightBgm = true;
    state.isNight = false;
    REQUIRE(HyruleFieldNightMusic_ShouldRestoreDaySequence(state));
    state.inHyruleField = false;
    REQUIRE(!HyruleFieldNightMusic_ShouldRestoreDaySequence(state));
    state.inHyruleField = true;
    state.enabled = false;
    REQUIRE(HyruleFieldNightMusic_ShouldRestoreDaySequence(state));
    state.explicitAudioOverride = true;
    REQUIRE(!HyruleFieldNightMusic_ShouldRestoreDaySequence(state));

    // The resolved night ID crosses the command boundary without another
    // Audio Editor lookup, so a chained 0x21 -> 0x35 -> 0x36 mapping plays and
    // restores the cached 0x35 rather than advancing to 0x36.
    ResetReplacementStub(0x35);
    ResetPlaybackBoundary();
    const uint16_t chainedNight =
        HyruleFieldNightMusic_StartSequence(0x21, 0x21, IsKnownSequence, GetChainedReplacementSequence);
    Audio_QueueResolvedSeqCmd(HyruleFieldNightMusic_GetPlaybackPlayer(), chainedNight, 0x1E);
    Audio_ProcessSeqCmds();
    REQUIRE(sPlayedSequence == 0x35);
    REQUIRE(sReplacementCalls == 1);
    Audio_QueueResolvedSeqCmd(HyruleFieldNightMusic_GetPlaybackPlayer(), HyruleFieldNightMusic_RestoreSequence(), 0x1E);
    Audio_ProcessSeqCmds();
    REQUIRE(sPlayedSequence == 0x35);
    REQUIRE(sReplacementCalls == 1);

    // Dawn playback uses the same no-remap boundary on MAIN.
    ResetPlaybackBoundary();
    const uint16_t chainedDay = GetChainedReplacementSequence(0x02);
    Audio_QueueResolvedSeqCmd(0, chainedDay, 0x1E);
    Audio_ProcessSeqCmds();
    REQUIRE(sPlayedSequence == 0x35);
    REQUIRE(sReplacementCalls == 2);

    // A full-width replacement is carried in queue-slot metadata while the ordinary
    // command retains zero sequence arguments; 0x8135 must not truncate to
    // 0x35 or be suppressed by a high argument byte.
    ResetReplacementStub(0x8135);
    ResetPlaybackBoundary();
    const uint16_t wideNight =
        HyruleFieldNightMusic_StartSequence(0x21, 0x21, IsKnownSequence, GetWideReplacementSequence);
    Audio_QueueResolvedSeqCmd(HyruleFieldNightMusic_GetPlaybackPlayer(), wideNight, 0x1E);
    Audio_ProcessSeqCmds();
    REQUIRE(sPlayedSequence == 0x8135);
    REQUIRE(sReplacementCalls == 1);

    // An earlier same-player start must not steal the later resolved entry's
    // cached ID/bypass. Test both low and full-width IDs with real delayed
    // production queueing, decoding and low-level start-command emission.
    for (uint16_t expected : { 0x35, 0x8135 }) {
        ResetReplacementStub(expected);
        ResetPlaybackBoundary();
        const uint16_t cached =
            HyruleFieldNightMusic_StartSequence(0x21, 0x21, IsKnownSequence, GetReplacementSequence);
        Audio_QueueSeqCmd(0x031E0040);
        Audio_QueueResolvedSeqCmd(3, cached, 0x1E);
        REQUIRE(sPlayedSequences.empty());
        Audio_ProcessSeqCmds();
        REQUIRE(sPlayedSequences.size() == 2);
        REQUIRE(sPlayedSequences[0] == 0x40);
        REQUIRE(sPlayedSequences[1] == expected);
        REQUIRE(sReplacementCalls == 2); // One initial lookup plus the ordinary start only.
        REQUIRE(sLastStartCommand == (0x82030000U | expected));
        REQUIRE(sLastStartFade == 0xF0);

        // Cached and mapped starts use the same atomic path under the same
        // delay on either sequence player.
        sPlayedSequences.clear();
        Audio_QueueSeqCmd(0x031E0040);
        Audio_QueueResolvedSeqCmd(3, HyruleFieldNightMusic_RestoreSequence(), 0x1E);
        Audio_ProcessSeqCmds();
        REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x40, expected }));
        REQUIRE(sReplacementCalls == 3);

        sPlayedSequences.clear();
        const uint16_t day = GetReplacementSequence(0x02);
        Audio_QueueSeqCmd(0x001E0040);
        Audio_QueueResolvedSeqCmd(0, day, 0x1E);
        Audio_ProcessSeqCmds();
        REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x40, expected }));
        REQUIRE(sReplacementCalls == 5);
        REQUIRE(sLastStartCommand == (0x82000000U | expected));
    }

    // Resolved entries neither consume nor overwrite existing MM state, even
    // if the ordinary MM command is queued later on the same player.
    for (bool primeFirst : { false, true }) {
        ResetPlaybackBoundary();
        sReplacementCalls = 0;
        if (primeFirst) {
            Audio_PrimeMmSideChannel(3, 0x8040);
        }
        Audio_QueueResolvedSeqCmd(3, 0x8135, 0x1E);
        if (!primeFirst) {
            Audio_PrimeMmSideChannel(3, 0x8040);
        }
        Audio_QueueSeqCmd(0x031E0040);
        Audio_ProcessSeqCmds();
        REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x8135, 0x8040 }));
        REQUIRE(sReplacementCalls == 0);
    }

    ResetPlaybackBoundary();
    Audio_QueueResolvedSeqCmd(3, 0x35, 0x1E);
    Audio_QueueResolvedSeqCmd(3, 0x8135, 0x1E);
    Audio_ProcessSeqCmds();
    REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x35, 0x8135 }));

    // Dropped starts (fonts pending or starts disabled) cannot leave a bypass
    // behind for the next ordinary start.
    for (bool waitingForFonts : { false, true }) {
        ResetPlaybackBoundary();
        D_80133408 = !waitingForFonts;
        gActiveSeqs[3].isWaitingForFonts = waitingForFonts;
        Audio_QueueResolvedSeqCmd(3, 0x8135, 0x1E);
        Audio_ProcessSeqCmds();
        REQUIRE(sPlayedSequences.empty());
        D_80133408 = 0;
        gActiveSeqs[3].isWaitingForFonts = 0;
        Audio_QueueSeqCmd(0x031E0040);
        Audio_ProcessSeqCmds();
        REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x40 }));
    }

    // Reusing every ring slot must clear metadata for ordinary/preview writes.
    for (int i = 0; i < 300; ++i) {
        ResetPlaybackBoundary();
        Audio_QueueResolvedSeqCmd(0, 0x8135, 0x1E);
        Audio_ProcessSeqCmds();
        Audio_QueueSeqCmd(0x001E0021);
        Audio_ProcessSeqCmds();
        Audio_QueuePreviewSeqCmd(0x8040);
        Audio_ProcessSeqCmds();
        REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x8135, 0x35, 0x8040 }));
    }

    // The legacy ring drops a full 256-entry batch when write catches read.
    // Even without consuming those entries, ordinary and preview reuse must
    // replace their metadata rather than inherit a stale resolved ID.
    for (bool preview : { false, true }) {
        ResetPlaybackBoundary();
        for (int i = 0; i < 256; ++i) {
            Audio_QueueResolvedSeqCmd(0, 0x8135, 0x1E);
        }
        Audio_ProcessSeqCmds();
        sPlayedSequences.clear();
        if (preview) {
            Audio_QueuePreviewSeqCmd(0x8040);
        } else {
            Audio_QueueSeqCmd(0x001E0021);
        }
        Audio_ProcessSeqCmds();
        REQUIRE((sPlayedSequences == std::vector<uint16_t>{ static_cast<uint16_t>(preview ? 0x8040 : 0x35) }));
    }

    // Active-sequence reset does not flush the command ring; preserve the
    // pending entry's metadata and do not leak it into the following start.
    ResetPlaybackBoundary();
    Audio_QueueResolvedSeqCmd(3, 0x8135, 0x1E);
    Audio_ResetActiveSequences();
    Audio_QueueSeqCmd(0x031E0040);
    Audio_ProcessSeqCmds();
    REQUIRE((sPlayedSequences == std::vector<uint16_t>{ 0x8135, 0x40 }));
    return 0;
}
